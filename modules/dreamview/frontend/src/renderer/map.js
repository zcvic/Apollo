import * as THREE from "three";
import WS from "store/websocket.js";

import { drawSegmentsFromPoints,
         drawDashedLineFromPoints,
         drawShapeFromPoints } from "utils/draw";

import trafficLightMaterial from "assets/models/traffic_light.mtl";
import trafficLightObject from "assets/models/traffic_light.obj";
import { loadObject } from "utils/models";

const colorMapping = {
    YELLOW: 0XDAA520,
    WHITE: 0xcccccc,
    CORAL: 0xFF7F50,
    RED: 0xff6666,
    GREEN: 0x006400,
    PURE_WHITE: 0xFFFFFF,
    DEFAULT: 0xC0C0C0
};

// The result will be the all the elements in current but
// not in data.
function diffMapElements(elements, data) {
    const result = {};

    for (const kind in elements) {
        result[kind] = [];
        const newOnes = elements[kind];
        const oldOnes = data[kind];

        for (let i = 0; i < newOnes.length; ++i) {
            const found = oldOnes ? oldOnes.find(x => {
                return x.id.id === newOnes[i];
            }) : false;

            if (!found) {
                result[kind].push(newOnes[i]);
            }
        }
    }

    return result;
}

function addLaneMesh(laneType, points) {
    switch (laneType) {
        case "DOTTED_YELLOW":
            return drawDashedLineFromPoints(
                points, colorMapping.YELLOW, 4, 3, 3, 1, false);
        case "DOTTED_WHITE":
            return drawDashedLineFromPoints(
                points, colorMapping.WHITE, 4, 3, 3, 1, false);
        case "SOLID_YELLOW":
            return drawSegmentsFromPoints(
                points, colorMapping.YELLOW, 3, 1, false);
        case "SOLID_WHITE":
            return drawSegmentsFromPoints(
                points, colorMapping.WHITE, 3, 1, false);
        case "DOUBLE_YELLOW":
            const left = drawSegmentsFromPoints(
                points, colorMapping.YELLOW, 2, 1, false);
            const right = drawSegmentsFromPoints(
                points.map(point =>
                    new THREE.Vector3(point.x + 0.3, point.y + 0.3, point.z)),
                colorMapping.YELLOW, 3, 1, false);
            left.add(right);
            return left;
        case "CURB":
            return drawSegmentsFromPoints(
                points, colorMapping.CORAL, 3, 1, false);
        default:
            return drawSegmentsFromPoints(
                points, colorMapping.DEFAULT, 3, 1, false);
    }
}

function addLane(lane, coordinates, scene) {
    const drewObjects = [];

    const centralLine = lane.centralCurve.segment;
    centralLine.forEach(segment => {
        const points = segment.lineSegment.point.map(point =>
            coordinates.applyOffset(point)
        );
        const centerLine = drawSegmentsFromPoints(
            points, colorMapping.GREEN, 1, 1, false);
        scene.add(centerLine);
        drewObjects.push(centerLine);
    });

    const rightLaneType = lane.rightBoundary.type;
    lane.rightBoundary.curve.segment.forEach((segment, index) => {
        const points = segment.lineSegment.point.map(point =>
            coordinates.applyOffset(point)
        );
        const boundary = addLaneMesh(rightLaneType, points);
        scene.add(boundary);
        drewObjects.push(boundary);
    });

    const leftLaneType = lane.leftBoundary.type;
    lane.leftBoundary.curve.segment.forEach((segment, index) => {
        const points = segment.lineSegment.point.map(point =>
            coordinates.applyOffset(point)
        );
        const boundary = addLaneMesh(leftLaneType, points);
        scene.add(boundary);
        drewObjects.push(boundary);
    });

    return drewObjects;
}

function addCrossWalk(crosswalk, coordinates, scene) {
    const drewObjects = [];

    const border = crosswalk.polygon.point.map(point =>
        coordinates.applyOffset(point)
    );
    border.push(border[0]);

    const crosswalkMaterial = new THREE.MeshBasicMaterial({
        color: colorMapping.PURE_WHITE,
        transparent: true,
        opacity: .15
    });

    const crosswalkShape = drawShapeFromPoints(border, crosswalkMaterial, false, 3, false);
    scene.add(crosswalkShape);
    drewObjects.push(crosswalkShape);

    const mesh = drawSegmentsFromPoints(
        border, colorMapping.PURE_WHITE, 2, 0, true, false, 1.0);
    scene.add(mesh);
    drewObjects.push(mesh);

    return drewObjects;
}

function extractOverlaps(overlaps) {
    // get overlaps with only one lane
    const relevantOverlaps = _.filter(overlaps, o => {
        const elementIsLane = _.map(o.object, overlapElement => {
            return overlapElement.laneOverlapInfo !== undefined;
        });
        return _.countBy(elementIsLane)[true] === 1;
    });
    for (let i = 0; i < relevantOverlaps.length; i++) {
        relevantOverlaps[i].object = _.sortBy(
            relevantOverlaps[i].object, obj => obj.laneOverlapInfo === undefined);
    }

    // construct overlap map with relevant overlaps
    const keys = _.map(relevantOverlaps, overlap => overlap.id.id);
    const values = _.map(relevantOverlaps, overlap => {
        return _.join(_.map(overlap.object, o => o.id.id), '_and_');
    });
    return _.zipObject(keys, values);
}

function getLaneHeading(lane) {
    const last2Points = _.takeRight(_.last(lane.centralCurve.segment).lineSegment.point, 2);
    let res = 0;
    if (last2Points.length === 2) {
        res = Math.atan2(last2Points[0].y - last2Points[1].y, last2Points[0].x - last2Points[1].x);
    }
    return res;
}

function getSignalPositionAndHeading(signal, overlapMap, laneHeading, coordinates) {
  let locations = _.pickBy(
      _.mapValues(signal.subsignal, obj => obj.location), v => !_.isEmpty(v));
  if (_.isEmpty(locations)) {
    locations = _.attempt(() => signal.boundary.point);
    }
    if (_.isError(locations) || locations === undefined) {
        return null;
    }
    let heading = _.attempt(() => laneHeading[_.first(
        overlapMap[_.last(signal.overlapId).id].split('_and_')
    )]);
    if (_.isError(heading) || heading === undefined) {
        heading = _.attempt(() => {
            const stopLine = signal.stopLine[0].segment[0].lineSegment.point;
            return Math.PI / 2 + Math.atan2(
                _.takeRight(stopLine).y - stopLine[0].y, _.takeRight(stopLine).x - stopLine[0].x);
        });
    }
    let res = null;
    if (!_.isError(heading) && heading !== undefined) {
        let position = new THREE.Vector3(0, 0, 0);
        position.x = _.meanBy(_.values(locations), l => l.x);
        position.y = _.meanBy(_.values(locations), l => l.y);
        position = coordinates.applyOffset(position);
        res = [position, heading];
    } else {
        console.error('Error loading traffic light. Unable to determine heading.');
    }
    return res;
}

function drawStopLine(stopLines, drewObjects, coordinates, scene) {
    stopLines.forEach(stopLine => {
        _.each(stopLine.segment, segment => {
            const points = segment.lineSegment.point.map(point =>
                coordinates.applyOffset(point)
            );
            const mesh = drawSegmentsFromPoints(points, colorMapping.PURE_WHITE, 5, 3, false);
            scene.add(mesh);
            drewObjects.push(mesh);
        });
    });
}

function addTrafficLight(signal, overlapMap, laneHeading, coordinates, scene) {
    const drewObjects = [];
    const posAndHeadings = [];
    const posAndHeading = getSignalPositionAndHeading(signal, overlapMap, laneHeading, coordinates);
    if (posAndHeading) {
        const scale = 0.006;
        loadObject(trafficLightMaterial, trafficLightObject,
            { x: scale, y: scale, z: scale},
            mesh => {
                mesh.rotation.x = Math.PI / 2;
                mesh.rotation.y = posAndHeading[1];
                mesh.position.set(posAndHeading[0].x, posAndHeading[0].y, 0);
                mesh.matrixAutoUpdate = false;
                mesh.updateMatrix();
                scene.add(mesh);
                drewObjects.push(mesh);
            });
    }
    drawStopLine(signal.stopLine, drewObjects, coordinates, scene);
    return drewObjects;
}

export default class Map {
    constructor() {
        this.hash = -1;
        this.data = {};
    }

    removeExpiredElements(elements, scene) {
        const newData = {};

        for (const kind in this.data) {
            newData[kind] = [];
            const oldDataOfThisKind = this.data[kind];
            const current = elements[kind];
            if (current) {
                for (let i = 0; i < oldDataOfThisKind.length; ++i) {
                    if (current.includes(oldDataOfThisKind[i].id.id)) {
                        newData[kind].push(oldDataOfThisKind[i]);
                    } else if (oldDataOfThisKind[i].drewObjects) {
                        oldDataOfThisKind[i].drewObjects.forEach(object => {
                            scene.remove(object);
                            if (object.geometry) {
                                object.geometry.dispose();
                            }
                            if (object.material) {
                                object.material.dispose();
                            }
                        });
                    }
                }
            }
        }
        this.data = newData;
    }

    // I do not want to do premature optimization either. Should the
    // performance become an issue, all the diff should be done at the server
    // side. This also means that the server should maintain a state of
    // (possibly) visible elements, presummably in the global store.
    appendMapData(newData, coordinates, scene) {
        const overlapMap = extractOverlaps(newData['overlap']);
        const laneHeading = {};

        for (const kind in newData) {
            if (!this.data[kind]) {
                this.data[kind] = [];
            }
            for (let i = 0; i < newData[kind].length; ++i) {
                switch (kind) {
                    case "lane":
                        const lane = newData[kind][i];
                        this.data[kind].push(Object.assign(newData[kind][i], {
                            drewObjects: addLane(lane, coordinates, scene)
                        }));
                        laneHeading[lane.id.id] = getLaneHeading(lane);
                        break;
                    case "crosswalk":
                        this.data[kind].push(Object.assign(newData[kind][i], {
                            drewObjects: addCrossWalk(
                                newData[kind][i], coordinates, scene)
                        }));
                        break;
                    case "signal":
                        this.data[kind].push(Object.assign(newData[kind][i], {
                            drewObjects: addTrafficLight(newData[kind][i],
                                overlapMap, laneHeading, coordinates, scene)
                        }));
                        break;
                    default:
                        this.data[kind].push(newData[kind][i]);
                        break;
                }
            }
        }
    }

    updateIndex(hash, elements, scene) {
        if (hash !== this.hash) {
            this.hash = hash;
            const diff = diffMapElements(elements, this.data);
            this.removeExpiredElements(elements, scene);
            WS.requestMapData(diff);
        }
    }
}
