"use strict";

const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");

// Run the actual camera functions with a controlled clock and minimal geometry.
// Browser tests separately exercise real buttons, wheel events, and DOM panels.
const source = fs.readFileSync(path.join(__dirname, "../visualizer-v2/static/app.js"), "utf8");
const functions = ["screenToWorld", "worldToScreen", "zoomAtPoint", "frameRoot",
  "viewportCenter", "fitZoomForRect", "easeInOutCubic", "updateCameraAnimation",
  "animateCameraTo", "closestExistingComponent", "smoothlyFocusComponent"];
function extract(name) {
  const start = source.indexOf(`function ${name}(`);
  assert(start >= 0, `Missing camera function ${name}`);
  const end = source.indexOf("\n}\n", start);
  assert(end > start);
  return source.slice(start, end + 3);
}
let now = 1000;
let renderRequests = 0;
const viewport = { x: 348, y: 84, w: 700, h: 660 };
const root = { id: "ROOT", rect: { x: 0, y: 0, w: 1000, h: 700 } };
const child = { id: "ROOT.CHILD", rect: { x: 200, y: 300, w: 70, h: 50 } };
const app = { camera: { zoom: 1, offset: { x: 10, y: -20 } }, cameraAnimation: null,
  components: new Map([[root.id, root], [child.id, child]]), rootId: root.id, geometryDirty: false };
const context = vm.createContext({
  app, performance: { now: () => now },
  FOCUS_ANIMATION_MIN_MS: 320, FOCUS_ANIMATION_MAX_MS: 700,
  clamp: (x, lo, hi) => Math.min(hi, Math.max(lo, x)),
  resizeCanvas() {}, computeGeometry() {}, cameraSafeViewport: () => viewport,
  componentFocusRect: component => component.rect,
  setSelectedComponent: id => { app.selectedComponentId = id; },
  requestRender: () => { ++renderRequests; },
});
vm.runInContext(functions.map(extract).join("\n"), context);
const camera = () => JSON.parse(JSON.stringify(app.camera));
const near = (a, b) => assert(Math.abs(a-b) <= 1e-9 * Math.max(1, Math.abs(a), Math.abs(b)), `${a} != ${b}`);
function anchored(point, world) {
  const actual = context.screenToWorld(point);
  near(actual.x, world.x); near(actual.y, world.y);
}
function advance(fraction) {
  const animation = app.cameraAnimation;
  assert(animation);
  now = animation.startedAt + animation.duration * fraction;
  context.updateCameraAnimation(now);
}
function finish() { advance(1); assert.equal(app.cameraAnimation, null); }

// Zoom starts without a jump, has intermediate poses, and preserves the cursor.
const point = { x: 511, y: 299 };
const before = camera(), world = context.screenToWorld(point);
context.zoomAtPoint(1.25, point);
assert.deepEqual(camera(), before);
assert.equal(app.cameraAnimation.kind, "zoom");
advance(0.5);
assert(app.camera.zoom > before.zoom && app.camera.zoom < before.zoom * 1.25);
anchored(point, world);
finish(); near(app.camera.zoom, before.zoom * 1.25); anchored(point, world);
context.zoomAtPoint(0.8, point); finish();
near(app.camera.zoom, before.zoom); anchored(point, world);

// Rapid inputs accumulate the full requested zoom, not a fraction of each step.
context.zoomAtPoint(1.25, point);
advance(0.3);
const midZoom = app.camera.zoom;
context.zoomAtPoint(1.25, point);
near(app.camera.zoom, midZoom);
context.zoomAtPoint(1.25, point);
finish(); near(app.camera.zoom, before.zoom * 1.25 ** 3); anchored(point, world);

// Retargeting with a different cursor location anchors that location immediately.
context.zoomAtPoint(1.25, point); advance(0.25);
const movedPoint = { x: 800, y: 450 }, movedWorld = context.screenToWorld(movedPoint);
const movedStart = camera(), pendingZoom = Math.exp(app.cameraAnimation.to.logZoom);
context.zoomAtPoint(0.8, movedPoint);
assert.deepEqual(camera(), movedStart);
advance(0.5); anchored(movedPoint, movedWorld);
finish(); near(app.camera.zoom, pendingZoom * 0.8); anchored(movedPoint, movedWorld);

// Reset uses the focus transition and does not change the selected component.
app.selectedComponentId = child.id;
const resetStart = camera();
context.frameRoot({ animate: true });
assert.deepEqual(camera(), resetStart);
assert.equal(app.selectedComponentId, child.id);
assert.equal(app.cameraAnimation.kind, "focus");
advance(0.5); assert.notDeepEqual(camera(), resetStart);
finish();
const rootZoom = Math.min(viewport.w / root.rect.w, viewport.h / root.rect.h);
near(app.camera.zoom, rootZoom);
anchored(context.viewportCenter(viewport), context.viewportCenter(root.rect));

// Focus remains animated. Interrupting it with zoom discards the focus target.
context.smoothlyFocusComponent(child.id);
assert.equal(app.selectedComponentId, child.id);
advance(0.3);
const focusZoom = app.camera.zoom, focusAnchor = context.screenToWorld(point);
context.zoomAtPoint(1.25, point);
finish(); near(app.camera.zoom, focusZoom * 1.25); anchored(point, focusAnchor);

// Reset interrupts pending zoom; initial scene framing stays immediate.
context.zoomAtPoint(2, point);
context.frameRoot({ animate: true }); finish(); near(app.camera.zoom, rootZoom);
context.zoomAtPoint(2, point);
context.frameRoot(); assert.equal(app.cameraAnimation, null); near(app.camera.zoom, rootZoom);

// Invalid factors leave the camera alone; very small valid zooms do not jump.
const valid = camera();
for (const factor of [NaN, Infinity, 0, -1]) context.zoomAtPoint(factor, point);
assert.deepEqual(camera(), valid); assert.equal(app.cameraAnimation, null);
app.camera.zoom = 1e-6;
const tiny = camera();
context.zoomAtPoint(1.25, point);
assert.deepEqual(camera(), tiny); finish(); near(app.camera.zoom, 1.25e-6);
assert(renderRequests > 0);
console.log("Camera animation: zoom anchoring/accumulation, retargeting, smooth reset/focus, and immediate startup passed");
