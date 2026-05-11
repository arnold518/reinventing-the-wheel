"use strict";

const STATE_COLORS = {
  "1": [76, 175, 80],
  "0": [211, 47, 47],
  "X": [158, 158, 158],
  "Z": [3, 155, 229],
  mixed: [255, 193, 7],
};

const BUS_MARKER_COLOR = "rgb(245, 245, 245)";
const DEFAULT_COLOR = [61, 90, 128, 100];
const WIRE_STROKE_PIN_RATIO = 0.18;
const WIRE_STROKE_MIN_PX = 1;
const WIRE_STROKE_MAX_PX = 4;
const WIRE_HOVER_STROKE_MULTIPLIER = 1.7;
const WIRE_HOVER_STROKE_MIN_PX = 2;
const WIRE_HOVER_STROKE_MAX_PX = 7;
const WIRE_HIT_PADDING_PX = 5;

const canvas = document.getElementById("circuit-canvas");
const ctx = canvas.getContext("2d", { alpha: false });
const tooltip = document.getElementById("tooltip");
const loading = document.getElementById("loading");

const ui = {
  scenario: document.getElementById("scenario-select"),
  slider: document.getElementById("time-slider"),
  playPause: document.getElementById("play-pause"),
  stepBack: document.getElementById("step-back"),
  stepForward: document.getElementById("step-forward"),
  resetSim: document.getElementById("reset-sim"),
  timeLabel: document.getElementById("time-label"),
  statsLabel: document.getElementById("stats-label"),
  resetView: document.getElementById("reset-view"),
  zoomOut: document.getElementById("zoom-out"),
  zoomIn: document.getElementById("zoom-in"),
  saveLayout: document.getElementById("save-layout"),
  saveStatus: document.getElementById("save-status"),
};

const app = {
  dpr: 1,
  width: 1,
  height: 1,
  scenario: null,
  rootId: null,
  timestamps: [0],
  layout: null,
  settings: {},
  stats: null,
  components: new Map(),
  pins: new Map(),
  wires: new Map(),
  componentOrder: [],
  pinOrder: [],
  wireOrder: [],
  state: { pins: {}, wires: {}, index: 0, time: 0 },
  currentIndex: 0,
  requestedIndex: 0,
  stateRequestId: 0,
  stateRequestInFlight: false,
  camera: { offset: { x: 0, y: 0 }, zoom: 1 },
  playing: false,
  playbackInterval: 250,
  lastPlaybackTick: 0,
  hover: null,
  interaction: null,
  geometryDirty: false,
  layoutDirty: false,
  componentIndex: null,
  pinIndex: null,
  wireIndex: null,
};

function apiUrl(path) {
  const normalized = path.replace(/^\//, "");
  return new URL(normalized, document.baseURI).toString();
}

class SpatialHash {
  constructor(cellSize = 220) {
    this.cellSize = cellSize;
    this.cells = new Map();
    this.globalItems = [];
  }

  clear() {
    this.cells.clear();
    this.globalItems = [];
  }

  _key(x, y) {
    return `${x},${y}`;
  }

  _range(rect) {
    const minX = Math.floor(rect.x / this.cellSize);
    const minY = Math.floor(rect.y / this.cellSize);
    const maxX = Math.floor((rect.x + rect.w) / this.cellSize);
    const maxY = Math.floor((rect.y + rect.h) / this.cellSize);
    return { minX, minY, maxX, maxY, count: (maxX - minX + 1) * (maxY - minY + 1) };
  }

  insert(item, rect) {
    const range = this._range(rect);
    if (range.count > 2000) {
      this.globalItems.push(item);
      return;
    }
    for (let y = range.minY; y <= range.maxY; y += 1) {
      for (let x = range.minX; x <= range.maxX; x += 1) {
        const key = this._key(x, y);
        if (!this.cells.has(key)) this.cells.set(key, []);
        this.cells.get(key).push(item);
      }
    }
  }

  queryPoint(point) {
    const key = this._key(Math.floor(point.x / this.cellSize), Math.floor(point.y / this.cellSize));
    return [...(this.cells.get(key) || []), ...this.globalItems];
  }
}

function rgba(color, alphaOverride = null) {
  const a = alphaOverride == null ? (color[3] == null ? 1 : color[3] / 255) : alphaOverride;
  return `rgba(${color[0]}, ${color[1]}, ${color[2]}, ${a})`;
}

function darken(color, amount) {
  return [Math.max(0, color[0] - amount), Math.max(0, color[1] - amount), Math.max(0, color[2] - amount), color[3] ?? 255];
}

function valueColor(value) {
  if (!value || value.length === 0) return STATE_COLORS.X;
  const first = value[0];
  if ([...value].every((bit) => bit === first)) return STATE_COLORS[first] || STATE_COLORS.X;
  if (value.includes("X")) return STATE_COLORS.X;
  if (value.includes("Z")) return STATE_COLORS.Z;
  return STATE_COLORS.mixed;
}

function rectIntersects(a, b) {
  return a.x <= b.x + b.w && a.x + a.w >= b.x && a.y <= b.y + b.h && a.y + a.h >= b.y;
}

function clamp(value, min, max) {
  return Math.max(min, Math.min(max, value));
}

function pointInRect(point, rect, inflate = 0) {
  return point.x >= rect.x - inflate && point.x <= rect.x + rect.w + inflate
    && point.y >= rect.y - inflate && point.y <= rect.y + rect.h + inflate;
}

function unionRect(a, b) {
  if (!a) return { ...b };
  const x = Math.min(a.x, b.x);
  const y = Math.min(a.y, b.y);
  const right = Math.max(a.x + a.w, b.x + b.w);
  const bottom = Math.max(a.y + a.h, b.y + b.h);
  return { x, y, w: right - x, h: bottom - y };
}

function pointsBBox(points) {
  let minX = Infinity;
  let minY = Infinity;
  let maxX = -Infinity;
  let maxY = -Infinity;
  for (const point of points) {
    minX = Math.min(minX, point.x);
    minY = Math.min(minY, point.y);
    maxX = Math.max(maxX, point.x);
    maxY = Math.max(maxY, point.y);
  }
  return { x: minX, y: minY, w: Math.max(0, maxX - minX), h: Math.max(0, maxY - minY) };
}

function distSq(a, b) {
  const dx = a.x - b.x;
  const dy = a.y - b.y;
  return dx * dx + dy * dy;
}

function screenToWorld(point) {
  return {
    x: point.x / app.camera.zoom + app.camera.offset.x,
    y: point.y / app.camera.zoom + app.camera.offset.y,
  };
}

function worldToScreen(point) {
  return {
    x: (point.x - app.camera.offset.x) * app.camera.zoom,
    y: (point.y - app.camera.offset.y) * app.camera.zoom,
  };
}

function zoomAtPoint(factor, screenPoint) {
  const before = screenToWorld(screenPoint);
  app.camera.zoom = Math.max(0.02, Math.min(12, app.camera.zoom * factor));
  const after = screenToWorld(screenPoint);
  app.camera.offset.x += before.x - after.x;
  app.camera.offset.y += before.y - after.y;
}

function viewRect() {
  return {
    x: app.camera.offset.x,
    y: app.camera.offset.y,
    w: app.width / app.camera.zoom,
    h: app.height / app.camera.zoom,
  };
}

function getLayoutFor(component) {
  if (app.layout.instance_layouts && app.layout.instance_layouts[component.id]) {
    return app.layout.instance_layouts[component.id];
  }
  return (app.layout.type_layouts && app.layout.type_layouts[component.type]) || {};
}

function getChildLayout(parent, child) {
  const parentLayout = getLayoutFor(parent);
  return (((parentLayout || {}).children || {})[child.name]) || {};
}

function childEntryFor(parent, child) {
  if (parent.depth === 0) {
    if (!app.layout.instance_layouts[parent.id]) app.layout.instance_layouts[parent.id] = {};
    const parentLayout = app.layout.instance_layouts[parent.id];
    if (!parentLayout.children) parentLayout.children = {};
    if (!parentLayout.children[child.name]) parentLayout.children[child.name] = {};
    return parentLayout.children[child.name];
  }

  if (!app.layout.type_layouts[parent.type]) app.layout.type_layouts[parent.type] = {};
  const parentLayout = app.layout.type_layouts[parent.type];
  if (!parentLayout.children) parentLayout.children = {};
  if (!parentLayout.children[child.name]) parentLayout.children[child.name] = {};
  return parentLayout.children[child.name];
}

function contentRect(component) {
  const titleHeight = component.rect.w * component.titleBarRatio;
  const marginX = component.rect.w * component.boundaryRatio;
  return {
    x: component.rect.x + marginX,
    y: component.rect.y + titleHeight,
    w: component.rect.w - 2 * marginX,
    h: component.rect.h - titleHeight,
  };
}

function ensureChildLayouts(parent) {
  const children = parent.childIds.map((id) => app.components.get(id)).filter(Boolean);
  const count = children.length;
  if (count === 0) return;

  const cols = Math.ceil(Math.sqrt(count));
  const rows = Math.ceil(count / cols);
  const padding = 0.1;
  const cellW = cols > 0 ? (1.0 - padding * (cols + 1)) / cols : 0;
  const cellH = rows > 0 ? (1.0 - padding * (rows + 1)) / rows : 0;
  const parentLayout = getLayoutFor(parent);
  const parentAspect = Number(parentLayout.aspect_ratio ?? parent.aspectRatio ?? (parent.depth === 0 ? 0.75 : 1.0));
  const titleFraction = Math.min(0.7, Number(app.settings.title_bar_ratio ?? 0.15) / Math.max(parentAspect, 0.01));
  const usableYFraction = Math.max(0.1, 1.0 - titleFraction);

  children.forEach((child, index) => {
    const layout = getChildLayout(parent, child);
    if (layout.rel_pos && layout.rel_width != null) return;

    const col = index % cols;
    const row = Math.floor(index / cols);
    const entry = childEntryFor(parent, child);
    entry.rel_pos = [
      padding + col * (cellW + padding),
      titleFraction + usableYFraction * (padding + row * (cellH + padding)),
    ];
    entry.rel_width = cellW;
  });
}

function componentSettings(component) {
  component.titleBarRatio = Number(app.settings.title_bar_ratio ?? 0.15);
  component.fontWidthRatio = Number(app.settings.font_width_ratio ?? 0.18);
  component.boundaryRatio = Number(app.settings.boundary_area_ratio ?? 0.15);
  component.pinSizeRatio = Number(app.settings.pin_size_ratio ?? 0.10);
}

function computeGeometry() {
  if (!app.rootId || !app.layout) return;

  app.componentOrder.forEach((component) => componentSettings(component));

  const root = app.components.get(app.rootId);
  const rootLayout = getLayoutFor(root);
  const rootConfig = app.layout.root_layout_config || { pos: [50, 150], width: 800 };
  const rootWidth = Number(rootConfig.width ?? 800);
  const rootAspect = Number(rootLayout.aspect_ratio ?? root.aspectRatio ?? 0.75);
  root.aspectRatio = rootAspect;
  root.color = rootLayout.color || root.color || DEFAULT_COLOR;
  root.rect = {
    x: Number((rootConfig.pos || [50, 150])[0]),
    y: Number((rootConfig.pos || [50, 150])[1]),
    w: rootWidth,
    h: rootWidth * rootAspect,
  };

  function layoutChildren(parent) {
    ensureChildLayouts(parent);
    for (const childId of parent.childIds) {
      const child = app.components.get(childId);
      if (!child) continue;
      const childOwnLayout = getLayoutFor(child);
      const childLayout = getChildLayout(parent, child);
      child.aspectRatio = Number(childOwnLayout.aspect_ratio ?? child.aspectRatio ?? 1.0);
      child.color = childOwnLayout.color || child.color || DEFAULT_COLOR;
      const relPos = childLayout.rel_pos || [0, 0];
      const relWidth = Number(childLayout.rel_width ?? 0.5);
      child.rect = {
        x: parent.rect.x + parent.rect.w * Number(relPos[0]),
        y: parent.rect.y + parent.rect.h * Number(relPos[1]),
        w: parent.rect.w * relWidth,
        h: parent.rect.w * relWidth * child.aspectRatio,
      };
      layoutChildren(child);
    }
  }

  layoutChildren(root);
  app.componentOrder.forEach(layoutPins);
  app.wireOrder.forEach(buildWirePaths);
  rebuildSpatialIndexes();
  app.geometryDirty = false;
}

function layoutPins(component) {
  const cr = contentRect(component);
  const pWidth = component.rect.w * component.boundaryRatio;
  const pinW = component.rect.w * component.pinSizeRatio;
  const pinH = pinW;
  const w2 = pinW / 2;
  const h2 = pinH / 2;

  function setupPin(pin, yCenter, isInput) {
    pin.rect = {
      x: (isInput ? component.rect.x : component.rect.x + component.rect.w) - pinW / 2,
      y: yCenter - pinH / 2,
      w: pinW,
      h: pinH,
    };
    pin.pos = { x: isInput ? component.rect.x : component.rect.x + component.rect.w, y: yCenter };
    pin.relPoints = [{ x: w2, y: 0 }, { x: -w2, y: -h2 }, { x: -w2, y: h2 }];

    if (isInput) {
      pin.outerStub = [{ x: component.rect.x - w2, y: yCenter }, { x: component.rect.x - pWidth, y: yCenter }];
      pin.innerStub = [{ x: component.rect.x + w2, y: yCenter }, { x: component.rect.x + pWidth, y: yCenter }];
    } else {
      pin.innerStub = [{ x: component.rect.x + component.rect.w - w2, y: yCenter }, { x: component.rect.x + component.rect.w - pWidth, y: yCenter }];
      pin.outerStub = [{ x: component.rect.x + component.rect.w + w2, y: yCenter }, { x: component.rect.x + component.rect.w + pWidth, y: yCenter }];
    }
  }

  if (component.inputPins.length) {
    const spacing = cr.h / (component.inputPins.length + 1);
    component.inputPins.forEach((pin, index) => setupPin(pin, cr.y + spacing * (index + 1), true));
  }

  if (component.outputPins.length) {
    const spacing = cr.h / (component.outputPins.length + 1);
    component.outputPins.forEach((pin, index) => setupPin(pin, cr.y + spacing * (index + 1), false));
  }
}

function activeStub(pin, isSource) {
  if (isSource) return pin.type === "input" ? pin.innerStub : pin.outerStub;
  return pin.type === "input" ? pin.outerStub : pin.innerStub;
}

function pinWireStrokeWorld(pin) {
  if (!pin || !pin.rect) return 1;
  return Math.max(0.25, Math.min(pin.rect.w, pin.rect.h) * WIRE_STROKE_PIN_RATIO);
}

function wireStrokePx(segment, hovered) {
  const basePx = Math.max(0, segment.strokeWorld || 1) * app.camera.zoom;
  const scaledPx = hovered ? basePx * WIRE_HOVER_STROKE_MULTIPLIER : basePx;
  const minPx = hovered ? WIRE_HOVER_STROKE_MIN_PX : WIRE_STROKE_MIN_PX;
  const maxPx = hovered ? WIRE_HOVER_STROKE_MAX_PX : WIRE_STROKE_MAX_PX;
  return Math.round(clamp(scaledPx, minPx, maxPx) * 2) / 2;
}

function dedupePath(path) {
  const result = [];
  for (const point of path) {
    if (result.length === 0 || distSq(point, result[result.length - 1]) > 0.01) {
      result.push({ x: point.x, y: point.y });
    }
  }
  return result;
}

function routeAdaptive(start, end) {
  const dx = end.x - start.x;
  if (dx > 10) return dedupePath([start, { x: end.x, y: start.y }, end]);
  const midX = (start.x + end.x) / 2;
  return dedupePath([start, { x: midX, y: start.y }, { x: midX, y: end.y }, end]);
}

function normalizeVector(v) {
  const length = Math.hypot(v.x, v.y);
  return length <= 0 ? { x: 0, y: 0 } : { x: v.x / length, y: v.y / length };
}

function addRoundedCorners(path, radius = 5) {
  path = dedupePath(path);
  if (path.length < 3) return path;

  const rounded = [path[0]];
  for (let index = 1; index < path.length - 1; index += 1) {
    const prev = path[index - 1];
    const corner = path[index];
    const next = path[index + 1];
    const incoming = { x: corner.x - prev.x, y: corner.y - prev.y };
    const outgoing = { x: next.x - corner.x, y: next.y - corner.y };
    const incomingLen = Math.hypot(incoming.x, incoming.y);
    const outgoingLen = Math.hypot(outgoing.x, outgoing.y);
    if (incomingLen < radius * 2 || outgoingLen < radius * 2) {
      rounded.push(corner);
      continue;
    }

    const inDir = normalizeVector(incoming);
    const outDir = normalizeVector(outgoing);
    if (Math.abs(inDir.x * outDir.x + inDir.y * outDir.y) > 0.999) {
      rounded.push(corner);
      continue;
    }

    const cornerRadius = Math.min(radius, incomingLen / 2, outgoingLen / 2);
    const arcStart = { x: corner.x - inDir.x * cornerRadius, y: corner.y - inDir.y * cornerRadius };
    const arcEnd = { x: corner.x + outDir.x * cornerRadius, y: corner.y + outDir.y * cornerRadius };
    rounded.push(arcStart);
    for (let step = 1; step < 4; step += 1) {
      const t = step / 4;
      const oneMinus = 1 - t;
      rounded.push({
        x: oneMinus * oneMinus * arcStart.x + 2 * oneMinus * t * corner.x + t * t * arcEnd.x,
        y: oneMinus * oneMinus * arcStart.y + 2 * oneMinus * t * corner.y + t * t * arcEnd.y,
      });
    }
    rounded.push(arcEnd);
  }
  rounded.push(path[path.length - 1]);
  return dedupePath(rounded);
}

function buildWirePaths(wire) {
  wire.paths = [];
  wire.bbox = null;
  const source = wire.sourcePinId ? app.pins.get(wire.sourcePinId) : null;
  if (!source || wire.sinkPins.length === 0) return;

  const [srcPinPoint, srcBoundaryPoint] = activeStub(source, true);
  const sourceStub = [srcPinPoint, srcBoundaryPoint];
  const sourceStrokeWorld = pinWireStrokeWorld(source);
  wire.paths.push({ kind: "stub", points: sourceStub, bbox: pointsBBox(sourceStub), strokeWorld: sourceStrokeWorld });
  wire.bbox = unionRect(wire.bbox, pointsBBox(sourceStub));

  for (const sink of wire.sinkPins) {
    const [dstPinPoint, dstBoundaryPoint] = activeStub(sink, false);
    const sinkStub = [dstBoundaryPoint, dstPinPoint];
    const sinkBBox = pointsBBox(sinkStub);
    const sinkStrokeWorld = pinWireStrokeWorld(sink);
    wire.paths.push({ kind: "stub", points: sinkStub, bbox: sinkBBox, strokeWorld: sinkStrokeWorld });
    wire.bbox = unionRect(wire.bbox, sinkBBox);

    const route = addRoundedCorners(routeAdaptive(srcBoundaryPoint, dstBoundaryPoint));
    const routeBBox = pointsBBox(route);
    wire.paths.push({ kind: "route", points: route, bbox: routeBBox, strokeWorld: Math.min(sourceStrokeWorld, sinkStrokeWorld) });
    wire.bbox = unionRect(wire.bbox, routeBBox);
  }
}

function rebuildSpatialIndexes() {
  app.componentIndex = new SpatialHash(260);
  app.pinIndex = new SpatialHash(180);
  app.wireIndex = new SpatialHash(260);

  app.componentOrder.forEach((component, index) => {
    if (component.rect) app.componentIndex.insert({ id: component.id, z: index }, component.rect);
  });
  app.pinOrder.forEach((pin, index) => {
    if (pin.rect) app.pinIndex.insert({ id: pin.id, z: index }, pin.rect);
  });
  app.wireOrder.forEach((wire, index) => {
    if (!wire.bbox) return;
    const padded = { x: wire.bbox.x - 8, y: wire.bbox.y - 8, w: wire.bbox.w + 16, h: wire.bbox.h + 16 };
    app.wireIndex.insert({ id: wire.id, z: index }, padded);
  });
}

function resizeCanvas() {
  const nextWidth = window.innerWidth;
  const nextHeight = window.innerHeight;
  const nextDpr = Math.max(1, window.devicePixelRatio || 1);
  if (nextWidth === app.width && nextHeight === app.height && nextDpr === app.dpr) return;

  app.width = nextWidth;
  app.height = nextHeight;
  app.dpr = nextDpr;
  canvas.width = Math.floor(nextWidth * nextDpr);
  canvas.height = Math.floor(nextHeight * nextDpr);
  canvas.style.width = `${nextWidth}px`;
  canvas.style.height = `${nextHeight}px`;
  ctx.setTransform(nextDpr, 0, 0, nextDpr, 0, 0);
}

function drawGrid() {
  const targetPixelsPerLine = 100;
  const worldUnitsPerLine = targetPixelsPerLine / app.camera.zoom;
  let powerOfTen = 10 ** Math.floor(Math.log10(worldUnitsPerLine));
  let gridSpacing = powerOfTen;
  if (worldUnitsPerLine / powerOfTen < 0.5) gridSpacing = powerOfTen / 2;
  if (worldUnitsPerLine / powerOfTen > 2) gridSpacing = powerOfTen * 2;
  const step = Math.max(1, Math.floor(gridSpacing));

  const vr = viewRect();
  const xStart = Math.floor(vr.x / gridSpacing) * gridSpacing;
  const yStart = Math.floor(vr.y / gridSpacing) * gridSpacing;
  const xEnd = vr.x + vr.w + gridSpacing;
  const yEnd = vr.y + vr.h + gridSpacing;

  ctx.lineWidth = 1;
  let num = 0;
  for (let x = xStart; x <= xEnd; x += step) {
    const start = worldToScreen({ x, y: yStart });
    const end = worldToScreen({ x, y: yEnd });
    ctx.strokeStyle = num % 10 === 0 ? "rgb(60, 60, 60)" : "rgb(45, 45, 45)";
    ctx.beginPath();
    ctx.moveTo(start.x, start.y);
    ctx.lineTo(end.x, end.y);
    ctx.stroke();
    num += 1;
  }

  num = 0;
  for (let y = yStart; y <= yEnd; y += step) {
    const start = worldToScreen({ x: xStart, y });
    const end = worldToScreen({ x: xEnd, y });
    ctx.strokeStyle = num % 10 === 0 ? "rgb(60, 60, 60)" : "rgb(45, 45, 45)";
    ctx.beginPath();
    ctx.moveTo(start.x, start.y);
    ctx.lineTo(end.x, end.y);
    ctx.stroke();
    num += 1;
  }
}

function strokePolyline(points, color, lineWidth) {
  if (points.length < 2) return;
  ctx.strokeStyle = color;
  ctx.lineWidth = lineWidth;
  ctx.lineCap = "round";
  ctx.lineJoin = "round";
  const first = worldToScreen(points[0]);
  ctx.beginPath();
  ctx.moveTo(first.x, first.y);
  for (let i = 1; i < points.length; i += 1) {
    const point = worldToScreen(points[i]);
    ctx.lineTo(point.x, point.y);
  }
  ctx.stroke();
}

function pointAtFraction(path, fraction) {
  if (!path.length) return { x: 0, y: 0 };
  const lengths = [];
  let total = 0;
  for (let i = 0; i < path.length - 1; i += 1) {
    const length = Math.hypot(path[i + 1].x - path[i].x, path[i + 1].y - path[i].y);
    lengths.push(length);
    total += length;
  }
  if (total <= 0) return { ...path[0] };
  const target = total * fraction;
  let traversed = 0;
  for (let i = 0; i < lengths.length; i += 1) {
    if (traversed + lengths[i] >= target) {
      const local = lengths[i] ? (target - traversed) / lengths[i] : 0;
      return {
        x: path[i].x + (path[i + 1].x - path[i].x) * local,
        y: path[i].y + (path[i + 1].y - path[i].y) * local,
      };
    }
    traversed += lengths[i];
  }
  return { ...path[path.length - 1] };
}

function drawBusSlash(worldPoint, lineWidth) {
  const center = worldToScreen(worldPoint);
  const length = clamp(lineWidth * 4, 6, 14);
  const half = length / 2;
  ctx.strokeStyle = BUS_MARKER_COLOR;
  ctx.lineWidth = Math.max(1, Math.min(2.5, lineWidth));
  ctx.beginPath();
  ctx.moveTo(center.x - half, center.y + half);
  ctx.lineTo(center.x + half, center.y - half);
  ctx.stroke();
}

function drawBusAnnotations(wire, path, lineWidth) {
  if (wire.width <= 1) return;
  drawBusSlash(pointAtFraction(path, 0.33), lineWidth);
  drawBusSlash(pointAtFraction(path, 0.66), lineWidth);
}

function isHovered(type, id) {
  return app.hover && app.hover.type === type && app.hover.id === id;
}

function drawWires() {
  const vr = viewRect();
  for (const wire of app.wireOrder) {
    if (!wire.paths || !wire.bbox || !rectIntersects(wire.bbox, vr)) continue;
    const color = `rgb(${valueColor(app.state.wires[wire.id] || "X").join(",")})`;
    const hovered = isHovered("wire", wire.id);
    for (const segment of wire.paths) {
      if (!rectIntersects(segment.bbox, vr)) continue;
      const lineWidth = wireStrokePx(segment, hovered);
      strokePolyline(segment.points, color, lineWidth);
      if (segment.kind === "route") drawBusAnnotations(wire, segment.points, lineWidth);
    }
  }
}

function drawRoundedRect(rect, fillStyle, strokeStyle, lineWidth) {
  const screen = worldToScreen({ x: rect.x, y: rect.y });
  const w = rect.w * app.camera.zoom;
  const h = rect.h * app.camera.zoom;
  const radius = Math.min(5, Math.abs(w) / 2, Math.abs(h) / 2);
  ctx.beginPath();
  if (ctx.roundRect) {
    ctx.roundRect(screen.x, screen.y, w, h, radius);
  } else {
    ctx.rect(screen.x, screen.y, w, h);
  }
  ctx.fillStyle = fillStyle;
  ctx.fill();
  ctx.strokeStyle = strokeStyle;
  ctx.lineWidth = lineWidth;
  ctx.stroke();
}

function drawComponent(component, vr) {
  if (!component.rect || !rectIntersects(component.rect, vr)) return;

  const titleHeight = component.rect.w * component.titleBarRatio;
  const titleRect = { x: component.rect.x, y: component.rect.y, w: component.rect.w, h: titleHeight };
  const bodyRect = { x: component.rect.x, y: component.rect.y + titleHeight, w: component.rect.w, h: Math.max(1, component.rect.h - titleHeight) };
  const baseColor = component.color || DEFAULT_COLOR;
  const fillColor = isHovered("component", component.id) ? rgba([baseColor[0], baseColor[1], baseColor[2], 150]) : rgba(baseColor);

  drawRoundedRect(bodyRect, fillColor, "rgba(238, 244, 255, 0.95)", isHovered("component", component.id) ? 4 : 2);
  drawRoundedRect(titleRect, rgba(darken(baseColor, 20)), "rgba(238, 244, 255, 0.95)", isHovered("component", component.id) ? 4 : 2);

  const fontSize = component.rect.w * component.fontWidthRatio * app.camera.zoom;
  if (fontSize >= 10) {
    const topLeft = worldToScreen({ x: component.rect.x, y: component.rect.y });
    ctx.font = `${Math.floor(fontSize)}px Arial, Helvetica, sans-serif`;
    ctx.fillStyle = "rgb(240, 240, 240)";
    ctx.textAlign = "center";
    ctx.textBaseline = "middle";
    ctx.fillText(component.name, topLeft.x + component.rect.w * app.camera.zoom / 2, topLeft.y + titleHeight * app.camera.zoom / 2);
  }

  for (const childId of component.childIds) {
    const child = app.components.get(childId);
    if (child) drawComponent(child, vr);
  }

  for (const pin of [...component.inputPins, ...component.outputPins]) drawPin(pin);
}

function drawPin(pin) {
  const center = worldToScreen(pin.pos);
  const color = `rgb(${valueColor(app.state.pins[pin.id] || "X").join(",")})`;
  ctx.beginPath();
  pin.relPoints.forEach((rel, index) => {
    const point = { x: center.x + rel.x * app.camera.zoom, y: center.y + rel.y * app.camera.zoom };
    if (index === 0) ctx.moveTo(point.x, point.y);
    else ctx.lineTo(point.x, point.y);
  });
  ctx.closePath();
  ctx.fillStyle = color;
  ctx.fill();
  if (isHovered("pin", pin.id)) {
    ctx.strokeStyle = "rgb(255, 255, 0)";
    ctx.lineWidth = 2;
    ctx.stroke();
  }

  const component = app.components.get(pin.componentId);
  const fontSize = Math.floor(Math.min(
    component.rect.w * component.fontWidthRatio * 0.7 * app.camera.zoom,
    pin.rect.h * 0.45 * app.camera.zoom,
    30,
  ));
  if (fontSize >= 8) {
    const label = pin.width === 1 ? pin.name : `${pin.name}[${pin.width}]`;
    ctx.font = `${fontSize}px Arial, Helvetica, sans-serif`;
    ctx.fillStyle = "rgb(220, 220, 220)";
    ctx.textBaseline = "middle";
    if (pin.type === "input") {
      ctx.textAlign = "left";
      ctx.fillText(label, center.x + pin.rect.w * 0.8 * app.camera.zoom, center.y);
    } else {
      ctx.textAlign = "right";
      ctx.fillText(label, center.x - pin.rect.w * 0.8 * app.camera.zoom, center.y);
    }
  }
}

function render() {
  resizeCanvas();
  if (app.geometryDirty) computeGeometry();

  ctx.fillStyle = "rgb(30, 30, 30)";
  ctx.fillRect(0, 0, app.width, app.height);
  drawGrid();
  drawWires();

  const root = app.components.get(app.rootId);
  if (root) drawComponent(root, viewRect());

  if (app.playing && !app.stateRequestInFlight && performance.now() - app.lastPlaybackTick > app.playbackInterval) {
    app.lastPlaybackTick = performance.now();
    if (app.currentIndex >= app.timestamps.length - 1) setPlaying(false);
    else {
      setStateIndex(app.currentIndex + 1).catch((error) => {
        setPlaying(false);
        showError(error);
      });
    }
  }

  requestAnimationFrame(render);
}

function distanceToPathSq(world, path) {
  let best = Infinity;
  for (let index = 0; index < path.length - 1; index += 1) {
    const p1 = path[index];
    const p2 = path[index + 1];
    const line = { x: p2.x - p1.x, y: p2.y - p1.y };
    const lenSq = line.x * line.x + line.y * line.y;
    if (lenSq === 0) continue;
    const point = { x: world.x - p1.x, y: world.y - p1.y };
    const t = Math.max(0, Math.min(1, (point.x * line.x + point.y * line.y) / lenSq));
    const projection = { x: p1.x + line.x * t, y: p1.y + line.y * t };
    best = Math.min(best, distSq(world, projection));
  }
  return best;
}

function wireHit(wire, world) {
  if (!wire.paths) return false;
  const maxStrokePx = wire.paths.reduce((maxWidth, segment) => Math.max(maxWidth, wireStrokePx(segment, false)), WIRE_STROKE_MIN_PX);
  const thresholdSq = (Math.max(7, maxStrokePx + WIRE_HIT_PADDING_PX) / app.camera.zoom) ** 2;
  return wire.paths.some((path) => distanceToPathSq(world, path.points) < thresholdSq);
}

function hoveredBorder(component, world) {
  if (!component || !component.rect || !pointInRect(world, component.rect)) return null;
  if (!component.parentId) return null;
  const threshold = 5 / app.camera.zoom;
  const onLeft = Math.abs(world.x - component.rect.x) < threshold;
  const onRight = Math.abs(world.x - (component.rect.x + component.rect.w)) < threshold;
  const onTop = Math.abs(world.y - component.rect.y) < threshold;
  const onBottom = Math.abs(world.y - (component.rect.y + component.rect.h)) < threshold;
  const inY = world.y > component.rect.y && world.y < component.rect.y + component.rect.h;
  const inX = world.x > component.rect.x && world.x < component.rect.x + component.rect.w;
  if (onLeft && inY) return "left";
  if (onRight && inY) return "right";
  if (onTop && inX) return "top";
  if (onBottom && inX) return "bottom";
  return null;
}

function updateCursor(world) {
  if (app.interaction) return;
  if (app.hover && app.hover.type === "component") {
    const border = hoveredBorder(app.components.get(app.hover.id), world);
    if (border === "left" || border === "right") {
      canvas.style.cursor = "ew-resize";
      return;
    }
    if (border === "top" || border === "bottom") {
      canvas.style.cursor = "ns-resize";
      return;
    }
    canvas.style.cursor = "grab";
    return;
  }
  canvas.style.cursor = app.hover ? "pointer" : "default";
}

function updateHover(screenPoint) {
  if (app.interaction || !app.pinIndex) return;
  const world = screenToWorld(screenPoint);
  let hover = null;

  const pinCandidates = app.pinIndex.queryPoint(world).sort((a, b) => b.z - a.z);
  for (const candidate of pinCandidates) {
    const pin = app.pins.get(candidate.id);
    if (pin && pointInRect(world, pin.rect, 4 / app.camera.zoom)) {
      hover = { type: "pin", id: pin.id };
      break;
    }
  }

  if (!hover) {
    const wireCandidates = app.wireIndex.queryPoint(world).sort((a, b) => a.z - b.z);
    for (const candidate of wireCandidates) {
      const wire = app.wires.get(candidate.id);
      if (wire && wireHit(wire, world)) {
        hover = { type: "wire", id: wire.id };
        break;
      }
    }
  }

  if (!hover) {
    const componentCandidates = app.componentIndex.queryPoint(world).sort((a, b) => b.z - a.z);
    for (const candidate of componentCandidates) {
      const component = app.components.get(candidate.id);
      if (component && pointInRect(world, component.rect)) {
        hover = { type: "component", id: component.id };
        break;
      }
    }
  }

  app.hover = hover;
  updateCursor(world);
  updateTooltip(screenPoint);
}

function updateTooltip(screenPoint) {
  if (!app.hover || app.interaction) {
    tooltip.style.display = "none";
    return;
  }

  let text = "";
  if (app.hover.type === "pin") {
    const pin = app.pins.get(app.hover.id);
    const label = pin.width === 1 ? pin.name : `${pin.name}[${pin.width}]`;
    text = `${label} = ${app.state.pins[pin.id] || "X"}`;
  } else if (app.hover.type === "wire") {
    const wire = app.wires.get(app.hover.id);
    text = `${wire.name}[${wire.width}]=${app.state.wires[wire.id] || "X"}`;
  } else {
    const component = app.components.get(app.hover.id);
    text = `${component.name} (${component.type})`;
  }

  tooltip.textContent = text;
  tooltip.style.display = "block";
  const x = Math.min(window.innerWidth - tooltip.offsetWidth - 6, screenPoint.x + 14);
  const y = Math.min(window.innerHeight - tooltip.offsetHeight - 6, screenPoint.y + 14);
  tooltip.style.left = `${Math.max(4, x)}px`;
  tooltip.style.top = `${Math.max(4, y)}px`;
}

function markLayoutDirty() {
  app.layoutDirty = true;
  ui.saveStatus.textContent = "Unsaved layout";
}

function moveComponent(component, worldPos) {
  if (component.parentId) {
    const parent = app.components.get(component.parentId);
    const safe = contentRect(parent);
    const qMargin = component.rect.w * component.boundaryRatio;
    let x = worldPos.x;
    let y = worldPos.y;
    const minX = safe.x + qMargin;
    const maxX = safe.x + safe.w - component.rect.w - qMargin;
    const minY = safe.y;
    const maxY = safe.y + safe.h - component.rect.h;
    x = minX > maxX ? (minX + maxX) / 2 : Math.max(minX, Math.min(x, maxX));
    y = Math.max(minY, Math.min(y, maxY));

    const entry = childEntryFor(parent, component);
    entry.rel_pos = [(x - parent.rect.x) / parent.rect.w, (y - parent.rect.y) / parent.rect.h];
    entry.rel_width = component.rect.w / parent.rect.w;
  } else {
    app.layout.root_layout_config.pos = [worldPos.x, worldPos.y];
  }
  app.geometryDirty = true;
  markLayoutDirty();
}

function resizeComponent(component, mode, world) {
  if (!component.parentId) return;
  const parent = app.components.get(component.parentId);
  const safe = contentRect(parent);
  const old = { ...component.rect };
  const minWidth = 60;
  let width = old.w;
  let left = old.x;
  let top = old.y;

  if (mode === "right") width = Math.max(minWidth, world.x - old.x);
  if (mode === "left") {
    width = Math.max(minWidth, old.x + old.w - world.x);
    left = old.x + old.w - width;
  }
  if (mode === "bottom") width = Math.max(minWidth, (world.y - old.y) / component.aspectRatio);
  if (mode === "top") {
    width = Math.max(minWidth, (old.y + old.h - world.y) / component.aspectRatio);
    top = old.y + old.h - width * component.aspectRatio;
  }

  const height = width * component.aspectRatio;
  const margin = width * component.boundaryRatio;
  left = Math.max(safe.x + margin, Math.min(left, safe.x + safe.w - width - margin));
  top = Math.max(safe.y, Math.min(top, safe.y + safe.h - height));

  const entry = childEntryFor(parent, component);
  entry.rel_pos = [(left - parent.rect.x) / parent.rect.w, (top - parent.rect.y) / parent.rect.h];
  entry.rel_width = width / parent.rect.w;
  app.geometryDirty = true;
  markLayoutDirty();
}

function pointerPosition(event) {
  const rect = canvas.getBoundingClientRect();
  return { x: event.clientX - rect.left, y: event.clientY - rect.top };
}

canvas.addEventListener("pointerdown", (event) => {
  canvas.setPointerCapture(event.pointerId);
  const screen = pointerPosition(event);
  updateHover(screen);
  const world = screenToWorld(screen);

  if (app.hover && app.hover.type === "component") {
    const component = app.components.get(app.hover.id);
    const border = hoveredBorder(component, world);
    if (border) {
      app.interaction = { type: "resize", componentId: component.id, mode: border };
    } else {
      app.interaction = {
        type: "drag",
        componentId: component.id,
        offset: { x: world.x - component.rect.x, y: world.y - component.rect.y },
      };
      canvas.style.cursor = "grabbing";
    }
  } else {
    app.interaction = { type: "pan", last: screen };
    canvas.style.cursor = "grabbing";
  }
});

canvas.addEventListener("pointermove", (event) => {
  const screen = pointerPosition(event);
  const world = screenToWorld(screen);
  if (!app.interaction) {
    updateHover(screen);
    return;
  }

  if (app.interaction.type === "pan") {
    const delta = { x: screen.x - app.interaction.last.x, y: screen.y - app.interaction.last.y };
    app.camera.offset.x -= delta.x / app.camera.zoom;
    app.camera.offset.y -= delta.y / app.camera.zoom;
    app.interaction.last = screen;
  } else if (app.interaction.type === "drag") {
    const component = app.components.get(app.interaction.componentId);
    moveComponent(component, { x: world.x - app.interaction.offset.x, y: world.y - app.interaction.offset.y });
  } else if (app.interaction.type === "resize") {
    const component = app.components.get(app.interaction.componentId);
    resizeComponent(component, app.interaction.mode, world);
  }
});

function endInteraction(event) {
  if (event.pointerId != null && canvas.hasPointerCapture(event.pointerId)) {
    canvas.releasePointerCapture(event.pointerId);
  }
  app.interaction = null;
  canvas.style.cursor = "default";
  tooltip.style.display = "none";
}

canvas.addEventListener("pointerup", endInteraction);
canvas.addEventListener("pointercancel", endInteraction);

canvas.addEventListener("wheel", (event) => {
  event.preventDefault();
  zoomAtPoint(event.deltaY < 0 ? 1.1 : 0.9, pointerPosition(event));
}, { passive: false });

function frameRoot() {
  const root = app.components.get(app.rootId);
  if (!root || !root.rect) return;
  const padding = 0.1;
  const paddedW = app.width * (1 - padding);
  const paddedH = app.height * (1 - padding);
  app.camera.zoom = Math.min(paddedW / root.rect.w, paddedH / root.rect.h);
  app.camera.zoom = Math.max(0.02, Math.min(12, app.camera.zoom));
  const screenCenter = { x: app.width / 2, y: app.height / 2 };
  const worldCenter = { x: root.rect.x + root.rect.w / 2, y: root.rect.y + root.rect.h / 2 };
  app.camera.offset.x = worldCenter.x - screenCenter.x / app.camera.zoom;
  app.camera.offset.y = worldCenter.y - screenCenter.y / app.camera.zoom;
}

function setPlaying(value) {
  app.playing = value;
  ui.playPause.textContent = value ? "Pause" : "Play";
  app.lastPlaybackTick = performance.now();
}

async function setStateIndex(index) {
  index = Math.max(0, Math.min(index, app.timestamps.length - 1));
  app.requestedIndex = index;
  ui.slider.value = String(index);
  const requestId = ++app.stateRequestId;
  app.stateRequestInFlight = true;
  try {
    const response = await fetch(apiUrl(`api/state?scenario=${encodeURIComponent(app.scenario)}&index=${index}`));
    if (!response.ok) throw new Error(await response.text());
    const state = await response.json();
    if (requestId !== app.stateRequestId) return;
    app.state = state;
    app.currentIndex = state.index;
    app.requestedIndex = state.index;
    ui.slider.value = String(state.index);
    ui.timeLabel.textContent = `Time: ${state.time}`;
  } finally {
    if (requestId === app.stateRequestId) app.stateRequestInFlight = false;
  }
}

function step(delta) {
  if (app.playing) setPlaying(false);
  setStateIndex(app.requestedIndex + delta).catch(showError);
}

async function saveLayout() {
  ui.saveStatus.textContent = "Saving...";
  const response = await fetch(apiUrl("api/layout"), {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ layout: app.layout }),
  });
  if (!response.ok) throw new Error(await response.text());
  app.layoutDirty = false;
  ui.saveStatus.textContent = "Layout saved";
}

function showError(error) {
  console.error(error);
  loading.textContent = String(error.message || error);
  loading.classList.remove("hidden");
}

function buildLocalModel(payload) {
  app.scenario = payload.scenario;
  app.rootId = payload.rootId;
  app.timestamps = payload.timestamps && payload.timestamps.length ? payload.timestamps : [0];
  app.layout = payload.layout;
  app.settings = payload.layout.default_settings || {};
  app.stats = payload.stats;
  app.state = payload.state;
  app.currentIndex = 0;
  app.requestedIndex = 0;
  app.stateRequestInFlight = false;
  app.components = new Map();
  app.pins = new Map();
  app.wires = new Map();
  app.componentOrder = [];
  app.pinOrder = [];
  app.wireOrder = [];

  for (const data of payload.components) {
    const component = {
      ...data,
      childIds: data.childIds || [],
      inputPins: [],
      outputPins: [],
      rect: null,
      color: data.color || DEFAULT_COLOR,
    };
    app.components.set(component.id, component);
    app.componentOrder.push(component);
  }

  for (const data of payload.pins) {
    const pin = { ...data, rect: null, pos: null, relPoints: [], outerStub: [], innerStub: [] };
    app.pins.set(pin.id, pin);
    app.pinOrder.push(pin);
    const component = app.components.get(pin.componentId);
    if (component) {
      if (pin.type === "input") component.inputPins.push(pin);
      else component.outputPins.push(pin);
    }
  }

  for (const data of payload.wires) {
    const wire = {
      ...data,
      sinkPins: (data.sinkPinIds || []).map((id) => app.pins.get(id)).filter(Boolean),
      paths: [],
      bbox: null,
    };
    app.wires.set(wire.id, wire);
    app.wireOrder.push(wire);
  }

  ui.slider.min = "0";
  ui.slider.max = String(Math.max(0, app.timestamps.length - 1));
  ui.slider.value = "0";
  ui.timeLabel.textContent = `Time: ${app.state.time || 0}`;
  ui.statsLabel.textContent = `${payload.stats.componentCount} comps | ${payload.stats.wireCount} wires | ${payload.stats.pinCount} pins`;
  app.geometryDirty = true;
  computeGeometry();
  frameRoot();
}

async function loadScenarios() {
  const response = await fetch(apiUrl("api/scenarios"));
  if (!response.ok) throw new Error(await response.text());
  const payload = await response.json();
  const params = new URLSearchParams(window.location.search);
  const selected = params.get("scenario") || payload.default;
  ui.scenario.innerHTML = "";
  for (const scenario of payload.scenarios) {
    const option = document.createElement("option");
    option.value = scenario.key;
    option.textContent = `${scenario.key} (${scenario.className})`;
    if (scenario.key === selected) option.selected = true;
    ui.scenario.appendChild(option);
  }
  return selected;
}

async function loadCircuit(scenario) {
  loading.textContent = "Loading circuit...";
  loading.classList.remove("hidden");
  setPlaying(false);
  const response = await fetch(apiUrl(`api/circuit?scenario=${encodeURIComponent(scenario)}`));
  if (!response.ok) throw new Error(await response.text());
  buildLocalModel(await response.json());
  loading.classList.add("hidden");
  ui.saveStatus.textContent = "";
}

ui.scenario.addEventListener("change", () => {
  const url = new URL(window.location);
  url.searchParams.set("scenario", ui.scenario.value);
  window.history.replaceState({}, "", url);
  loadCircuit(ui.scenario.value).catch(showError);
});
ui.playPause.addEventListener("click", () => setPlaying(!app.playing));
ui.stepBack.addEventListener("click", () => step(-1));
ui.stepForward.addEventListener("click", () => step(1));
ui.resetSim.addEventListener("click", () => {
  if (app.playing) setPlaying(false);
  setStateIndex(0).catch(showError);
});
ui.slider.addEventListener("input", () => {
  if (app.playing) setPlaying(false);
  setStateIndex(Number(ui.slider.value)).catch(showError);
});
ui.resetView.addEventListener("click", frameRoot);
ui.zoomOut.addEventListener("click", () => zoomAtPoint(0.8, { x: app.width / 2, y: app.height / 2 }));
ui.zoomIn.addEventListener("click", () => zoomAtPoint(1.25, { x: app.width / 2, y: app.height / 2 }));
ui.saveLayout.addEventListener("click", () => saveLayout().catch(showError));

document.addEventListener("keydown", (event) => {
  const tag = document.activeElement ? document.activeElement.tagName : "";
  if (tag === "INPUT" || tag === "SELECT") return;
  if (event.code === "Space") {
    event.preventDefault();
    setPlaying(!app.playing);
  } else if (event.code === "ArrowLeft") {
    event.preventDefault();
    step(-1);
  } else if (event.code === "ArrowRight") {
    event.preventDefault();
    step(1);
  } else if ((event.ctrlKey || event.metaKey) && event.code === "KeyS") {
    event.preventDefault();
    saveLayout().catch(showError);
  }
});

window.addEventListener("resize", resizeCanvas);

(async function init() {
  try {
    resizeCanvas();
    const selected = await loadScenarios();
    await loadCircuit(selected);
    requestAnimationFrame(render);
  } catch (error) {
    showError(error);
  }
}());
