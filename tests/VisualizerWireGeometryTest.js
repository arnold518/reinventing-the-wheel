"use strict";

const assert = require("node:assert/strict");
const path = require("node:path");

const geometry = require(path.join(
  __dirname,
  "..",
  "visualizer-v2",
  "static",
  "wire_geometry.js",
));

function direction(start, end) {
  const dx = end.x - start.x;
  const dy = end.y - start.y;
  if (dx > 0 && dy === 0) return "right";
  if (dx < 0 && dy === 0) return "left";
  if (dy > 0 && dx === 0) return "down";
  if (dy < 0 && dx === 0) return "up";
  return "none";
}

function opposite(left, right) {
  return (left === "left" && right === "right")
    || (left === "right" && right === "left")
    || (left === "up" && right === "down")
    || (left === "down" && right === "up");
}

function assertNoImmediateReversal(points) {
  for (let index = 1; index + 1 < points.length; index += 1) {
    assert.equal(
      opposite(direction(points[index - 1], points[index]), direction(points[index], points[index + 1])),
      false,
      `path reverses immediately at point ${index}: ${JSON.stringify(points)}`,
    );
  }
}

function branch(sourcePinPoint, sourceBoundaryPoint, sinkPinPoint, sinkBoundaryPoint) {
  const route = geometry.routeBetweenHorizontalStubs({
    sourcePinPoint,
    sourceBoundaryPoint,
    sinkPinPoint,
    sinkBoundaryPoint,
    bounds: { x: 0, y: 0, w: 100, h: 100 },
  });
  return geometry.dedupePath([
    sourcePinPoint,
    sourceBoundaryPoint,
    ...route.slice(1),
    sinkPinPoint,
  ]);
}

function main() {
  const forward = branch(
    { x: 10, y: 20 }, { x: 15, y: 20 },
    { x: 90, y: 40 }, { x: 85, y: 40 },
  );
  assertNoImmediateReversal(forward);
  assert.deepEqual(forward.slice(0, 3), [
    { x: 10, y: 20 },
    { x: 15, y: 20 },
    { x: 85, y: 20 },
  ]);

  const leftward = branch(
    { x: 80, y: 40 }, { x: 85, y: 40 },
    { x: 20, y: 60 }, { x: 15, y: 60 },
  );
  assertNoImmediateReversal(leftward);
  assert.ok(leftward[2].x > leftward[1].x, "leftward route must escape right first");
  assert.ok(
    leftward[leftward.length - 2].x < leftward[leftward.length - 1].x,
    "left-side sink must be approached from farther left",
  );

  const rightward = branch(
    { x: 20, y: 40 }, { x: 15, y: 40 },
    { x: 80, y: 60 }, { x: 85, y: 60 },
  );
  assertNoImmediateReversal(rightward);
  assert.ok(rightward[2].x < rightward[1].x, "rightward route must escape left first");
  assert.ok(
    rightward[rightward.length - 2].x > rightward[rightward.length - 1].x,
    "right-side sink must be approached from farther right",
  );

  const levelTurnaround = branch(
    { x: 80, y: 50 }, { x: 85, y: 50 },
    { x: 20, y: 50 }, { x: 15, y: 50 },
  );
  assertNoImmediateReversal(levelTurnaround);
  assert.ok(levelTurnaround.some((point) => point.y !== 50), "level turnaround needs a perpendicular lane");
}

main();
