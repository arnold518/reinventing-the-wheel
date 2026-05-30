"use strict";

const STATE_COLORS = {
  "1": [76, 175, 80],
  "0": [211, 47, 47],
  "X": [158, 158, 158],
  "Z": [3, 155, 229],
  mixed: [255, 193, 7],
};

const DEFAULT_COLOR = [61, 90, 128, 190];
const WIRE_STROKE_PIN_RATIO = 0.18;
const WIRE_STROKE_MAX_PX = 4;
const WIRE_HOVER_STROKE_MULTIPLIER = 1.7;
const WIRE_HOVER_STROKE_MIN_PX = 2;
const WIRE_HOVER_STROKE_MAX_PX = 7;
const WIRE_HIGHLIGHT_HALO_MIN_PX = 5;
const WIRE_HIGHLIGHT_HALO_EXTRA_PX = 3;
const WIRE_HIGHLIGHT_HALO_COLOR = "rgba(255, 255, 255, 0.55)";
const WIRE_HIT_PADDING_PX = 5;
const BUS_SLASH_LENGTH_STROKE_RATIO = 4;
const BUS_SLASH_MAX_ROUTE_RATIO = 0.2;
const SCREEN_CLIP_MARGIN_PX = 64;
const MAX_DIRECT_CANVAS_SIZE_PX = 100000;
const MAX_COMPONENT_FONT_PX = 96;
const TITLE_TEXT_PADDING_PX = 6;
const COMPONENT_TITLE_MIN_FONT_PX = 6;
const PIN_LABEL_MIN_FONT_PX = 6;
const COMPONENT_STROKE_BASE_PX = 1.5;
const COMPONENT_STROKE_ACTIVE_PX = 3.5;
const COMPONENT_STROKE_ACTIVE_MIN_PX = 1;
const COMPONENT_STROKE_MIN_PX = 0.35;
const COMPONENT_STROKE_MIN_ALPHA = 0.16;
const COMPONENT_STROKE_MAX_ALPHA = 0.78;
const COMPONENT_STROKE_FADE_START_PX = 6;
const COMPONENT_STROKE_FULL_SIZE_PX = 96;
const COMPONENT_TITLE_SEPARATOR_START_PX = 42;
const GRID_TARGET_PIXELS = 100;
const GRID_BACKGROUND_MINOR = "rgb(58, 64, 72)";
const GRID_BACKGROUND_MAJOR = "rgb(86, 96, 108)";
const GRID_COMPONENT_MINOR = "rgba(255, 255, 255, 0.08)";
const GRID_COMPONENT_MAJOR = "rgba(255, 255, 255, 0.16)";
const GRID_COMPONENT_MIN_SCREEN_SIZE = 36;
const ROOT_RECT = { x: 50, y: 150, width: 800 };

const canvas = document.getElementById("circuit-canvas");
const ctx = canvas.getContext("2d", { alpha: false });
const tooltip = document.getElementById("tooltip");
const loading = document.getElementById("loading");

const ui = {
  scenario: document.getElementById("scenario-select"),
  slider: document.getElementById("time-slider"),
  checkpointMarks: document.getElementById("checkpoint-marks"),
  playPause: document.getElementById("play-pause"),
  checkpointBack: document.getElementById("checkpoint-back"),
  stepBack: document.getElementById("step-back"),
  stepForward: document.getElementById("step-forward"),
  checkpointForward: document.getElementById("checkpoint-forward"),
  resetSim: document.getElementById("reset-sim"),
  timeLabel: document.getElementById("time-label"),
  statsLabel: document.getElementById("stats-label"),
  resetView: document.getElementById("reset-view"),
  zoomOut: document.getElementById("zoom-out"),
  zoomIn: document.getElementById("zoom-in"),
  undoLayout: document.getElementById("undo-layout"),
  redoLayout: document.getElementById("redo-layout"),
  resetLayout: document.getElementById("reset-layout"),
  saveLayout: document.getElementById("save-layout"),
  saveStatus: document.getElementById("save-status"),
  inspector: document.getElementById("inspector"),
  inspectorTitle: document.getElementById("inspector-title"),
  inspectorSubtitle: document.getElementById("inspector-subtitle"),
  inspectorPath: document.getElementById("inspector-path"),
  inspectorClose: document.getElementById("inspector-close"),
  componentAspect: document.getElementById("component-aspect"),
  componentAspectValue: document.getElementById("component-aspect-value"),
  componentColor: document.getElementById("component-color"),
  componentColorCode: document.getElementById("component-color-code"),
  selectionInfoSection: document.getElementById("selection-info-section"),
  selectionInfo: document.getElementById("selection-info"),
  componentPinsSection: document.getElementById("component-pins-section"),
  componentPins: document.getElementById("component-pins"),
  typeStyleSection: document.getElementById("type-style-section"),
  layoutSection: document.getElementById("layout-section"),
  restoreTypeStyle: document.getElementById("restore-type-style"),
  restorePlacement: document.getElementById("restore-placement"),
  restoreChildrenLayout: document.getElementById("restore-children-layout"),
};

const app = {
  dpr: 1,
  width: 1,
  height: 1,
  scenario: null,
  rootId: null,
  timestamps: [0],
  checkpoints: [],
  layout: null,
  savedLayout: null,
  settings: {},
  stats: null,
  components: new Map(),
  pins: new Map(),
  wires: new Map(),
  componentOrder: [],
  pinOrder: [],
  wireOrder: [],
  wiresByOwner: new Map(),
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
  selection: null,
  selectedComponentId: null,
  interaction: null,
  geometryDirty: false,
  layoutDirty: false,
  layoutHistory: {
    undo: [],
    redo: [],
    transactionBefore: null,
    transactionScope: null,
    limit: 100,
  },
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

function intersectRect(a, b) {
  const x = Math.max(a.x, b.x);
  const y = Math.max(a.y, b.y);
  const right = Math.min(a.x + a.w, b.x + b.w);
  const bottom = Math.min(a.y + a.h, b.y + b.h);
  if (right < x || bottom < y) return null;
  return { x, y, w: right - x, h: bottom - y };
}

function padRect(rect, amount) {
  return { x: rect.x - amount, y: rect.y - amount, w: rect.w + amount * 2, h: rect.h + amount * 2 };
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

function screenClipBounds(margin = SCREEN_CLIP_MARGIN_PX) {
  return {
    minX: -margin,
    minY: -margin,
    maxX: app.width + margin,
    maxY: app.height + margin,
  };
}

function rectToScreenRect(rect) {
  const topLeft = worldToScreen({ x: rect.x, y: rect.y });
  return {
    x: topLeft.x,
    y: topLeft.y,
    w: rect.w * app.camera.zoom,
    h: rect.h * app.camera.zoom,
  };
}

function clipScreenRect(rect, margin = SCREEN_CLIP_MARGIN_PX) {
  if (!Number.isFinite(rect.x) || !Number.isFinite(rect.y) || !Number.isFinite(rect.w) || !Number.isFinite(rect.h)) {
    return null;
  }
  const bounds = screenClipBounds(margin);
  const left = Math.max(bounds.minX, Math.min(rect.x, rect.x + rect.w));
  const right = Math.min(bounds.maxX, Math.max(rect.x, rect.x + rect.w));
  const top = Math.max(bounds.minY, Math.min(rect.y, rect.y + rect.h));
  const bottom = Math.min(bounds.maxY, Math.max(rect.y, rect.y + rect.h));
  if (right < left || bottom < top) return null;
  return { x: left, y: top, w: right - left, h: bottom - top };
}

function zoomAtPoint(factor, screenPoint) {
  const before = screenToWorld(screenPoint);
  const nextZoom = app.camera.zoom * factor;
  if (!Number.isFinite(nextZoom) || nextZoom <= 0) return;
  app.camera.zoom = nextZoom;
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

function componentLayoutKey(component) {
  return component.layoutType || component.type;
}

function getTypeLayoutFor(component) {
  const typeLayouts = app.layout.type_layouts || {};
  const baseLayout = typeLayouts[component.type] || {};
  const layoutKey = componentLayoutKey(component);
  if (layoutKey === component.type) return baseLayout;
  return mergeLayout(baseLayout, typeLayouts[layoutKey] || {});
}

function getLayoutFor(component) {
  if (component.depth === 0) {
    return getRootLayoutFor(component);
  }
  return getTypeLayoutFor(component);
}

function mergeLayout(base, override) {
  return {
    ...(base || {}),
    ...(override || {}),
    children: {
      ...(((base || {}).children) || {}),
      ...(((override || {}).children) || {}),
    },
  };
}

function getRootLayoutFor(component) {
  const typeLayout = getTypeLayoutFor(component);
  const rootLayout = (app.layout.root_layouts && app.layout.root_layouts[app.scenario]) || {};
  return mergeLayout(typeLayout, rootLayout);
}

function cloneJson(value) {
  return JSON.parse(JSON.stringify(value));
}

function layoutEquals(left, right) {
  return JSON.stringify(left || {}) === JSON.stringify(right || {});
}

function updateLayoutControls() {
  ui.undoLayout.disabled = app.layoutHistory.undo.length === 0;
  ui.redoLayout.disabled = app.layoutHistory.redo.length === 0;
}

function syncLayoutDirty(statusText = null) {
  app.layoutDirty = app.savedLayout ? !layoutEquals(app.layout, app.savedLayout) : Boolean(app.layout);
  if (statusText !== null) {
    ui.saveStatus.textContent = statusText;
  } else if (app.layoutDirty) {
    ui.saveStatus.textContent = "Unsaved layout";
  } else {
    ui.saveStatus.textContent = "";
  }
  updateLayoutControls();
}

function resetLayoutHistory() {
  app.layoutHistory.undo = [];
  app.layoutHistory.redo = [];
  app.layoutHistory.transactionBefore = null;
  app.layoutHistory.transactionScope = null;
  updateLayoutControls();
}

function pushUndoSnapshot(snapshot) {
  if (!snapshot || !app.layout || layoutEquals(snapshot, app.layout)) return false;
  const history = app.layoutHistory;
  if (!history.undo.length || !layoutEquals(history.undo[history.undo.length - 1], snapshot)) {
    history.undo.push(snapshot);
    if (history.undo.length > history.limit) history.undo.shift();
  }
  history.redo = [];
  updateLayoutControls();
  return true;
}

function beginLayoutTransaction(scope) {
  const history = app.layoutHistory;
  if (history.transactionBefore && history.transactionScope !== scope) {
    commitLayoutTransaction();
  }
  if (!history.transactionBefore) {
    history.transactionBefore = cloneJson(app.layout);
    history.transactionScope = scope;
  }
}

function commitLayoutTransaction(statusText = "Unsaved layout") {
  const history = app.layoutHistory;
  if (!history.transactionBefore) {
    syncLayoutDirty();
    return false;
  }
  const before = history.transactionBefore;
  history.transactionBefore = null;
  history.transactionScope = null;
  const changed = pushUndoSnapshot(before);
  syncLayoutDirty(changed ? statusText : null);
  return changed;
}

function recordLayoutChange(statusText, mutator) {
  commitLayoutTransaction();
  const before = cloneJson(app.layout);
  mutator();
  if (pushUndoSnapshot(before)) syncLayoutDirty(statusText);
  else syncLayoutDirty();
}

function applyLayoutSnapshot(snapshot, statusText) {
  app.layout = cloneJson(snapshot);
  app.settings = app.layout.default_settings || {};
  app.geometryDirty = true;
  app.hover = null;
  app.interaction = null;
  syncLayoutDirty(statusText);
  updateInspector();
}

function undoLayoutChange() {
  commitLayoutTransaction();
  const history = app.layoutHistory;
  if (!history.undo.length) return;
  const current = cloneJson(app.layout);
  let previous = history.undo.pop();
  while (previous && layoutEquals(previous, app.layout) && history.undo.length) {
    previous = history.undo.pop();
  }
  if (!previous || layoutEquals(previous, app.layout)) {
    updateLayoutControls();
    return;
  }
  history.redo.push(current);
  if (history.redo.length > history.limit) history.redo.shift();
  applyLayoutSnapshot(previous, "Layout undone");
  updateLayoutControls();
}

function redoLayoutChange() {
  commitLayoutTransaction();
  const history = app.layoutHistory;
  if (!history.redo.length) return;
  const current = cloneJson(app.layout);
  let next = history.redo.pop();
  while (next && layoutEquals(next, app.layout) && history.redo.length) {
    next = history.redo.pop();
  }
  if (!next || layoutEquals(next, app.layout)) {
    updateLayoutControls();
    return;
  }
  history.undo.push(current);
  if (history.undo.length > history.limit) history.undo.shift();
  applyLayoutSnapshot(next, "Layout redone");
  updateLayoutControls();
}

function migrateLegacyLayoutPayload(payload) {
  app.layout.type_layouts = app.layout.type_layouts || {};
  app.layout.root_layouts = app.layout.root_layouts || {};

  const legacyRootLayout = payload.rootId
    && app.layout.instance_layouts
    && app.layout.instance_layouts[payload.rootId];
  if (legacyRootLayout && !app.layout.root_layouts[payload.scenario]) {
    app.layout.root_layouts[payload.scenario] = cloneJson(legacyRootLayout);
  }
}

function typeLayoutEntryFor(componentType) {
  if (!app.layout.type_layouts) app.layout.type_layouts = {};
  if (!app.layout.type_layouts[componentType]) app.layout.type_layouts[componentType] = {};
  return app.layout.type_layouts[componentType];
}

function colorToHex(color) {
  const rgb = color || DEFAULT_COLOR;
  return `#${rgb.slice(0, 3).map((value) => clamp(Math.round(Number(value) || 0), 0, 255).toString(16).padStart(2, "0")).join("")}`;
}

function hexToRgb(hex) {
  const match = /^#?([0-9a-f]{6})$/i.exec(hex || "");
  if (!match) return null;
  const raw = match[1];
  return [0, 2, 4].map((index) => parseInt(raw.slice(index, index + 2), 16));
}

function normalizeHexColor(value) {
  const match = /^#?([0-9a-f]{6})$/i.exec((value || "").trim());
  return match ? `#${match[1].toLowerCase()}` : null;
}

function updateColorInputs(hex) {
  ui.componentColor.value = hex;
  ui.componentColorCode.value = hex;
  ui.componentColorCode.classList.remove("invalid");
}

function getChildLayout(parent, child) {
  const parentLayout = getLayoutFor(parent);
  return (((parentLayout || {}).children || {})[child.name]) || {};
}

function childEntryFor(parent, child) {
  if (parent.depth === 0) {
    if (!app.layout.root_layouts) app.layout.root_layouts = {};
    if (!app.layout.root_layouts[app.scenario]) app.layout.root_layouts[app.scenario] = {};
    const parentLayout = app.layout.root_layouts[app.scenario];
    if (!parentLayout.children) parentLayout.children = {};
    if (!parentLayout.children[child.name]) parentLayout.children[child.name] = {};
    return parentLayout.children[child.name];
  }

  const parentLayoutKey = componentLayoutKey(parent);
  if (!app.layout.type_layouts[parentLayoutKey]) app.layout.type_layouts[parentLayoutKey] = {};
  const parentLayout = app.layout.type_layouts[parentLayoutKey];
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

function requireChildLayout(parent, child) {
  const layout = getChildLayout(parent, child);
  const relPos = layout.rel_pos;
  const relWidth = layout.rel_width;
  const validRelPos = Array.isArray(relPos)
    && relPos.length >= 2
    && Number.isFinite(Number(relPos[0]))
    && Number.isFinite(Number(relPos[1]));
  const validRelWidth = relWidth != null && Number.isFinite(Number(relWidth));
  if (!validRelPos || !validRelWidth) {
    const scope = parent.depth === 0 ? `root_layouts.${app.scenario}` : `type_layouts.${componentLayoutKey(parent)}`;
    throw new Error(`Missing layout for ${scope}.children.${child.name}; reload to let the server regenerate layout.json.`);
  }
  return layout;
}

function componentSettings(component) {
  component.titleBarRatio = Number(app.settings.title_bar_ratio ?? 0.15);
  component.fontWidthRatio = Number(app.settings.font_width_ratio ?? 0.18);
  component.boundaryRatio = Number(app.settings.boundary_area_ratio ?? 0.15);
  component.pinSizeRatio = Number(app.settings.pin_size_ratio ?? 0.10);
}

function minAspectFor(component) {
  const value = Number(component.minAspectRatio ?? 0.1);
  return Number.isFinite(value) && value > 0 ? value : 0.1;
}

function aspectFor(component, rawValue, fallback) {
  const value = Number(rawValue);
  return Math.max(minAspectFor(component), Number.isFinite(value) && value > 0 ? value : fallback);
}

function computeGeometry() {
  if (!app.rootId || !app.layout) return;

  app.componentOrder.forEach((component) => componentSettings(component));

  const root = app.components.get(app.rootId);
  const rootLayout = getLayoutFor(root);
  const rootWidth = ROOT_RECT.width;
  const rootAspect = aspectFor(root, rootLayout.aspect_ratio ?? root.baseAspectRatio ?? root.aspectRatio, 0.75);
  const rootBaseColor = root.baseColor || root.color || DEFAULT_COLOR;
  root.aspectRatio = rootAspect;
  root.color = rootLayout.color || rootBaseColor;
  root.rect = {
    x: ROOT_RECT.x,
    y: ROOT_RECT.y,
    w: rootWidth,
    h: rootWidth * rootAspect,
  };

  function layoutChildren(parent) {
    for (const childId of parent.childIds) {
      const child = app.components.get(childId);
      if (!child) continue;
      const childOwnLayout = getLayoutFor(child);
      const childLayout = requireChildLayout(parent, child);
      child.aspectRatio = aspectFor(child, childOwnLayout.aspect_ratio ?? child.baseAspectRatio ?? child.aspectRatio, 1.0);
      child.color = childOwnLayout.color || child.baseColor || child.color || DEFAULT_COLOR;
      const relPos = childLayout.rel_pos;
      const relWidth = Number(childLayout.rel_width);
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
  if (!pin || !pin.rect) return 0;
  return Math.max(0, Math.min(pin.rect.w, pin.rect.h) * WIRE_STROKE_PIN_RATIO);
}

function connectedWireStrokeWorld(source, sinks) {
  const endpointStrokes = [source, ...sinks].map(pinWireStrokeWorld).filter((width) => Number.isFinite(width));
  if (endpointStrokes.length === 0) return 0;
  return Math.min(...endpointStrokes);
}

function wireStrokePx(segment, hovered) {
  const strokeWorld = Number(segment.strokeWorld);
  const basePx = Math.max(0, Number.isFinite(strokeWorld) ? strokeWorld : 0) * app.camera.zoom;
  const scaledPx = hovered ? basePx * WIRE_HOVER_STROKE_MULTIPLIER : basePx;
  const maxPx = hovered ? WIRE_HOVER_STROKE_MAX_PX : WIRE_STROKE_MAX_PX;
  const cappedPx = Math.min(scaledPx, maxPx);
  return hovered ? Math.max(WIRE_HOVER_STROKE_MIN_PX, cappedPx) : cappedPx;
}

function samePathPoint(a, b) {
  return a.x === b.x && a.y === b.y;
}

function dedupePath(path) {
  const result = [];
  for (const point of path) {
    if (result.length === 0 || !samePathPoint(point, result[result.length - 1])) {
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

function buildWirePaths(wire) {
  wire.paths = [];
  wire.branches = [];
  wire.bbox = null;
  const source = wire.sourcePinId ? app.pins.get(wire.sourcePinId) : null;
  const sinks = wire.sinkPins.filter((sink) => sink && sink.rect);
  if (!source || sinks.length === 0) return;
  const strokeWorld = connectedWireStrokeWorld(source, sinks);

  const [srcPinPoint, srcBoundaryPoint] = activeStub(source, true);
  const sourceStub = [srcPinPoint, srcBoundaryPoint];
  wire.paths.push({ kind: "stub", points: sourceStub, bbox: pointsBBox(sourceStub), strokeWorld });
  wire.bbox = unionRect(wire.bbox, pointsBBox(sourceStub));

  for (const sink of sinks) {
    const [dstPinPoint, dstBoundaryPoint] = activeStub(sink, false);
    const sinkStub = [dstBoundaryPoint, dstPinPoint];
    const sinkBBox = pointsBBox(sinkStub);
    wire.paths.push({ kind: "stub", points: sinkStub, bbox: sinkBBox, strokeWorld });
    wire.bbox = unionRect(wire.bbox, sinkBBox);

    const route = routeAdaptive(srcBoundaryPoint, dstBoundaryPoint);
    const routeBBox = pointsBBox(route);
    wire.paths.push({ kind: "route", points: route, bbox: routeBBox, strokeWorld });
    wire.bbox = unionRect(wire.bbox, routeBBox);

    const branch = dedupePath([...sourceStub, ...route.slice(1), ...sinkStub.slice(1)]);
    wire.branches.push({ points: branch, routePoints: route, bbox: pointsBBox(branch), strokeWorld });
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

function currentGridSpacing() {
  const worldUnitsPerLine = GRID_TARGET_PIXELS / app.camera.zoom;
  if (!Number.isFinite(worldUnitsPerLine) || worldUnitsPerLine <= 0) return;
  let powerOfTen = 10 ** Math.floor(Math.log10(worldUnitsPerLine));
  let gridSpacing = powerOfTen;
  if (worldUnitsPerLine / powerOfTen < 0.5) gridSpacing = powerOfTen / 2;
  if (worldUnitsPerLine / powerOfTen > 2) gridSpacing = powerOfTen * 2;
  return Number.isFinite(gridSpacing) && gridSpacing > 0 ? gridSpacing : null;
}

function drawGridLines(worldRect, minorColor, majorColor, lineWidth = 1, clipWorldRect = null) {
  const step = currentGridSpacing();
  if (!step || !worldRect || worldRect.w <= 0 || worldRect.h <= 0) return;

  const xStart = Math.floor(worldRect.x / step) * step;
  const yStart = Math.floor(worldRect.y / step) * step;
  const xEnd = worldRect.x + worldRect.w + step;
  const yEnd = worldRect.y + worldRect.h + step;
  if (!Number.isFinite(xStart) || !Number.isFinite(yStart) || !Number.isFinite(xEnd) || !Number.isFinite(yEnd)) return;

  if (clipWorldRect) {
    const clipRect = clipScreenRect(rectToScreenRect(clipWorldRect), 0);
    if (!clipRect) return;
    ctx.save();
    ctx.beginPath();
    ctx.rect(clipRect.x, clipRect.y, clipRect.w, clipRect.h);
    ctx.clip();
  }

  ctx.lineWidth = lineWidth;
  ctx.lineCap = "butt";
  for (let x = xStart; x <= xEnd; x += step) {
    const start = worldToScreen({ x, y: worldRect.y });
    const end = worldToScreen({ x, y: worldRect.y + worldRect.h });
    const gridIndex = Math.round(x / step);
    ctx.strokeStyle = Math.abs(gridIndex % 10) === 0 ? majorColor : minorColor;
    ctx.lineWidth = Math.abs(gridIndex % 10) === 0 ? Math.max(1, lineWidth + 0.5) : lineWidth;
    ctx.beginPath();
    ctx.moveTo(start.x, start.y);
    ctx.lineTo(end.x, end.y);
    ctx.stroke();
  }

  for (let y = yStart; y <= yEnd; y += step) {
    const start = worldToScreen({ x: worldRect.x, y });
    const end = worldToScreen({ x: worldRect.x + worldRect.w, y });
    const gridIndex = Math.round(y / step);
    ctx.strokeStyle = Math.abs(gridIndex % 10) === 0 ? majorColor : minorColor;
    ctx.lineWidth = Math.abs(gridIndex % 10) === 0 ? Math.max(1, lineWidth + 0.5) : lineWidth;
    ctx.beginPath();
    ctx.moveTo(start.x, start.y);
    ctx.lineTo(end.x, end.y);
    ctx.stroke();
  }

  if (clipWorldRect) ctx.restore();
}

function drawGrid() {
  drawGridLines(viewRect(), GRID_BACKGROUND_MINOR, GRID_BACKGROUND_MAJOR);
}

function drawComponentGrid(component, vr) {
  const visibleRect = intersectRect(component.rect, vr);
  if (!visibleRect) return;
  const screenRect = rectToScreenRect(component.rect);
  if (Math.abs(screenRect.w) < GRID_COMPONENT_MIN_SCREEN_SIZE || Math.abs(screenRect.h) < GRID_COMPONENT_MIN_SCREEN_SIZE) return;
  drawGridLines(visibleRect, GRID_COMPONENT_MINOR, GRID_COMPONENT_MAJOR, 1, component.rect);
}

function strokePolyline(points, color, lineWidth) {
  if (points.length < 2 || lineWidth <= 0) return;
  const screenPoints = points.map(worldToScreen);
  const margin = Math.max(SCREEN_CLIP_MARGIN_PX, lineWidth * 2);
  ctx.strokeStyle = color;
  ctx.lineWidth = lineWidth;
  ctx.lineCap = "square";
  ctx.lineJoin = "miter";

  let hasVisibleSegment = false;
  ctx.beginPath();
  for (let i = 0; i < screenPoints.length - 1; i += 1) {
    const clipped = clipScreenLine(screenPoints[i], screenPoints[i + 1], margin);
    if (!clipped) continue;
    ctx.moveTo(clipped[0].x, clipped[0].y);
    ctx.lineTo(clipped[1].x, clipped[1].y);
    hasVisibleSegment = true;
  }
  if (hasVisibleSegment) ctx.stroke();
}

function strokeWirePolyline(points, color, lineWidth, highlighted) {
  if (highlighted) {
    const haloWidth = Math.max(WIRE_HIGHLIGHT_HALO_MIN_PX, lineWidth + WIRE_HIGHLIGHT_HALO_EXTRA_PX);
    strokePolyline(points, WIRE_HIGHLIGHT_HALO_COLOR, haloWidth);
  }
  strokePolyline(points, color, lineWidth);
}

function clipScreenLine(start, end, margin = SCREEN_CLIP_MARGIN_PX) {
  if (!Number.isFinite(start.x) || !Number.isFinite(start.y) || !Number.isFinite(end.x) || !Number.isFinite(end.y)) {
    return null;
  }

  const bounds = screenClipBounds(margin);
  const LEFT = 1;
  const RIGHT = 2;
  const TOP = 4;
  const BOTTOM = 8;

  function code(point) {
    let result = 0;
    if (point.x < bounds.minX) result |= LEFT;
    else if (point.x > bounds.maxX) result |= RIGHT;
    if (point.y < bounds.minY) result |= TOP;
    else if (point.y > bounds.maxY) result |= BOTTOM;
    return result;
  }

  let x0 = start.x;
  let y0 = start.y;
  let x1 = end.x;
  let y1 = end.y;
  let code0 = code({ x: x0, y: y0 });
  let code1 = code({ x: x1, y: y1 });

  while (true) {
    if ((code0 | code1) === 0) return [{ x: x0, y: y0 }, { x: x1, y: y1 }];
    if ((code0 & code1) !== 0) return null;

    const outside = code0 || code1;
    let x = 0;
    let y = 0;
    if (outside & TOP) {
      x = x0 + ((x1 - x0) * (bounds.minY - y0)) / (y1 - y0);
      y = bounds.minY;
    } else if (outside & BOTTOM) {
      x = x0 + ((x1 - x0) * (bounds.maxY - y0)) / (y1 - y0);
      y = bounds.maxY;
    } else if (outside & RIGHT) {
      y = y0 + ((y1 - y0) * (bounds.maxX - x0)) / (x1 - x0);
      x = bounds.maxX;
    } else if (outside & LEFT) {
      y = y0 + ((y1 - y0) * (bounds.minX - x0)) / (x1 - x0);
      x = bounds.minX;
    }

    if (!Number.isFinite(x) || !Number.isFinite(y)) return null;
    if (outside === code0) {
      x0 = x;
      y0 = y;
      code0 = code({ x: x0, y: y0 });
    } else {
      x1 = x;
      y1 = y;
      code1 = code({ x: x1, y: y1 });
    }
  }
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

function pathLengthWorld(path) {
  let total = 0;
  for (let i = 0; i < path.length - 1; i += 1) {
    total += Math.hypot(path[i + 1].x - path[i].x, path[i + 1].y - path[i].y);
  }
  return total;
}

function busSlashLengthWorld(strokeWorld, routeLength) {
  const targetLength = Math.max(0, strokeWorld || 0) * BUS_SLASH_LENGTH_STROKE_RATIO;
  const routeCap = Math.max(0, routeLength || 0) * BUS_SLASH_MAX_ROUTE_RATIO;
  return routeCap > 0 ? Math.min(targetLength, routeCap) : 0;
}

function drawBusSlash(worldPoint, color, lineWidth, slashLength, highlighted) {
  if (slashLength <= 0) return;
  const half = slashLength / 2;
  strokeWirePolyline([
    { x: worldPoint.x - half, y: worldPoint.y + half },
    { x: worldPoint.x + half, y: worldPoint.y - half },
  ], color, lineWidth, highlighted);
}

function drawBusAnnotations(wire, path, color, lineWidth, strokeWorld, highlighted) {
  if (wire.width <= 1) return;
  const slashLength = busSlashLengthWorld(strokeWorld, pathLengthWorld(path));
  drawBusSlash(pointAtFraction(path, 0.33), color, lineWidth, slashLength, highlighted);
  drawBusSlash(pointAtFraction(path, 0.66), color, lineWidth, slashLength, highlighted);
}

function isHovered(type, id) {
  return app.hover && app.hover.type === type && app.hover.id === id;
}

function isSelectedComponent(id) {
  return app.selection && app.selection.type === "component" && app.selection.id === id;
}

function isSelectedPin(id) {
  return app.selection && app.selection.type === "pin" && app.selection.id === id;
}

function isSelectedWire(id) {
  return app.selection && app.selection.type === "wire" && app.selection.id === id;
}

function drawWire(wire, vr, forceHighlight = false) {
  if (!wire.paths || !wire.bbox) return;
  const color = `rgb(${valueColor(app.state.wires[wire.id] || "X").join(",")})`;
  const highlighted = forceHighlight || isHovered("wire", wire.id) || isSelectedWire(wire.id);
  const wireCullMargin = (WIRE_HOVER_STROKE_MAX_PX + SCREEN_CLIP_MARGIN_PX) / app.camera.zoom;
  if (!rectIntersects(padRect(wire.bbox, wireCullMargin), vr)) return;
  const drawSegments = wire.branches && wire.branches.length ? wire.branches : wire.paths;
  for (const segment of drawSegments) {
    const lineWidth = wireStrokePx(segment, highlighted);
    const segmentCullMargin = (lineWidth / 2 + SCREEN_CLIP_MARGIN_PX) / app.camera.zoom;
    if (!rectIntersects(padRect(segment.bbox, segmentCullMargin), vr)) continue;
    strokeWirePolyline(segment.points, color, lineWidth, highlighted);
    if (segment.routePoints) drawBusAnnotations(wire, segment.routePoints, color, lineWidth, segment.strokeWorld, highlighted);
    else if (segment.kind === "route") drawBusAnnotations(wire, segment.points, color, lineWidth, segment.strokeWorld, highlighted);
  }
}

function drawOwnedWires(ownerId, vr) {
  const wires = app.wiresByOwner.get(ownerId) || [];
  for (const wire of wires) drawWire(wire, vr);
}

function drawWireOverlay(vr) {
  for (const wire of app.wireOrder) {
    if (isHovered("wire", wire.id) || isSelectedWire(wire.id)) drawWire(wire, vr, true);
  }
}

function drawRoundedRect(rect, fillStyle, strokeStyle, lineWidth) {
  const screenRect = rectToScreenRect(rect);
  const clipped = clipScreenRect(screenRect);
  if (!clipped) return;

  const canDrawDirectly = Math.abs(screenRect.w) <= MAX_DIRECT_CANVAS_SIZE_PX
    && Math.abs(screenRect.h) <= MAX_DIRECT_CANVAS_SIZE_PX
    && Math.abs(screenRect.x) <= MAX_DIRECT_CANVAS_SIZE_PX
    && Math.abs(screenRect.y) <= MAX_DIRECT_CANVAS_SIZE_PX;

  if (!canDrawDirectly) {
    if (fillStyle) {
      ctx.fillStyle = fillStyle;
      ctx.fillRect(clipped.x, clipped.y, clipped.w, clipped.h);
    }
    if (strokeStyle && lineWidth > 0) drawVisibleRectEdges(screenRect, strokeStyle, lineWidth);
    return;
  }

  const radius = Math.min(5, Math.abs(screenRect.w) / 2, Math.abs(screenRect.h) / 2);
  ctx.beginPath();
  if (ctx.roundRect) {
    ctx.roundRect(screenRect.x, screenRect.y, screenRect.w, screenRect.h, radius);
  } else {
    ctx.rect(screenRect.x, screenRect.y, screenRect.w, screenRect.h);
  }
  if (fillStyle) {
    ctx.fillStyle = fillStyle;
    ctx.fill();
  }
  if (strokeStyle && lineWidth > 0) {
    ctx.strokeStyle = strokeStyle;
    ctx.lineWidth = lineWidth;
    ctx.stroke();
  }
}

function drawVisibleRectEdges(screenRect, strokeStyle, lineWidth) {
  const bounds = screenClipBounds(SCREEN_CLIP_MARGIN_PX);
  const left = screenRect.x;
  const right = screenRect.x + screenRect.w;
  const top = screenRect.y;
  const bottom = screenRect.y + screenRect.h;
  ctx.strokeStyle = strokeStyle;
  ctx.lineWidth = lineWidth;
  ctx.beginPath();

  if (top >= bounds.minY && top <= bounds.maxY && right >= bounds.minX && left <= bounds.maxX) {
    ctx.moveTo(clamp(left, bounds.minX, bounds.maxX), top);
    ctx.lineTo(clamp(right, bounds.minX, bounds.maxX), top);
  }
  if (bottom >= bounds.minY && bottom <= bounds.maxY && right >= bounds.minX && left <= bounds.maxX) {
    ctx.moveTo(clamp(left, bounds.minX, bounds.maxX), bottom);
    ctx.lineTo(clamp(right, bounds.minX, bounds.maxX), bottom);
  }
  if (left >= bounds.minX && left <= bounds.maxX && bottom >= bounds.minY && top <= bounds.maxY) {
    ctx.moveTo(left, clamp(top, bounds.minY, bounds.maxY));
    ctx.lineTo(left, clamp(bottom, bounds.minY, bounds.maxY));
  }
  if (right >= bounds.minX && right <= bounds.maxX && bottom >= bounds.minY && top <= bounds.maxY) {
    ctx.moveTo(right, clamp(top, bounds.minY, bounds.maxY));
    ctx.lineTo(right, clamp(bottom, bounds.minY, bounds.maxY));
  }

  ctx.stroke();
}

function ellipsizeText(text, maxWidth) {
  if (!text || maxWidth <= 0) return "";
  if (ctx.measureText(text).width <= maxWidth) return text;

  const ellipsis = "...";
  if (ctx.measureText(ellipsis).width > maxWidth) return "";

  let left = 0;
  let right = text.length;
  while (left < right) {
    const mid = Math.ceil((left + right) / 2);
    if (ctx.measureText(`${text.slice(0, mid)}${ellipsis}`).width <= maxWidth) left = mid;
    else right = mid - 1;
  }
  return `${text.slice(0, left)}${ellipsis}`;
}

function clippedRectText(text, screenRect, fontSize, color, align = "center") {
  if (fontSize < 1 || screenRect.w <= TITLE_TEXT_PADDING_PX * 2 || screenRect.h <= 0) return;
  const clipped = clipScreenRect(screenRect, 0);
  if (!clipped) return;

  ctx.save();
  ctx.beginPath();
  ctx.rect(clipped.x, clipped.y, clipped.w, clipped.h);
  ctx.clip();
  ctx.font = `${Math.floor(fontSize)}px Arial, Helvetica, sans-serif`;
  const fitted = ellipsizeText(text, Math.max(0, screenRect.w - TITLE_TEXT_PADDING_PX * 2));
  if (fitted) {
    ctx.fillStyle = color;
    ctx.textAlign = align;
    ctx.textBaseline = "middle";
    let x = screenRect.x + screenRect.w / 2;
    if (align === "left") x = screenRect.x + TITLE_TEXT_PADDING_PX;
    if (align === "right") x = screenRect.x + screenRect.w - TITLE_TEXT_PADDING_PX;
    ctx.fillText(fitted, x, screenRect.y + screenRect.h / 2);
  }
  ctx.restore();
}

function polygonCentroid(points) {
  if (!points.length) return { x: 0, y: 0 };
  const total = points.reduce((sum, point) => ({ x: sum.x + point.x, y: sum.y + point.y }), { x: 0, y: 0 });
  return { x: total.x / points.length, y: total.y / points.length };
}

function contrastingTextColor(rgb) {
  const luminance = 0.299 * rgb[0] + 0.587 * rgb[1] + 0.114 * rgb[2];
  return luminance > 150 ? "rgb(18, 24, 32)" : "rgb(245, 245, 245)";
}

function drawTextInsidePolygon(text, points, maxWidth, fontSize, color) {
  if (!text || points.length < 3 || maxWidth <= 0 || fontSize < PIN_LABEL_MIN_FONT_PX) return;
  ctx.save();
  ctx.font = `${Math.floor(fontSize)}px Arial, Helvetica, sans-serif`;
  const fitted = ellipsizeText(text, maxWidth);
  if (fitted) {
    ctx.beginPath();
    points.forEach((point, index) => {
      if (index === 0) ctx.moveTo(point.x, point.y);
      else ctx.lineTo(point.x, point.y);
    });
    ctx.closePath();
    ctx.clip();
    const center = polygonCentroid(points);
    ctx.fillStyle = color;
    ctx.textAlign = "center";
    ctx.textBaseline = "middle";
    ctx.fillText(fitted, center.x, center.y);
  }
  ctx.restore();
}

function componentStrokeScale() {
  return Math.min(1, Math.sqrt(Math.max(0.0001, app.camera.zoom)));
}

function componentScreenSizeFactor(component) {
  if (!component || !component.rect) return 1;
  const screenRect = rectToScreenRect(component.rect);
  const size = Math.min(Math.abs(screenRect.w), Math.abs(screenRect.h));
  return clamp(
    (size - COMPONENT_STROKE_FADE_START_PX) / (COMPONENT_STROKE_FULL_SIZE_PX - COMPONENT_STROKE_FADE_START_PX),
    0,
    1,
  );
}

function componentStrokeStyle(component, selected, hovered) {
  if (selected) return "rgb(255, 193, 7)";
  if (hovered) return "rgba(238, 244, 255, 0.95)";
  const factor = componentScreenSizeFactor(component);
  if (factor <= 0) return null;
  const alpha = clamp(
    COMPONENT_STROKE_MIN_ALPHA + factor * componentStrokeScale() * (COMPONENT_STROKE_MAX_ALPHA - COMPONENT_STROKE_MIN_ALPHA),
    COMPONENT_STROKE_MIN_ALPHA,
    COMPONENT_STROKE_MAX_ALPHA,
  );
  return `rgba(238, 244, 255, ${alpha})`;
}

function componentStrokeWidth(component, selected, hovered) {
  const scale = componentStrokeScale();
  if (selected || hovered) {
    return clamp(COMPONENT_STROKE_ACTIVE_PX * scale, COMPONENT_STROKE_ACTIVE_MIN_PX, COMPONENT_STROKE_ACTIVE_PX);
  }
  const factor = componentScreenSizeFactor(component);
  if (factor <= 0) return 0;
  const weightedFactor = 0.35 + factor * 0.65;
  return Math.max(COMPONENT_STROKE_MIN_PX, COMPONENT_STROKE_BASE_PX * scale * weightedFactor);
}

function componentTitleSeparatorWidth(component, selected, hovered, strokeWidth) {
  if (!component || !component.rect || strokeWidth <= 0) return 0;
  const screenRect = rectToScreenRect(component.rect);
  const size = Math.min(Math.abs(screenRect.w), Math.abs(screenRect.h));
  const factor = selected || hovered
    ? clamp((size - COMPONENT_STROKE_FADE_START_PX) / (COMPONENT_TITLE_SEPARATOR_START_PX - COMPONENT_STROKE_FADE_START_PX), 0, 1)
    : clamp((size - COMPONENT_TITLE_SEPARATOR_START_PX) / (COMPONENT_STROKE_FULL_SIZE_PX - COMPONENT_TITLE_SEPARATOR_START_PX), 0, 1);
  return strokeWidth * factor * 0.75;
}

function drawWorldHorizontalLine(x1, x2, y, strokeStyle, lineWidth) {
  if (!strokeStyle || lineWidth <= 0) return;
  const start = worldToScreen({ x: x1, y });
  const end = worldToScreen({ x: x2, y });
  const bounds = screenClipBounds(SCREEN_CLIP_MARGIN_PX);
  if (start.y < bounds.minY || start.y > bounds.maxY) return;
  if (Math.max(start.x, end.x) < bounds.minX || Math.min(start.x, end.x) > bounds.maxX) return;
  ctx.strokeStyle = strokeStyle;
  ctx.lineWidth = lineWidth;
  ctx.beginPath();
  ctx.moveTo(clamp(start.x, bounds.minX, bounds.maxX), start.y);
  ctx.lineTo(clamp(end.x, bounds.minX, bounds.maxX), end.y);
  ctx.stroke();
}

function componentDrawParts(component) {
  const titleHeight = component.rect.w * component.titleBarRatio;
  const fullRect = { x: component.rect.x, y: component.rect.y, w: component.rect.w, h: component.rect.h };
  const titleRect = { x: component.rect.x, y: component.rect.y, w: component.rect.w, h: titleHeight };
  const bodyRect = { x: component.rect.x, y: component.rect.y + titleHeight, w: component.rect.w, h: Math.max(0, component.rect.h - titleHeight) };
  const baseColor = component.color || DEFAULT_COLOR;
  const hoverAlpha = Math.max(baseColor[3] ?? DEFAULT_COLOR[3], 220);
  const fillColor = isHovered("component", component.id) ? rgba([baseColor[0], baseColor[1], baseColor[2], hoverAlpha]) : rgba(baseColor);

  const selected = isSelectedComponent(component.id);
  const hovered = isHovered("component", component.id);
  const strokeStyle = componentStrokeStyle(component, selected, hovered);
  const strokeWidth = componentStrokeWidth(component, selected, hovered);
  return {
    fullRect,
    titleRect,
    bodyRect,
    baseColor,
    fillColor,
    titleFillColor: rgba(darken(baseColor, 20)),
    strokeStyle,
    strokeWidth,
    titleSeparatorWidth: componentTitleSeparatorWidth(component, selected, hovered, strokeWidth),
  };
}

function drawComponentBackground(component, vr) {
  if (!component.rect || !rectIntersects(component.rect, vr)) return false;
  const parts = componentDrawParts(component);
  drawRoundedRect(parts.bodyRect, parts.fillColor, null, 0);
  drawRoundedRect(parts.titleRect, parts.titleFillColor, null, 0);
  return true;
}

function drawComponentForeground(component, vr) {
  if (!component.rect || !rectIntersects(component.rect, vr)) return;
  const parts = componentDrawParts(component);
  drawRoundedRect(parts.fullRect, null, parts.strokeStyle, parts.strokeWidth);
  drawWorldHorizontalLine(
    parts.titleRect.x,
    parts.titleRect.x + parts.titleRect.w,
    parts.titleRect.y + parts.titleRect.h,
    parts.strokeStyle,
    parts.titleSeparatorWidth,
  );

  const fontSize = Math.min(component.rect.w * component.fontWidthRatio * app.camera.zoom, MAX_COMPONENT_FONT_PX);
  if (fontSize >= COMPONENT_TITLE_MIN_FONT_PX) {
    clippedRectText(component.name, rectToScreenRect(parts.titleRect), fontSize, "rgb(240, 240, 240)");
  }

  for (const pin of [...component.inputPins, ...component.outputPins]) drawPin(pin);
}

function drawComponent(component, vr) {
  if (!drawComponentBackground(component, vr)) return;
  drawComponentGrid(component, vr);
  drawOwnedWires(component.id, vr);

  for (const childId of component.childIds) {
    const child = app.components.get(childId);
    if (child) drawComponent(child, vr);
  }

  drawComponentForeground(component, vr);
}

function drawPin(pin) {
  const center = worldToScreen(pin.pos);
  if (!Number.isFinite(center.x) || !Number.isFinite(center.y)) return;
  const screenPoints = pin.relPoints.map((rel) => ({
    x: center.x + rel.x * app.camera.zoom,
    y: center.y + rel.y * app.camera.zoom,
  }));
  const pinScreenBounds = screenPoints.reduce(
    (bounds, point) => ({
      minX: Math.min(bounds.minX, point.x),
      minY: Math.min(bounds.minY, point.y),
      maxX: Math.max(bounds.maxX, point.x),
      maxY: Math.max(bounds.maxY, point.y),
    }),
    { minX: Infinity, minY: Infinity, maxX: -Infinity, maxY: -Infinity },
  );
  const pinScreenRect = {
    x: pinScreenBounds.minX,
    y: pinScreenBounds.minY,
    w: pinScreenBounds.maxX - pinScreenBounds.minX,
    h: pinScreenBounds.maxY - pinScreenBounds.minY,
  };
  const clippedPinRect = clipScreenRect(pinScreenRect);
  if (!clippedPinRect) return;

  const color = `rgb(${valueColor(app.state.pins[pin.id] || "X").join(",")})`;
  ctx.fillStyle = color;
  const stateColor = valueColor(app.state.pins[pin.id] || "X");
  if (pinScreenRect.w > MAX_DIRECT_CANVAS_SIZE_PX || pinScreenRect.h > MAX_DIRECT_CANVAS_SIZE_PX) {
    ctx.fillRect(clippedPinRect.x, clippedPinRect.y, clippedPinRect.w, clippedPinRect.h);
  } else {
    ctx.beginPath();
    screenPoints.forEach((point, index) => {
      if (index === 0) ctx.moveTo(point.x, point.y);
      else ctx.lineTo(point.x, point.y);
    });
    ctx.closePath();
    ctx.fill();
  }
  if ((isHovered("pin", pin.id) || isSelectedPin(pin.id)) && pinScreenRect.w <= MAX_DIRECT_CANVAS_SIZE_PX && pinScreenRect.h <= MAX_DIRECT_CANVAS_SIZE_PX) {
    ctx.strokeStyle = "rgb(255, 255, 0)";
    ctx.lineWidth = 2;
    ctx.stroke();
  }

  if (pinScreenRect.w <= MAX_DIRECT_CANVAS_SIZE_PX && pinScreenRect.h <= MAX_DIRECT_CANVAS_SIZE_PX) {
    const label = pin.width === 1 ? pin.name : `${pin.name}[${pin.width}]`;
    const fontSize = Math.floor(Math.min(
      pinScreenRect.h * 0.26,
      pinScreenRect.w * 0.22,
    ));
    drawTextInsidePolygon(
      label,
      screenPoints,
      pinScreenRect.w * 0.58,
      fontSize,
      contrastingTextColor(stateColor),
    );
  }
}

function render() {
  resizeCanvas();
  if (app.geometryDirty) computeGeometry();

  ctx.fillStyle = "rgb(30, 30, 30)";
  ctx.fillRect(0, 0, app.width, app.height);
  drawGrid();

  const vr = viewRect();
  const root = app.components.get(app.rootId);
  if (root) drawComponent(root, vr);
  drawWireOverlay(vr);

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
  const maxStrokePx = wire.paths.reduce((maxWidth, segment) => Math.max(maxWidth, wireStrokePx(segment, false)), 0);
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

function selectedComponent() {
  return app.selection && app.selection.type === "component" ? app.components.get(app.selection.id) : null;
}

function selectedPin() {
  return app.selection && app.selection.type === "pin" ? app.pins.get(app.selection.id) : null;
}

function selectedWire() {
  return app.selection && app.selection.type === "wire" ? app.wires.get(app.selection.id) : null;
}

function setSelection(type, id) {
  if (app.selection && (app.selection.type !== type || app.selection.id !== id)) {
    commitLayoutTransaction();
  }
  app.selection = type && id ? { type, id } : null;
  app.selectedComponentId = type === "component" ? id : null;
  updateInspector();
}

function setSelectedComponent(componentId) {
  setSelection(componentId ? "component" : null, componentId);
}

function componentPath(component) {
  const names = [];
  const seen = new Set();
  let current = component;
  while (current && !seen.has(current.id)) {
    seen.add(current.id);
    names.push(current.name);
    current = current.parentId ? app.components.get(current.parentId) : null;
  }
  return names.reverse().join(".");
}

function pinLabel(pin) {
  return pin.width === 1 ? pin.name : `${pin.name}[${pin.width}]`;
}

function pinValue(pin) {
  return app.state.pins[pin.id] || "X";
}

function wireValue(wire) {
  return app.state.wires[wire.id] || "X";
}

function pinPath(pin) {
  const component = app.components.get(pin.componentId);
  return component ? `${componentPath(component)}.${pin.name}` : pin.id;
}

function wirePath(wire) {
  const owner = app.components.get(wire.ownerId);
  return owner ? `${componentPath(owner)}.${wire.name}` : wire.id;
}

function setSectionVisible(section, visible) {
  section.classList.toggle("hidden", !visible);
}

function setInfoRows(container, rows) {
  container.textContent = "";
  rows.forEach(([label, value]) => {
    const labelNode = document.createElement("div");
    labelNode.className = "inspector-key";
    labelNode.textContent = label;
    const valueNode = document.createElement("div");
    valueNode.className = "inspector-data";
    valueNode.textContent = value == null || value === "" ? "-" : String(value);
    container.append(labelNode, valueNode);
  });
}

function pinEndpointSummary(pinId) {
  const pin = app.pins.get(pinId);
  return pin ? pinPath(pin) : pinId || "-";
}

function wireEndpointSummary(wire) {
  const sinks = (wire.sinkPinIds || []).map(pinEndpointSummary);
  return {
    source: pinEndpointSummary(wire.sourcePinId),
    sinks: sinks.length ? sinks.join("\n") : "-",
  };
}

function renderPinGroup(title, pins) {
  const group = document.createElement("div");
  group.className = "pin-group";
  const heading = document.createElement("div");
  heading.className = "pin-group-title";
  heading.textContent = title;
  group.appendChild(heading);
  if (!pins.length) {
    const empty = document.createElement("div");
    empty.className = "inspector-value";
    empty.textContent = "-";
    group.appendChild(empty);
    return group;
  }
  pins.forEach((pin) => {
    const row = document.createElement("button");
    row.type = "button";
    row.className = "pin-row";
    row.title = pinPath(pin);
    row.addEventListener("click", () => setSelection("pin", pin.id));

    const name = document.createElement("span");
    name.className = "pin-row-name";
    name.textContent = pinLabel(pin);
    const value = document.createElement("span");
    value.className = "pin-row-value";
    value.textContent = pinValue(pin);
    row.append(name, value);
    group.appendChild(row);
  });
  return group;
}

function renderComponentPins(component) {
  ui.componentPins.textContent = "";
  ui.componentPins.append(
    renderPinGroup("Inputs", component.inputPins),
    renderPinGroup("Outputs", component.outputPins),
  );
}

function showComponentInspector(component) {
  const layoutKey = componentLayoutKey(component);
  const typeLayout = getLayoutFor(component);
  const color = typeLayout.color || component.baseColor || component.color || DEFAULT_COLOR;
  const minAspect = minAspectFor(component);
  const aspect = aspectFor(component, typeLayout.aspect_ratio ?? component.baseAspectRatio ?? component.aspectRatio, 1);
  const path = componentPath(component);

  ui.inspectorTitle.textContent = component.name;
  ui.inspectorSubtitle.textContent = layoutKey === component.type
    ? `${component.type} | depth ${component.depth}`
    : `${component.type} | layout ${layoutKey} | depth ${component.depth}`;
  ui.inspectorPath.textContent = path;
  ui.inspectorPath.title = path;
  setSectionVisible(ui.selectionInfoSection, false);
  setSectionVisible(ui.componentPinsSection, true);
  setSectionVisible(ui.typeStyleSection, true);
  setSectionVisible(ui.layoutSection, true);
  renderComponentPins(component);

  ui.componentAspect.min = String(minAspect);
  ui.componentAspect.max = String(Math.max(3, Math.ceil(aspect * 20) / 20));
  ui.componentAspect.value = String(aspect);
  ui.componentAspectValue.textContent = `Aspect: ${aspect.toFixed(2)} min ${minAspect.toFixed(2)}`;
  updateColorInputs(colorToHex(color));
  ui.restorePlacement.disabled = !component.parentId;
  ui.restoreChildrenLayout.disabled = component.childIds.length === 0;
  ui.restoreTypeStyle.disabled = !app.savedLayout;
}

function showPinInspector(pin) {
  const component = app.components.get(pin.componentId);
  const sourceWires = pin.sourceWireIds || [];
  const sinkWires = pin.sinkWireIds || [];

  ui.inspectorTitle.textContent = pinLabel(pin);
  ui.inspectorSubtitle.textContent = `${pin.type} pin | value ${pinValue(pin)}`;
  ui.inspectorPath.textContent = pinPath(pin);
  ui.inspectorPath.title = pinPath(pin);
  setSectionVisible(ui.selectionInfoSection, true);
  setSectionVisible(ui.componentPinsSection, false);
  setSectionVisible(ui.typeStyleSection, false);
  setSectionVisible(ui.layoutSection, false);
  setInfoRows(ui.selectionInfo, [
    ["Value", pinValue(pin)],
    ["Width", pin.width],
    ["Direction", pin.type],
    ["Component", component ? componentPath(component) : pin.componentId],
    ["Source wires", sourceWires.length ? sourceWires.join("\n") : "-"],
    ["Sink wires", sinkWires.length ? sinkWires.join("\n") : "-"],
  ]);
}

function showWireInspector(wire) {
  const endpoints = wireEndpointSummary(wire);
  const owner = app.components.get(wire.ownerId);

  ui.inspectorTitle.textContent = wire.name;
  ui.inspectorSubtitle.textContent = `wire | value ${wireValue(wire)}`;
  ui.inspectorPath.textContent = wirePath(wire);
  ui.inspectorPath.title = wirePath(wire);
  setSectionVisible(ui.selectionInfoSection, true);
  setSectionVisible(ui.componentPinsSection, false);
  setSectionVisible(ui.typeStyleSection, false);
  setSectionVisible(ui.layoutSection, false);
  setInfoRows(ui.selectionInfo, [
    ["Value", wireValue(wire)],
    ["Width", wire.width],
    ["Owner", owner ? componentPath(owner) : wire.ownerId],
    ["Source", endpoints.source],
    ["Sinks", endpoints.sinks],
  ]);
}

function updateInspector() {
  if (!app.selection) {
    ui.inspector.classList.add("hidden");
    return;
  }

  const component = selectedComponent();
  const pin = selectedPin();
  const wire = selectedWire();
  if (component) showComponentInspector(component);
  else if (pin) showPinInspector(pin);
  else if (wire) showWireInspector(wire);
  else {
    app.selection = null;
    ui.inspector.classList.add("hidden");
    return;
  }
  ui.inspector.classList.remove("hidden");
}

function markStyleChanged() {
  app.geometryDirty = true;
  markLayoutDirty();
}

function applyAspectFromInspector() {
  const component = selectedComponent();
  if (!component) return;
  const value = Math.max(minAspectFor(component), Number(ui.componentAspect.value));
  if (!Number.isFinite(value) || value <= 0) return;
  const layoutKey = componentLayoutKey(component);
  const entry = typeLayoutEntryFor(layoutKey);
  if (Number(entry.aspect_ratio) === value) return;
  beginLayoutTransaction(`type-style:${layoutKey}`);
  entry.aspect_ratio = value;
  ui.componentAspect.value = String(value);
  ui.componentAspectValue.textContent = `Aspect: ${value.toFixed(2)} min ${minAspectFor(component).toFixed(2)}`;
  markStyleChanged();
}

function applyColorValue(hex) {
  const component = selectedComponent();
  if (!component) return;
  const normalizedHex = normalizeHexColor(hex);
  const rgb = hexToRgb(normalizedHex);
  if (!rgb) return;
  const layoutKey = componentLayoutKey(component);
  const entry = typeLayoutEntryFor(layoutKey);
  const previousColor = entry.color || component.baseColor || component.color || DEFAULT_COLOR;
  const alpha = clamp(Math.round(Number(previousColor[3] ?? DEFAULT_COLOR[3] ?? 255)), 0, 255);
  const nextColor = [...rgb, alpha];
  if (layoutEquals(entry.color, nextColor)) return;
  beginLayoutTransaction(`type-style:${layoutKey}`);
  entry.color = nextColor;
  updateColorInputs(colorToHex(entry.color));
  markStyleChanged();
}

function applyColorFromInspector() {
  applyColorValue(ui.componentColor.value);
}

function applyColorCodeFromInspector() {
  const value = ui.componentColorCode.value.trim();
  const normalizedHex = normalizeHexColor(value);
  if (!normalizedHex) {
    ui.componentColorCode.classList.toggle("invalid", value.length > 0);
    return;
  }
  applyColorValue(normalizedHex);
}

async function fetchDefaultPlacements(parent, childName = null) {
  const body = {
    scenario: app.scenario,
    parentId: parent.id,
    layout: app.layout,
  };
  if (childName) body.childName = childName;
  const response = await fetch(apiUrl("api/layout/defaults"), {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body),
  });
  if (!response.ok) throw new Error(await response.text());
  return response.json();
}

function applyPlacements(parent, placements) {
  for (const childId of parent.childIds) {
    const child = app.components.get(childId);
    if (!child || !placements[child.name]) continue;
    const entry = childEntryFor(parent, child);
    entry.rel_pos = cloneJson(placements[child.name].rel_pos);
    entry.rel_width = placements[child.name].rel_width;
  }
  app.geometryDirty = true;
  markLayoutDirty();
  updateInspector();
}

async function restoreSelectedPlacement() {
  const component = selectedComponent();
  if (!component || !component.parentId) return;
  const parent = app.components.get(component.parentId);
  if (!parent) return;
  const payload = await fetchDefaultPlacements(parent, component.name);
  recordLayoutChange("Placement restored", () => applyPlacements(parent, payload.placements || {}));
  ui.saveStatus.textContent = "Placement restored";
}

async function restoreSelectedChildrenLayout() {
  const component = selectedComponent();
  if (!component || component.childIds.length === 0) return;
  const payload = await fetchDefaultPlacements(component);
  recordLayoutChange("Children restored", () => applyPlacements(component, payload.placements || {}));
  ui.saveStatus.textContent = "Children restored";
}

function restoreSelectedTypeStyle() {
  const component = selectedComponent();
  if (!component || !app.savedLayout) return;
  recordLayoutChange("Type style restored", () => {
    const layoutKey = componentLayoutKey(component);
    const current = typeLayoutEntryFor(layoutKey);
    const saved = (app.savedLayout.type_layouts && app.savedLayout.type_layouts[layoutKey]) || {};
    if ("aspect_ratio" in saved) current.aspect_ratio = aspectFor(component, saved.aspect_ratio, 1);
    else current.aspect_ratio = aspectFor(component, component.baseAspectRatio ?? component.aspectRatio, 1);
    if ("color" in saved) current.color = cloneJson(saved.color);
    else current.color = cloneJson(component.baseColor || component.color || DEFAULT_COLOR);
    markStyleChanged();
    updateInspector();
  });
  ui.saveStatus.textContent = "Type style restored";
}

function markLayoutDirty() {
  app.layoutDirty = true;
  ui.saveStatus.textContent = "Unsaved layout";
  updateLayoutControls();
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
    return;
  }
  app.geometryDirty = true;
  markLayoutDirty();
}

function resizeComponent(component, mode, world) {
  if (!component.parentId) return;
  const parent = app.components.get(component.parentId);
  const safe = contentRect(parent);
  const old = { ...component.rect };
  let width = old.w;
  let left = old.x;
  let top = old.y;

  if (mode === "right") width = Math.max(0, world.x - old.x);
  if (mode === "left") {
    width = Math.max(0, old.x + old.w - world.x);
    left = old.x + old.w - width;
  }
  if (mode === "bottom") width = Math.max(0, (world.y - old.y) / component.aspectRatio);
  if (mode === "top") {
    width = Math.max(0, (old.y + old.h - world.y) / component.aspectRatio);
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
    setSelectedComponent(component.id);
    const border = hoveredBorder(component, world);
    if (border) {
      if (component.parentId) beginLayoutTransaction(`resize:${component.id}`);
      app.interaction = { type: "resize", componentId: component.id, mode: border };
    } else {
      if (component.parentId) beginLayoutTransaction(`drag:${component.id}`);
      app.interaction = {
        type: "drag",
        componentId: component.id,
        offset: { x: world.x - component.rect.x, y: world.y - component.rect.y },
      };
      canvas.style.cursor = "grabbing";
    }
  } else if (app.hover && (app.hover.type === "pin" || app.hover.type === "wire")) {
    setSelection(app.hover.type, app.hover.id);
  } else {
    setSelectedComponent(null);
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
  const interaction = app.interaction;
  if (interaction && (interaction.type === "drag" || interaction.type === "resize")) {
    commitLayoutTransaction();
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
  if (!Number.isFinite(app.camera.zoom) || app.camera.zoom <= 0) app.camera.zoom = 1;
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

function checkpointForIndex(index) {
  return app.checkpoints.find((checkpoint) => checkpoint.index === index) || null;
}

function updateCheckpointControls() {
  const previous = app.checkpoints.some((checkpoint) => checkpoint.index < app.requestedIndex);
  const next = app.checkpoints.some((checkpoint) => checkpoint.index > app.requestedIndex);
  ui.checkpointBack.disabled = !previous;
  ui.checkpointForward.disabled = !next;
}

function renderCheckpointMarks() {
  ui.checkpointMarks.innerHTML = "";
  for (const checkpoint of app.checkpoints) {
    const option = document.createElement("option");
    option.value = String(checkpoint.index);
    option.label = checkpoint.label;
    option.title = checkpoint.detail || checkpoint.label;
    ui.checkpointMarks.appendChild(option);
  }
  updateCheckpointControls();
}

function setTimeLabel(state) {
  const checkpoint = checkpointForIndex(state.index);
  if (checkpoint) {
    ui.timeLabel.textContent = `Time: ${state.time} | ${checkpoint.label}`;
    ui.timeLabel.title = checkpoint.detail || checkpoint.label;
  } else {
    ui.timeLabel.textContent = `Time: ${state.time}`;
    ui.timeLabel.title = "";
  }
}

async function setStateIndex(index) {
  index = Math.max(0, Math.min(index, app.timestamps.length - 1));
  app.requestedIndex = index;
  ui.slider.value = String(index);
  updateCheckpointControls();
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
    setTimeLabel(state);
    updateCheckpointControls();
    updateInspector();
  } finally {
    if (requestId === app.stateRequestId) app.stateRequestInFlight = false;
  }
}

function step(delta) {
  if (app.playing) setPlaying(false);
  setStateIndex(app.requestedIndex + delta).catch(showError);
}

function stepCheckpoint(delta) {
  if (app.playing) setPlaying(false);
  const sorted = [...app.checkpoints].sort((a, b) => a.index - b.index);
  const checkpoint = delta < 0
    ? sorted.filter((item) => item.index < app.requestedIndex).pop()
    : sorted.find((item) => item.index > app.requestedIndex);
  if (checkpoint) setStateIndex(checkpoint.index).catch(showError);
}

async function saveLayout() {
  commitLayoutTransaction();
  ui.saveStatus.textContent = "Saving...";
  const response = await fetch(apiUrl("api/layout"), {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ layout: app.layout }),
  });
  if (!response.ok) throw new Error(await response.text());
  app.savedLayout = cloneJson(app.layout);
  syncLayoutDirty("Layout saved");
  ui.saveStatus.textContent = "Layout saved";
}

function resetLayout() {
  if (!app.savedLayout) return;
  recordLayoutChange("Layout reset", () => {
    app.layout = cloneJson(app.savedLayout);
    app.settings = app.layout.default_settings || {};
    app.geometryDirty = true;
    app.hover = null;
    app.interaction = null;
    updateInspector();
  });
  ui.saveStatus.textContent = "Layout reset";
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
  app.checkpoints = (payload.checkpoints || []).filter((checkpoint) => Number.isFinite(Number(checkpoint.index)));
  app.layout = payload.layout;
  migrateLegacyLayoutPayload(payload);
  app.savedLayout = cloneJson(app.layout);
  resetLayoutHistory();
  app.settings = app.layout.default_settings || {};
  app.stats = payload.stats;
  app.state = payload.state;
  app.currentIndex = 0;
  app.requestedIndex = 0;
  app.stateRequestInFlight = false;
  app.layoutDirty = false;
  app.selection = null;
  app.selectedComponentId = null;
  app.components = new Map();
  app.pins = new Map();
  app.wires = new Map();
  app.componentOrder = [];
  app.pinOrder = [];
  app.wireOrder = [];
  app.wiresByOwner = new Map();

  for (const data of payload.components) {
    const component = {
      ...data,
      childIds: data.childIds || [],
      inputPins: [],
      outputPins: [],
      rect: null,
      color: data.color || DEFAULT_COLOR,
      baseAspectRatio: data.aspectRatio,
      baseColor: data.color || DEFAULT_COLOR,
    };
    app.components.set(component.id, component);
    app.componentOrder.push(component);
  }

  for (const data of payload.pins) {
    const pin = {
      ...data,
      rect: null,
      pos: null,
      relPoints: [],
      outerStub: [],
      innerStub: [],
      sourceWireIds: [],
      sinkWireIds: [],
    };
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
      sourcePin: data.sourcePinId ? app.pins.get(data.sourcePinId) : null,
      sinkPins: (data.sinkPinIds || []).map((id) => app.pins.get(id)).filter(Boolean),
      paths: [],
      bbox: null,
    };
    app.wires.set(wire.id, wire);
    app.wireOrder.push(wire);
    if (!app.wiresByOwner.has(wire.ownerId)) app.wiresByOwner.set(wire.ownerId, []);
    app.wiresByOwner.get(wire.ownerId).push(wire);
    if (wire.sourcePin) wire.sourcePin.sourceWireIds.push(wire.id);
    wire.sinkPins.forEach((pin) => pin.sinkWireIds.push(wire.id));
  }

  ui.slider.min = "0";
  ui.slider.max = String(Math.max(0, app.timestamps.length - 1));
  ui.slider.value = "0";
  renderCheckpointMarks();
  setTimeLabel(app.state);
  ui.statsLabel.textContent = `${payload.stats.componentCount} comps | ${payload.stats.wireCount} wires | ${payload.stats.pinCount} pins`;
  app.geometryDirty = true;
  computeGeometry();
  frameRoot();
  updateInspector();
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
ui.checkpointBack.addEventListener("click", () => stepCheckpoint(-1));
ui.stepBack.addEventListener("click", () => step(-1));
ui.stepForward.addEventListener("click", () => step(1));
ui.checkpointForward.addEventListener("click", () => stepCheckpoint(1));
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
ui.undoLayout.addEventListener("click", undoLayoutChange);
ui.redoLayout.addEventListener("click", redoLayoutChange);
ui.resetLayout.addEventListener("click", resetLayout);
ui.saveLayout.addEventListener("click", () => saveLayout().catch(showError));
ui.inspectorClose.addEventListener("click", () => setSelectedComponent(null));
ui.componentAspect.addEventListener("input", applyAspectFromInspector);
ui.componentAspect.addEventListener("change", () => commitLayoutTransaction());
ui.componentColor.addEventListener("input", applyColorFromInspector);
ui.componentColor.addEventListener("change", () => commitLayoutTransaction());
ui.componentColorCode.addEventListener("input", applyColorCodeFromInspector);
ui.componentColorCode.addEventListener("change", () => commitLayoutTransaction());
ui.componentColorCode.addEventListener("blur", () => commitLayoutTransaction());
ui.restoreTypeStyle.addEventListener("click", restoreSelectedTypeStyle);
ui.restorePlacement.addEventListener("click", () => restoreSelectedPlacement().catch(showError));
ui.restoreChildrenLayout.addEventListener("click", () => restoreSelectedChildrenLayout().catch(showError));

document.addEventListener("keydown", (event) => {
  const tag = document.activeElement ? document.activeElement.tagName : "";
  if (tag === "INPUT" || tag === "SELECT") return;
  if ((event.ctrlKey || event.metaKey) && event.code === "KeyZ") {
    event.preventDefault();
    if (event.shiftKey) redoLayoutChange();
    else undoLayoutChange();
  } else if ((event.ctrlKey || event.metaKey) && event.code === "KeyY") {
    event.preventDefault();
    redoLayoutChange();
  } else if (event.code === "Space") {
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
