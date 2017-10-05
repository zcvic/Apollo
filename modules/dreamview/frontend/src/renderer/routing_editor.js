import * as THREE from "three";
import "imports-loader?THREE=three!three/examples/js/controls/OrbitControls.js";

import routingPointPin from "assets/images/routing/pin.png";


import PARAMETERS from "store/config/parameters.yml";
import WS from "store/websocket.js";
import { drawImage } from "utils/draw";


export default class RoutingEditor {
    constructor() {
        this.routePoints = [];
        this.inEditingMode = false;
    }

    isInEditingMode() {
        return this.inEditingMode;
    }

    enableEditingMode(camera, adc) {
        this.inEditingMode = true;

        const pov = "Map";
        camera.fov = PARAMETERS.camera[pov].fov;
        camera.near = PARAMETERS.camera[pov].near;
        camera.far = PARAMETERS.camera[pov].far;

        camera.updateProjectionMatrix();
        WS.requestMapElementsByRadius(this.EDITING_MAP_RADIUS);
    }

    disableEditingMode(scene) {
        this.inEditingMode = false;
        this.removeAllRoutePoints(scene);
    }

    addRoutingPoint(event, camera, ground, scene) {
        const mouse = {
            x:  (event.clientX / window.innerWidth) * 2 - 1,
            y: -(event.clientY / window.innerHeight) * 2 + 1
        };

        const raycaster = new THREE.Raycaster();
        raycaster.setFromCamera(mouse, camera);

        const intersects = raycaster.intersectObject(ground.mesh);
        if (intersects.length > 0) {
                const point = drawImage(routingPointPin, 3.5, 3.5,
                                        intersects[0].point.x, intersects[0].point.y, 0.3);
                this.routePoints.push(point);
                scene.add(point);
        }
    }

    removeLastRoutingPoint(scene) {
        const lastPoint = this.routePoints.pop();
        if (lastPoint) {
            scene.remove(lastPoint);
            if (lastPoint.geometry) {
                lastPoint.geometry.dispose();
            }
            if (lastPoint.material) {
                lastPoint.material.dispose();
            }
        }
    }

    removeAllRoutePoints(scene) {
        this.routePoints.forEach((object) => {
            scene.remove(object);
            if (object.geometry) {
                object.geometry.dispose();
            }
            if (object.material) {
                object.material.dispose();
            }
        });
        this.routePoints = [];
    }

    sendRoutingRequest(scene, carOffsetPosition, coordinates) {
        if (this.routePoints.length === 0) {
            alert("Please provide at least an end point.");
            return false;
        }

        const points = this.routePoints.map((object) => {
            object.position.z = 0;
            return coordinates.applyOffset(object.position, true);
        });

        const start    = (this.routePoints.length > 1) ?
                                points[0] : coordinates.applyOffset(carOffsetPosition, true);
        const end      = (this.routePoints.length > 1) ? points[points.length-1] : points[0];
        const waypoint = (this.routePoints.length > 1) ? points.slice(1,-1) : [];
        WS.requestRoute(start, waypoint, end);

        return true;
    }

    sendDefaultRoutingRequest(carOffsetPosition, coordinates) {
        const start    = coordinates.applyOffset(carOffsetPosition, true);
        const end      = undefined;
        const waypoint = undefined;
        WS.requestRoute(start, waypoint, end, true);

        return true;
    }
}

RoutingEditor.prototype.EDITING_MAP_RADIUS = 1000.0; //meters