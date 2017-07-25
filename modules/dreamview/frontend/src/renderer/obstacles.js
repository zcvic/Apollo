import * as THREE from "three";

import STORE from "store";
import PARAMETERS from "store/config/parameters.yml";
import { drawSegmentsFromPoints, drawBox, drawArrow } from "utils/draw";
const _ = require('lodash');

const DEFAULT_HEIGHT = 1.5;
const DEFAULT_COLOR = 0xad21fe;
const ObstacleColorMapping = {
        PEDESTRIAN: 0xffea00,
        BICYCLE: 0x0febff,
        VEHICLE: 0x48dc6b,
        VIRTUAL: 0x800000
};
const LINE_THICKNESS = 1.5;

function copyProperty(toObj, fromObj) {
    for (const property in fromObj) {
        if (fromObj.hasOwnProperty(property)) {
            toObj[property] = fromObj[property];
        }
    }
}

function hideArrayObjects(objects, startIdx = 0) {
    if (objects.constructor === Array && objects.length > 0) {
        for (;startIdx < objects.length; startIdx++) {
            objects[startIdx].visible = false;
        }
    }
}

export default class PerceptionObstacles {
    constructor() {
        this.arrows = []; // for indication of direction of moving obstacles
        this.cubes = []; // for obstacles with only length/width/height
        this.extrusionFaces = []; // for obstacles with polygon points
    }

    update(world, coordinates, rotation, scene) {
        if (!STORE.options.showObstacles) {
            hideArrayObjects(this.arrows);
            hideArrayObjects(this.cubes);
            hideArrayObjects(this.extrusionFaces);
            return;
        }

        const objects = world.object;
        if (_.isEmpty(objects)) {
            return;
        }

        let arrowIdx = 0;
        let cubeIdx = 0;
        let extrusionFaceIdx = 0;
        for (let i = 0; i < objects.length; i++) {
            const obstacle = objects[i];
            if (!obstacle.positionX || !obstacle.positionY) {
                continue;
            }
            const position = coordinates.applyOffset(
                    new THREE.Vector3(obstacle.positionX,
                                      obstacle.positionY,
                                      (obstacle.height || DEFAULT_HEIGHT) / 2));
            const color = ObstacleColorMapping[obstacle.type] || DEFAULT_COLOR;
            const arrowMesh = this.updateArrow(position, obstacle.heading,
                    color, arrowIdx++, scene);
            const polygon = obstacle.polygonPoint;
            if (polygon.length > 0) {
                const scale = this.updatePolygon(polygon, obstacle.height, color, coordinates,
                        extrusionFaceIdx, scene);
                extrusionFaceIdx += polygon.length;
                arrowMesh.scale.set(scale, scale, scale);
            } else if (obstacle.length && obstacle.width && obstacle.height) {
                this.updateCube(obstacle.length, obstacle.width, obstacle.height, position,
                        obstacle.heading, color, cubeIdx++);
                arrowMesh.scale.set(obstacle.width, obstacle.length, obstacle.height);
            }
            arrowMesh.visible = (obstacle.type && obstacle.type !== 'UNKNOWN_UNMOVABLE');
            this.updateId(obstacle.id, new THREE.Vector3(position.x, position.y, obstacle.height),
                    rotation, scene);
        }
        hideArrayObjects(this.arrows, arrowIdx);
        hideArrayObjects(this.cubes, cubeIdx);
        hideArrayObjects(this.extrusionFaces, extrusionFaceIdx);
    }

    updateArrow(position, heading, color, arrowIdx, scene) {
        const arrowMesh = this.getArrow(arrowIdx, scene);
        copyProperty(arrowMesh.position, position);
        arrowMesh.material.color.setHex(color);
        arrowMesh.rotation.set(0, 0, -(Math.PI / 2 - heading));
        return arrowMesh;
    }

    updatePolygon(points, height, color, coordinates, extrusionFaceIdx, scene) {
        let edgeDistanceSum = 0;
        for (let i = 0; i < points.length; i++) {
            const faceMesh = this.getFace(extrusionFaceIdx + i, scene);
            const next = (i === points.length - 1) ? 0 : i + 1;
            const v = new THREE.Vector3(points[i].x, points[i].y, points[i].z);
            const vNext = new THREE.Vector3(points[next].x, points[next].y, points[next].z);
            const facePosition = coordinates.applyOffset(
                    new THREE.Vector2((v.x + vNext.x) / 2.0, (v.y + vNext.y) / 2.0));

            if (facePosition === null) {
                continue;
            }
            faceMesh.position.set(facePosition.x, facePosition.y, 0);
            const edgeDistance = v.distanceTo(vNext);
            edgeDistanceSum += edgeDistance;
            faceMesh.scale.set(edgeDistance, 1, height);
            faceMesh.material.color.setHex(color);
            faceMesh.rotation.set(0, 0, Math.atan2(vNext.y - v.y, vNext.x - v.x));
            faceMesh.visible = true;
        }
        return 1.0 * edgeDistanceSum / points.length;
    }

    updateCube(length, width, height, position, heading, color, cubeIdx) {
        const cubeMesh = this.getCube(cubeIdx, scene);
        cubeMesh.position.set(position.x, position.y, position.z);
        cubeMesh.scale.set(length, width, height);
        cubeMesh.material.color.setHex(color);
        cubeMesh.rotation.set(0, 0, heading);
        cubeMesh.visible = true;
    }

    updateId(id, position, rotation, scene) {
        // TODO (caoyu05): display obstacle id with text
    }

    getArrow(index, scene) {
        if (index < this.arrows.length) {
            return this.arrows[index];
        }
        const arrowMesh = drawArrow(1.5, LINE_THICKNESS, 0.5, 0.5, DEFAULT_COLOR);
        arrowMesh.rotation.set(0, 0, -Math.PI / 2);
        arrowMesh.visible = false;
        this.arrows.push(arrowMesh);
        scene.add(arrowMesh);
        return arrowMesh;
    }

    getFace(index, scene) {
        if (index < this.extrusionFaces.length) {
            return this.extrusionFaces[index];
        }
        const extrusionFace = drawSegmentsFromPoints([
            new THREE.Vector3(-0.5, 0, 0),
            new THREE.Vector3(0.5, 0, 0),
            new THREE.Vector3(0.5, 0, 1),
            new THREE.Vector3(-0.5, 0, 1)
        ], DEFAULT_COLOR, LINE_THICKNESS);
        extrusionFace.visible = false;
        this.extrusionFaces.push(extrusionFace);
        scene.add(extrusionFace);
        return extrusionFace;
    }

    getCube(index, scene) {
        if (index < this.cubes.length) {
            return this.cubes[index];
        }
        const cubeMesh = drawBox(new THREE.Vector3(1, 1, 1), DEFAULT_COLOR, LINE_THICKNESS);
        cubeMesh.visible = false;
        this.cubes.push(cubeMesh);
        scene.add(cubeMesh);
        return cubeMesh;
    }
}
