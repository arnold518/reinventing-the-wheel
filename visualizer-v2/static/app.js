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
const FOCUS_PADDING_PX = 28;
const FOCUS_ANIMATION_MIN_MS = 320;
const FOCUS_ANIMATION_MAX_MS = 700;
// Descendants narrower than one physical pixel cannot add resolvable detail.
// Keep their parent shell visible and reveal the exact internals again on zoom.
const COMPONENT_DESCEND_MIN_DEVICE_PX = 1;
const GRID_TARGET_PIXELS = 100;
const GRID_BACKGROUND_MINOR = "rgb(58, 64, 72)";
const GRID_BACKGROUND_MAJOR = "rgb(86, 96, 108)";
const GRID_COMPONENT_MINOR = "rgba(255, 255, 255, 0.08)";
const GRID_COMPONENT_MAJOR = "rgba(255, 255, 255, 0.16)";
const GRID_COMPONENT_MIN_SCREEN_SIZE = 36;
const REGISTER_GRID_COLS = 4;
const REGISTER_GRID_ROWS = 8;
const MEMORY_WORD_WINDOW_ROWS = 8;
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
  topbar: document.getElementById("topbar"),
  viewbar: document.getElementById("viewbar"),
  explorerToggle: document.getElementById("toggle-explorer"),
  explorerPanel: document.getElementById("component-explorer"),
  explorerViewport: document.getElementById("explorer-viewport"),
  explorerSpacer: document.getElementById("explorer-spacer"),
  explorerRows: document.getElementById("explorer-rows"),
  explorerSearch: document.getElementById("explorer-search"),
  explorerApply: document.getElementById("apply-profile"),
  explorerRevert: document.getElementById("revert-profile"),
  explorerStatus: document.getElementById("explorer-status"),
  explorerResizer: document.getElementById("explorer-resizer"),
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
  sessionId: null,
  profile: null,
  layoutKey: null,
  rootId: null,
  timestamps: [0],
  checkpoints: [],
  layout: null,
  savedLayout: null,
  settings: {},
  stats: null,
  performanceMetrics: [],
  components: new Map(),
  pins: new Map(),
  wires: new Map(),
  componentStates: new Map(),
  componentOrder: [],
  pinOrder: [],
  wireOrder: [],
  wiresByOwner: new Map(),
  topologyEncoding: "complete-v1",
  loadedScopeIds: new Set(),
  pendingScopeIds: new Set(),
  topologyRequestInFlight: false,
  state: { pins: {}, wires: {}, index: 0, time: 0 },
  currentIndex: 0,
  requestedIndex: 0,
  stateRequestId: 0,
  stateRequestInFlight: false,
  camera: { offset: { x: 0, y: 0 }, zoom: 1 },
  cameraAnimation: null,
  playing: false,
  playbackInterval: 250,
  lastPlaybackTick: 0,
  hover: null,
  selection: null,
  selectionPath: new Set(),
  selectedComponentId: null,
  interaction: null,
  memoryScrollOffsets: new Map(),
  memoryScrollControls: [],
  geometryDirty: false,
  renderScheduled: false,
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
  explorer: null,
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
  app.cameraAnimation = null;
  const before = screenToWorld(screenPoint);
  const nextZoom = app.camera.zoom * factor;
  if (!Number.isFinite(nextZoom) || nextZoom <= 0) return;
  app.camera.zoom = nextZoom;
  const after = screenToWorld(screenPoint);
  app.camera.offset.x += before.x - after.x;
  app.camera.offset.y += before.y - after.y;
  requestRender();
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
  const specializedLayout = layoutKey === component.type
    ? baseLayout
    : mergeLayout(baseLayout, typeLayouts[layoutKey] || {});
  const fingerprint = component.profileFingerprint;
  if (!fingerprint || fingerprint === "explicit") return specializedLayout;
  const profileLayout = (((app.layout.profile_layouts || {})[fingerprint] || {})[layoutKey]) || {};
  return mergeLayout(specializedLayout, profileLayout);
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
  const rootLayout = (app.layout.root_layouts && app.layout.root_layouts[app.layoutKey]) || {};
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
  requestRender();
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

function normalizeLayoutPayload(payload) {
  app.layout.schema_version = Number(app.layout.schema_version || 2);
  app.layout.type_layouts = app.layout.type_layouts || {};
  app.layout.profile_layouts = app.layout.profile_layouts || {};
  app.layout.root_layouts = app.layout.root_layouts || {};

  const instanceRootLayout = payload.rootId
    && app.layout.instance_layouts
    && app.layout.instance_layouts[payload.rootId];
  if (instanceRootLayout && !app.layout.root_layouts[app.layoutKey]) {
    app.layout.root_layouts[app.layoutKey] = cloneJson(instanceRootLayout);
  }

  const scenarioRootLayout = app.layout.root_layouts[payload.scenario];
  if (app.layoutKey !== payload.scenario && scenarioRootLayout && !app.layout.root_layouts[app.layoutKey]) {
    app.layout.root_layouts[app.layoutKey] = cloneJson(scenarioRootLayout);
  }
}

function typeLayoutEntryFor(componentType) {
  if (!app.layout.type_layouts) app.layout.type_layouts = {};
  if (!app.layout.type_layouts[componentType]) app.layout.type_layouts[componentType] = {};
  return app.layout.type_layouts[componentType];
}

function profileLayoutEntryFor(component) {
  const fingerprint = component.profileFingerprint;
  if (!fingerprint || fingerprint === "explicit") return null;
  if (!app.layout.profile_layouts) app.layout.profile_layouts = {};
  if (!app.layout.profile_layouts[fingerprint]) app.layout.profile_layouts[fingerprint] = {};
  const profile = app.layout.profile_layouts[fingerprint];
  const layoutKey = componentLayoutKey(component);
  if (!profile[layoutKey]) profile[layoutKey] = {};
  return profile[layoutKey];
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
    if (!app.layout.root_layouts[app.layoutKey]) app.layout.root_layouts[app.layoutKey] = {};
    const parentLayout = app.layout.root_layouts[app.layoutKey];
    if (!parentLayout.children) parentLayout.children = {};
    if (!parentLayout.children[child.name]) parentLayout.children[child.name] = {};
    return parentLayout.children[child.name];
  }

  const profileLayout = profileLayoutEntryFor(parent);
  if (profileLayout) {
    if (!profileLayout.children) profileLayout.children = {};
    if (!profileLayout.children[child.name]) profileLayout.children[child.name] = {};
    return profileLayout.children[child.name];
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
    const fingerprint = parent.profileFingerprint;
    const scope = parent.depth === 0
      ? `root_layouts.${app.layoutKey}`
      : fingerprint && fingerprint !== "explicit"
        ? `profile_layouts.${fingerprint}.${componentLayoutKey(parent)}`
        : `type_layouts.${componentLayoutKey(parent)}`;
    throw new Error(`Missing layout for ${scope}.children.${child.name}; reload to let the server regenerate layout.json.`);
  }
  return layout;
}

function componentSettings(component) {
  const layout = getLayoutFor(component);
  component.titleBarRatio = Number(layout.title_bar_ratio ?? app.settings.title_bar_ratio ?? 0.15);
  component.fontWidthRatio = Number(layout.font_width_ratio ?? app.settings.font_width_ratio ?? 0.18);
  component.boundaryRatio = Number(layout.boundary_area_ratio ?? app.settings.boundary_area_ratio ?? 0.15);
  component.pinSizeRatio = Number(layout.pin_size_ratio ?? app.settings.pin_size_ratio ?? 0.10);
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

function componentCanDescend(component) {
  if (!component || !component.rect) return false;
  const projectedWidth = Math.abs(
    component.rect.w * app.camera.zoom * app.dpr,
  );
  return projectedWidth >= COMPONENT_DESCEND_MIN_DEVICE_PX
    || app.selectionPath.has(component.id);
}

function visibleGeometryComponents() {
  const root = app.components.get(app.rootId);
  if (!root || !root.rect) return [];
  const visible = [];
  const viewport = viewRect();

  const walk = (component) => {
    if (!component || !component.rect) return;
    if (
      !rectIntersects(component.rect, viewport)
      && !app.selectionPath.has(component.id)
    ) {
      return;
    }
    visible.push(component);
    if (!componentCanDescend(component)) return;
    for (const childId of component.childIds) {
      walk(app.components.get(childId));
    }
  };
  walk(root);
  return visible;
}

function rebuildSpatialIndexes() {
  app.componentIndex = new SpatialHash(260);
  app.pinIndex = new SpatialHash(180);
  app.wireIndex = new SpatialHash(260);

  visibleGeometryComponents().forEach((component, index) => {
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
  const bounds = canvas.getBoundingClientRect();
  const nextWidth = Math.max(1, Math.round(bounds.width));
  const nextHeight = Math.max(1, Math.round(bounds.height));
  const nextDpr = Math.max(1, window.devicePixelRatio || 1);
  if (nextWidth === app.width && nextHeight === app.height && nextDpr === app.dpr) return;

  app.width = nextWidth;
  app.height = nextHeight;
  app.dpr = nextDpr;
  canvas.width = Math.floor(nextWidth * nextDpr);
  canvas.height = Math.floor(nextHeight * nextDpr);
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
  const color = `rgb(${valueColor(wireValue(wire)).join(",")})`;
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
  const overlayIds = new Set();
  if (app.hover && app.hover.type === "wire") overlayIds.add(app.hover.id);
  if (app.selection && app.selection.type === "wire") overlayIds.add(app.selection.id);
  for (const wireId of overlayIds) {
    const wire = app.wires.get(wireId);
    if (wire) drawWire(wire, vr, true);
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

const BASIC_LOGIC_GATE_VISUALS = {
  ANDGate: "and",
  NANDGate: "nand",
  ORGate: "or",
  NORGate: "nor",
  XORGate: "xor",
  NOTGate: "not",
};

function basicLogicGateKind(component) {
  return BASIC_LOGIC_GATE_VISUALS[component.type] || null;
}

function pinByName(pins, name) {
  return (pins || []).find((pin) => pin.name === name) || null;
}

function parseKnownBinary(value) {
  return typeof value === "string" && /^[01]+$/.test(value) ? Number.parseInt(value, 2) : null;
}

function registerWordDisplay(register) {
  if (!register) return "--";
  if (register.hex) return register.hex.replace(/^0x/i, "").toUpperCase().padStart(8, "0");
  const bits = register.bits || "";
  if (!bits) return "--";
  if ([...bits].every((bit) => bit === "X")) return "XXXXXXXX";
  if ([...bits].every((bit) => bit === "Z")) return "ZZZZZZZZ";
  return "MIXED";
}

function signalDisplay(value) {
  if (!value) return "X";
  if (/^[01]+$/.test(value)) {
    if (value.length <= 4) return value;
    const width = Math.max(1, Math.ceil(value.length / 4));
    return `0x${Number.parseInt(value, 2).toString(16).toUpperCase().padStart(width, "0")}`;
  }
  if ([...value].every((bit) => bit === "X")) return value.length <= 4 ? "X" : "UNKNOWN";
  if ([...value].every((bit) => bit === "Z")) return value.length <= 4 ? "Z" : "HIGH-Z";
  return "MIXED";
}

function constantSignalValue(component, outPin) {
  if (component.constantValue == null || !Number.isFinite(Number(component.constantValue))) {
    return outPin ? pinValue(outPin) : "X";
  }
  const width = Math.max(1, Math.min(64, Number(outPin && outPin.width ? outPin.width : 1)));
  let value = BigInt(Math.trunc(Number(component.constantValue)));
  const bits = [];
  for (let bit = width - 1; bit >= 0; bit -= 1) {
    bits.push(((value >> BigInt(bit)) & 1n) === 1n ? "1" : "0");
  }
  return bits.join("");
}

function drawScreenRoundedRect(rect, fillStyle, strokeStyle = null, lineWidth = 0, radius = 4) {
  ctx.beginPath();
  if (ctx.roundRect) ctx.roundRect(rect.x, rect.y, rect.w, rect.h, radius);
  else ctx.rect(rect.x, rect.y, rect.w, rect.h);
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

function drawDefaultComponentBackground(parts) {
  drawRoundedRect(parts.bodyRect, parts.fillColor, null, 0);
  drawRoundedRect(parts.titleRect, parts.titleFillColor, null, 0);
}

function drawDefaultComponentForeground(component, parts) {
  drawRoundedRect(parts.fullRect, null, parts.strokeStyle, parts.strokeWidth);
  drawWorldHorizontalLine(
    parts.titleRect.x,
    parts.titleRect.x + parts.titleRect.w,
    parts.titleRect.y + parts.titleRect.h,
    parts.strokeStyle,
    parts.titleSeparatorWidth,
  );

  const titleScreenRect = rectToScreenRect(parts.titleRect);
  const fontSize = Math.min(
    component.rect.w * component.fontWidthRatio * app.camera.zoom,
    titleScreenRect.h * 0.68,
    MAX_COMPONENT_FONT_PX,
  );
  if (fontSize >= COMPONENT_TITLE_MIN_FONT_PX) {
    clippedRectText(component.name, titleScreenRect, fontSize, "rgb(240, 240, 240)");
  }
}

function drawScreenGatePath(fillStyle, strokeStyle, lineWidth, buildPath) {
  ctx.save();
  ctx.fillStyle = fillStyle;
  ctx.strokeStyle = strokeStyle;
  ctx.lineWidth = lineWidth;
  ctx.lineJoin = "round";
  ctx.lineCap = "round";
  ctx.beginPath();
  buildPath();
  ctx.fill();
  ctx.stroke();
  ctx.restore();
}

function drawGateTerminalLine(pin, from, to) {
  if (!pin || !from || !to) return;
  const strokeWorld = pinWireStrokeWorld(pin);
  const highlighted = isHovered("pin", pin.id) || isSelectedPin(pin.id);
  const lineWidth = wireStrokePx({ strokeWorld }, highlighted);
  const color = `rgb(${valueColor(pinValue(pin)).join(",")})`;
  strokeWirePolyline([from, to], color, lineWidth, highlighted);
}

function pinInnerContactPoint(pin) {
  return pin.innerStub && pin.innerStub.length ? pin.innerStub[0] : pin.pos;
}

function cubicBezierPoint(p0, p1, p2, p3, t) {
  const u = 1 - t;
  const uu = u * u;
  const tt = t * t;
  return {
    x: uu * u * p0.x + 3 * uu * t * p1.x + 3 * u * tt * p2.x + tt * t * p3.x,
    y: uu * u * p0.y + 3 * uu * t * p1.y + 3 * u * tt * p2.y + tt * t * p3.y,
  };
}

function cubicBezierXAtY(p0, p1, p2, p3, targetY) {
  const minY = Math.min(p0.y, p3.y);
  const maxY = Math.max(p0.y, p3.y);
  const clampedY = clamp(targetY, minY, maxY);
  const ascending = p3.y > p0.y;
  let lo = 0;
  let hi = 1;
  for (let i = 0; i < 28; i += 1) {
    const mid = (lo + hi) / 2;
    const point = cubicBezierPoint(p0, p1, p2, p3, mid);
    if (ascending ? point.y < clampedY : point.y > clampedY) lo = mid;
    else hi = mid;
  }
  return cubicBezierPoint(p0, p1, p2, p3, (lo + hi) / 2).x;
}

function drawLogicGateSymbol(component, parts, kind) {
  const screenRect = rectToScreenRect(parts.bodyRect);
  const clipped = clipScreenRect(screenRect);
  if (!clipped || screenRect.w < 16 || screenRect.h < 12) return;

  const padX = Math.max(3, Math.abs(screenRect.w) * 0.16);
  const padY = Math.max(3, Math.abs(screenRect.h) * 0.15);
  const innerLeft = screenRect.x + padX;
  const innerRight = screenRect.x + screenRect.w - padX;
  const aspectRatio = Number(component.aspectRatio);
  const symbolWidthScale = Number.isFinite(aspectRatio) && aspectRatio > 0 ? Math.min(1, aspectRatio) : 1;
  const innerWidth = Math.max(1, innerRight - innerLeft);
  const symbolWidth = Math.max(1, innerWidth * symbolWidthScale);
  const symbolCenterX = (innerLeft + innerRight) / 2;
  const left = symbolCenterX - symbolWidth / 2;
  const right = symbolCenterX + symbolWidth / 2;
  const top = screenRect.y + padY;
  const bottom = screenRect.y + screenRect.h - padY;
  const midY = (top + bottom) / 2;
  const width = Math.max(1, right - left);
  const height = Math.max(1, bottom - top);
  const bubbleRadius = clamp(Math.min(width, height) * 0.075, 2.5, 8);
  const strokeStyle = parts.strokeStyle || "rgba(238, 244, 255, 0.72)";
  const lineWidth = Math.max(1.25, parts.strokeWidth || 1.25);
  const symbolFill = "rgba(10, 18, 30, 0.92)";

  const inputPins = component.inputPins || [];
  const outputPins = component.outputPins || [];
  const symbolLayers = [];
  let inputEndpointX = () => left;
  let outputEndpointX = () => right;
  const pinScreenY = (pin) => worldToScreen(pin.pos).y;
  const terminalPoint = (x, pin) => screenToWorld({ x, y: pinScreenY(pin) });

  if (kind === "not") {
    const triRight = right - bubbleRadius * 2.4;
    inputEndpointX = () => left;
    outputEndpointX = () => triRight + bubbleRadius * 2;
    symbolLayers.push(() => {
      drawScreenGatePath(symbolFill, strokeStyle, lineWidth, () => {
        ctx.moveTo(left, top);
        ctx.lineTo(left, bottom);
        ctx.lineTo(triRight, midY);
        ctx.closePath();
      });
    });
    symbolLayers.push(() => {
      ctx.save();
      ctx.strokeStyle = strokeStyle;
      ctx.lineWidth = lineWidth;
      ctx.beginPath();
      ctx.arc(triRight + bubbleRadius, midY, bubbleRadius, 0, Math.PI * 2);
      ctx.stroke();
      ctx.restore();
    });
  } else {
    const hasBubble = kind === "nand" || kind === "nor";
    const shapeRight = hasBubble ? right - bubbleRadius * 2.4 : right;
    const gateKind = kind === "nand" ? "and" : kind === "nor" ? "or" : kind;
    let bubbleCenterX = shapeRight + bubbleRadius;

    if (gateKind === "and") {
      const shoulder = left + width * 0.42;
      const roundEndCenterX = shoulder * 0.25 + shapeRight * 0.75;
      symbolLayers.push(() => {
        drawScreenGatePath(symbolFill, strokeStyle, lineWidth, () => {
          ctx.moveTo(left, top);
          ctx.lineTo(shoulder, top);
          ctx.bezierCurveTo(shapeRight, top, shapeRight, bottom, shoulder, bottom);
          ctx.lineTo(left, bottom);
          ctx.closePath();
        });
      });
      inputEndpointX = () => left;
      outputEndpointX = () => roundEndCenterX;
      bubbleCenterX = roundEndCenterX + bubbleRadius;
    } else if (gateKind === "or" || gateKind === "xor") {
      const inputTerminalX = left + width * 0.18;
      const inputCurve = [
        { x: inputTerminalX, y: bottom },
        { x: left + width * 0.42, y: bottom - height * 0.26 },
        { x: left + width * 0.42, y: top + height * 0.26 },
        { x: inputTerminalX, y: top },
      ];
      symbolLayers.push(() => {
        drawScreenGatePath(symbolFill, strokeStyle, lineWidth, () => {
          ctx.moveTo(inputTerminalX, top);
          ctx.bezierCurveTo(left + width * 0.72, top, shapeRight, top + height * 0.12, shapeRight, midY);
          ctx.bezierCurveTo(shapeRight, bottom - height * 0.12, left + width * 0.72, bottom, inputTerminalX, bottom);
          ctx.bezierCurveTo(inputCurve[1].x, inputCurve[1].y, inputCurve[2].x, inputCurve[2].y, inputCurve[3].x, inputCurve[3].y);
          ctx.closePath();
        });
      });
      if (gateKind === "xor") {
        const xorCurve = [
          { x: left, y: bottom - height * 0.04 },
          { x: left + width * 0.28, y: bottom - height * 0.29 },
          { x: left + width * 0.28, y: top + height * 0.29 },
          { x: left, y: top + height * 0.04 },
        ];
        inputEndpointX = (pin) => cubicBezierXAtY(xorCurve[0], xorCurve[1], xorCurve[2], xorCurve[3], pinScreenY(pin));
        symbolLayers.push(() => {
          ctx.save();
          ctx.strokeStyle = strokeStyle;
          ctx.lineWidth = lineWidth * 0.85;
          ctx.lineCap = "round";
          ctx.beginPath();
          ctx.moveTo(xorCurve[0].x, xorCurve[0].y);
          ctx.bezierCurveTo(xorCurve[1].x, xorCurve[1].y, xorCurve[2].x, xorCurve[2].y, xorCurve[3].x, xorCurve[3].y);
          ctx.stroke();
          ctx.restore();
        });
      } else {
        inputEndpointX = (pin) => cubicBezierXAtY(inputCurve[0], inputCurve[1], inputCurve[2], inputCurve[3], pinScreenY(pin));
      }
      outputEndpointX = () => shapeRight;
    }

    if (hasBubble) {
      symbolLayers.push(() => {
        ctx.save();
        ctx.strokeStyle = strokeStyle;
        ctx.lineWidth = lineWidth;
        ctx.beginPath();
        ctx.arc(bubbleCenterX, midY, bubbleRadius, 0, Math.PI * 2);
        ctx.stroke();
        ctx.restore();
      });
    }
    if (hasBubble) outputEndpointX = () => bubbleCenterX + bubbleRadius;
  }

  for (const pin of inputPins) {
    if (!pin.pos) continue;
    drawGateTerminalLine(
      pin,
      pinInnerContactPoint(pin),
      terminalPoint(inputEndpointX(pin), pin),
    );
  }
  for (const pin of outputPins) {
    if (!pin.pos) continue;
    drawGateTerminalLine(
      pin,
      terminalPoint(outputEndpointX(pin), pin),
      pinInnerContactPoint(pin),
    );
  }
  for (const drawSymbolLayer of symbolLayers) drawSymbolLayer();
}

function adapterPinSignal(pin) {
  return pin ? pinValue(pin) : "X";
}

function signalBitAt(value, bitIndex) {
  if (!value || bitIndex == null || bitIndex < 0) return "X";
  if (value.length === 1) return bitIndex === 0 ? value[0] : "X";
  const charIndex = value.length - 1 - bitIndex;
  return charIndex >= 0 && charIndex < value.length ? value[charIndex] : "X";
}

function indexedPinNumber(pin, prefix) {
  const match = new RegExp(`^${prefix}_(\\d+)$`).exec(pin.name || "");
  return match ? Number.parseInt(match[1], 10) : null;
}

function sortedIndexedPins(pins, prefix) {
  return [...(pins || [])]
    .map((pin) => ({ pin, index: indexedPinNumber(pin, prefix) }))
    .filter((entry) => Number.isFinite(entry.index))
    .sort((a, b) => a.index - b.index)
    .map((entry) => entry.pin);
}

function pinMapByName(pins) {
  const result = new Map();
  (pins || []).forEach((pin) => result.set(pin.name, pin));
  return result;
}

function adapterStrokeWorld(component, pins) {
  const pinStrokes = (pins || [])
    .map(pinWireStrokeWorld)
    .filter((stroke) => Number.isFinite(stroke) && stroke > 0);
  if (pinStrokes.length) return Math.max(Math.min(...pinStrokes), Math.min(component.rect.w, component.rect.h) * 0.006);
  return Math.min(component.rect.w, component.rect.h) * 0.012;
}

function adapterPinsHighlighted(pins) {
  return (pins || []).some((pin) => pin && (isHovered("pin", pin.id) || isSelectedPin(pin.id)));
}

function drawAdapterPolyline(component, path, value, pins, width = 1) {
  if (!path || path.length < 2) return;
  const strokeWorld = adapterStrokeWorld(component, pins);
  const highlighted = adapterPinsHighlighted(pins);
  const lineWidth = Math.max(1, wireStrokePx({ strokeWorld }, highlighted));
  const color = `rgb(${valueColor(value).join(",")})`;
  strokeWirePolyline(path, color, lineWidth, highlighted);
  if (width > 1) drawBusAnnotations({ width }, path, color, lineWidth, strokeWorld, highlighted);
}

function drawAdapterJunction(component, point, value, pins) {
  const screen = worldToScreen(point);
  if (!Number.isFinite(screen.x) || !Number.isFinite(screen.y)) return;
  const radius = Math.max(1.6, Math.min(4.5, adapterStrokeWorld(component, pins) * app.camera.zoom * 0.9));
  ctx.beginPath();
  ctx.arc(screen.x, screen.y, radius, 0, Math.PI * 2);
  ctx.fillStyle = `rgb(${valueColor(value).join(",")})`;
  ctx.fill();
}

function drawAdapterPanel(worldRect, strokeStyle) {
  const screenRect = rectToScreenRect(worldRect);
  const clipped = clipScreenRect(screenRect, 0);
  if (!clipped || screenRect.w < 3 || screenRect.h < 3) return;
  drawScreenRoundedRect(
    screenRect,
    "rgba(8, 12, 18, 0.32)",
    strokeStyle || "rgba(238, 244, 255, 0.16)",
    Math.max(0.6, Math.min(1.4, Math.min(screenRect.w, screenRect.h) * 0.015)),
    Math.max(2, Math.min(5, Math.min(screenRect.w, screenRect.h) * 0.06)),
  );
}

function withComponentBodyClip(parts, minWidth, minHeight, draw) {
  const screenRect = rectToScreenRect(parts.bodyRect);
  const clipped = clipScreenRect(screenRect, 0);
  if (!clipped || screenRect.w < minWidth || screenRect.h < minHeight) return;
  ctx.save();
  ctx.beginPath();
  ctx.rect(clipped.x, clipped.y, clipped.w, clipped.h);
  ctx.clip();
  draw(screenRect);
  ctx.restore();
}

function drawBitSplitterContents(component, parts) {
  const inputPin = pinByName(component.inputPins, "IN") || component.inputPins[0];
  const outputPins = sortedIndexedPins(component.outputPins, "OUT");
  if (!inputPin || !outputPins.length) return;

  withComponentBodyClip(parts, 34, 24, () => {
    const body = parts.bodyRect;
    const spineX = body.x + body.w * 0.47;
    const inputValue = adapterPinSignal(inputPin);
    const inputY = inputPin.pos.y;
    const laneYs = outputPins.map((pin) => pin.pos.y);
    const topY = Math.min(inputY, ...laneYs);
    const bottomY = Math.max(inputY, ...laneYs);
    const panelPad = Math.min(body.w, body.h) * 0.035;

    drawAdapterPanel(
      {
        x: spineX - panelPad * 1.4,
        y: topY - panelPad * 1.2,
        w: panelPad * 2.8,
        h: Math.max(panelPad * 2.4, bottomY - topY + panelPad * 2.4),
      },
      "rgba(124, 197, 255, 0.22)",
    );

    drawAdapterPolyline(
      component,
      [pinInnerContactPoint(inputPin), { x: spineX, y: inputY }],
      inputValue,
      [inputPin],
      inputPin.width || 1,
    );
    if (outputPins.length > 1 || Math.abs(bottomY - topY) > 0.001) {
      drawAdapterPolyline(component, [{ x: spineX, y: topY }, { x: spineX, y: bottomY }], inputValue, [inputPin], inputPin.width || outputPins.length);
    }
    drawAdapterJunction(component, { x: spineX, y: inputY }, inputValue, [inputPin]);

    outputPins.forEach((pin) => {
      const index = indexedPinNumber(pin, "OUT");
      const laneValue = adapterPinSignal(pin) || signalBitAt(inputValue, index);
      const junction = { x: spineX, y: pin.pos.y };
      drawAdapterPolyline(component, [junction, pinInnerContactPoint(pin)], laneValue, [pin], 1);
      drawAdapterJunction(component, junction, laneValue, [pin]);
    });
  });
}

function drawBitJoinerContents(component, parts) {
  const inputPins = sortedIndexedPins(component.inputPins, "IN");
  const outputPin = pinByName(component.outputPins, "OUT") || component.outputPins[0];
  if (!inputPins.length || !outputPin) return;

  withComponentBodyClip(parts, 34, 24, () => {
    const body = parts.bodyRect;
    const spineX = body.x + body.w * 0.53;
    const outputValue = adapterPinSignal(outputPin);
    const outputY = outputPin.pos.y;
    const laneYs = inputPins.map((pin) => pin.pos.y);
    const topY = Math.min(outputY, ...laneYs);
    const bottomY = Math.max(outputY, ...laneYs);
    const panelPad = Math.min(body.w, body.h) * 0.035;

    drawAdapterPanel(
      {
        x: spineX - panelPad * 1.4,
        y: topY - panelPad * 1.2,
        w: panelPad * 2.8,
        h: Math.max(panelPad * 2.4, bottomY - topY + panelPad * 2.4),
      },
      "rgba(124, 197, 255, 0.22)",
    );

    inputPins.forEach((pin) => {
      const laneValue = adapterPinSignal(pin);
      const junction = { x: spineX, y: pin.pos.y };
      drawAdapterPolyline(component, [pinInnerContactPoint(pin), junction], laneValue, [pin], 1);
      drawAdapterJunction(component, junction, laneValue, [pin]);
    });
    if (inputPins.length > 1 || Math.abs(bottomY - topY) > 0.001) {
      drawAdapterPolyline(component, [{ x: spineX, y: topY }, { x: spineX, y: bottomY }], outputValue, [outputPin], outputPin.width || inputPins.length);
    }
    drawAdapterPolyline(
      component,
      [{ x: spineX, y: outputY }, pinInnerContactPoint(outputPin)],
      outputValue,
      [outputPin],
      outputPin.width || 1,
    );
    drawAdapterJunction(component, { x: spineX, y: outputY }, outputValue, [outputPin]);
  });
}

function rewireRouteValue(sourcePin, mappings) {
  const sourceValue = adapterPinSignal(sourcePin);
  if (!mappings || mappings.length === 0) return sourceValue;
  return [...mappings]
    .sort((a, b) => {
      if (a.dstBit !== b.dstBit) return a.dstBit - b.dstBit;
      return a.srcBit - b.srcBit;
    })
    .map((mapping) => signalBitAt(sourceValue, mapping.srcBit))
    .join("");
}

function groupedRewireMappings(component) {
  const mappings = component.rewire && Array.isArray(component.rewire.mappings) ? component.rewire.mappings : [];
  if (!mappings.length) {
    const inputs = component.inputPins || [];
    const outputs = component.outputPins || [];
    const count = Math.min(inputs.length, outputs.length);
    return Array.from({ length: count }, (_, index) => ({
      srcWire: inputs[index].name,
      dstWire: outputs[index].name,
      mappings: [],
    }));
  }

  const groups = new Map();
  mappings.forEach((mapping) => {
    const key = `${mapping.srcWire}\u0000${mapping.dstWire}`;
    if (!groups.has(key)) {
      groups.set(key, { srcWire: mapping.srcWire, dstWire: mapping.dstWire, mappings: [] });
    }
    groups.get(key).mappings.push(mapping);
  });
  return [...groups.values()].sort((a, b) => {
    if (a.srcWire !== b.srcWire) return a.srcWire.localeCompare(b.srcWire);
    return a.dstWire.localeCompare(b.dstWire);
  });
}

function drawRewireContents(component, parts) {
  const inputPins = pinMapByName(component.inputPins);
  const outputPins = pinMapByName(component.outputPins);
  const groups = groupedRewireMappings(component)
    .map((group) => ({
      ...group,
      sourcePin: inputPins.get(group.srcWire),
      outputPin: outputPins.get(group.dstWire),
    }))
    .filter((group) => group.sourcePin && group.outputPin);
  if (!groups.length) return;

  withComponentBodyClip(parts, 38, 24, () => {
    const body = parts.bodyRect;
    const leftX = body.x + body.w * 0.42;
    const rightX = body.x + body.w * 0.58;
    const allYs = groups.flatMap((group) => [group.sourcePin.pos.y, group.outputPin.pos.y]);
    const topY = Math.min(...allYs);
    const bottomY = Math.max(...allYs);
    const panelPad = Math.min(body.w, body.h) * 0.04;

    drawAdapterPanel(
      {
        x: leftX - panelPad,
        y: topY - panelPad * 1.2,
        w: Math.max(panelPad * 2, rightX - leftX + panelPad * 2),
        h: Math.max(panelPad * 2.4, bottomY - topY + panelPad * 2.4),
      },
      "rgba(255, 184, 77, 0.24)",
    );

    const sourcePins = new Map();
    const outputPinsByName = new Map();
    groups.forEach((group) => {
      sourcePins.set(group.sourcePin.name, group.sourcePin);
      outputPinsByName.set(group.outputPin.name, group.outputPin);
    });

    sourcePins.forEach((pin) => {
      const value = adapterPinSignal(pin);
      const junction = { x: leftX, y: pin.pos.y };
      drawAdapterPolyline(component, [pinInnerContactPoint(pin), junction], value, [pin], pin.width || 1);
      drawAdapterJunction(component, junction, value, [pin]);
    });

    groups.forEach((group, index) => {
      const sourceY = group.sourcePin.pos.y;
      const outputY = group.outputPin.pos.y;
      const bendX = (leftX + rightX) / 2 + (groups.length > 1 ? ((index % 3) - 1) * Math.min(body.w * 0.018, 4 / app.camera.zoom) : 0);
      const route = [
        { x: leftX, y: sourceY },
        { x: bendX, y: sourceY },
        { x: bendX, y: outputY },
        { x: rightX, y: outputY },
      ];
      const value = rewireRouteValue(group.sourcePin, group.mappings);
      const routeWidth = group.mappings.length || Math.min(group.sourcePin.width || 1, group.outputPin.width || 1);
      drawAdapterPolyline(component, route, value, [group.sourcePin, group.outputPin], routeWidth);
      drawAdapterJunction(component, { x: bendX, y: outputY }, value, [group.outputPin]);
    });

    outputPinsByName.forEach((pin) => {
      const value = adapterPinSignal(pin);
      const junction = { x: rightX, y: pin.pos.y };
      drawAdapterPolyline(component, [junction, pinInnerContactPoint(pin)], value, [pin], pin.width || 1);
      drawAdapterJunction(component, junction, value, [pin]);
    });
  });
}

function drawMemoryBitValue(component, parts) {
  const qPin = pinByName(component.outputPins, "Q");
  const value = qPin ? pinValue(qPin) : "X";
  const screenRect = rectToScreenRect(parts.bodyRect);
  const clipped = clipScreenRect(screenRect, 0);
  if (
    !clipped
    || screenRect.w < 120
    || screenRect.h < 84
  ) {
    return;
  }

  const color = valueColor(value);
  const panelWidth = screenRect.w * 0.46;
  const panelHeight = screenRect.h * 0.5;
  if (panelWidth < 72 || panelHeight < 52) return;

  const panel = {
    x: screenRect.x + (screenRect.w - panelWidth) / 2,
    y: screenRect.y + (screenRect.h - panelHeight) / 2,
    w: panelWidth,
    h: panelHeight,
  };
  ctx.save();
  ctx.beginPath();
  ctx.rect(clipped.x, clipped.y, clipped.w, clipped.h);
  ctx.clip();
  drawScreenRoundedRect(
    panel,
    `rgba(${color.join(",")}, 0.12)`,
    `rgb(${color.join(",")})`,
    Math.max(1.5, Math.min(panelWidth, panelHeight) * 0.012),
    Math.min(panelWidth, panelHeight) * 0.07,
  );
  ctx.textAlign = "center";
  ctx.textBaseline = "middle";
  ctx.font = `700 ${Math.floor(panelHeight * 0.13)}px Arial, Helvetica, sans-serif`;
  ctx.fillStyle = "rgba(238, 244, 255, 0.72)";
  ctx.fillText(
    "STORED BIT",
    panel.x + panel.w / 2,
    panel.y + panel.h * 0.24,
  );
  ctx.font = `800 ${Math.floor(
    Math.min(panelHeight * 0.52, panelWidth * 0.28)
  )}px ui-monospace, SFMono-Regular, Consolas, monospace`;
  ctx.fillStyle = `rgb(${color.join(",")})`;
  ctx.fillText(
    value[0] || "X",
    panel.x + panel.w / 2,
    panel.y + panel.h * 0.65,
  );
  ctx.restore();
}

function drawRegister32Value(component, parts) {
  const qPin = pinByName(component.outputPins, "Q");
  const value = qPin ? pinValue(qPin) : "X";
  const screenRect = rectToScreenRect(parts.bodyRect);
  const clipped = clipScreenRect(screenRect, 0);
  if (
    !clipped
    || screenRect.w < 180
    || screenRect.h < 110
  ) {
    return;
  }

  const color = valueColor(value);
  const panelWidth = screenRect.w * 0.7;
  const panelHeight = screenRect.h * 0.56;
  if (panelWidth < 126 || panelHeight < 62) return;

  const panel = {
    x: screenRect.x + (screenRect.w - panelWidth) / 2,
    y: screenRect.y + (screenRect.h - panelHeight) / 2,
    w: panelWidth,
    h: panelHeight,
  };
  const groupedBits = typeof value === "string" && value.length > 1
    ? (value.match(/.{1,8}/g) || [value]).join(" ")
    : value || "X";

  ctx.save();
  ctx.beginPath();
  ctx.rect(clipped.x, clipped.y, clipped.w, clipped.h);
  ctx.clip();
  drawScreenRoundedRect(
    panel,
    `rgba(${color.join(",")}, 0.12)`,
    `rgb(${color.join(",")})`,
    Math.max(1.5, Math.min(panelWidth, panelHeight) * 0.012),
    Math.min(panelWidth, panelHeight) * 0.07,
  );
  ctx.textAlign = "center";
  ctx.textBaseline = "middle";
  ctx.font = `700 ${Math.floor(panelHeight * 0.11)}px Arial, Helvetica, sans-serif`;
  ctx.fillStyle = "rgba(238, 244, 255, 0.72)";
  ctx.fillText(
    "STORED WORD",
    panel.x + panel.w / 2,
    panel.y + panel.h * 0.2,
  );
  ctx.font = `800 ${Math.floor(
    Math.min(panelHeight * 0.31, panelWidth * 0.12)
  )}px ui-monospace, SFMono-Regular, Consolas, monospace`;
  ctx.fillStyle = `rgb(${color.join(",")})`;
  ctx.fillText(
    signalDisplay(value),
    panel.x + panel.w / 2,
    panel.y + panel.h * 0.51,
  );
  ctx.font = `600 ${Math.floor(
    Math.min(panelHeight * 0.12, panelWidth * 0.036)
  )}px ui-monospace, SFMono-Regular, Consolas, monospace`;
  ctx.fillStyle = "rgba(238, 244, 255, 0.68)";
  ctx.fillText(
    groupedBits,
    panel.x + panel.w / 2,
    panel.y + panel.h * 0.79,
  );
  ctx.restore();
}

function drawConstantValueContents(component, parts) {
  const outPin = pinByName(component.outputPins, "OUT");
  const value = constantSignalValue(component, outPin);
  const screenRect = rectToScreenRect(parts.bodyRect);
  const clipped = clipScreenRect(screenRect, 0);
  if (!clipped || screenRect.w < 32 || screenRect.h < 18) return;

  const display = signalDisplay(value);
  const fontSize = Math.floor(Math.min(screenRect.h * 0.48, screenRect.w * 0.14, 72));
  if (fontSize < 7) return;

  ctx.save();
  ctx.beginPath();
  ctx.rect(clipped.x, clipped.y, clipped.w, clipped.h);
  ctx.clip();
  ctx.font = `800 ${fontSize}px ui-monospace, SFMono-Regular, Consolas, monospace`;
  ctx.textAlign = "center";
  ctx.textBaseline = "middle";
  ctx.lineJoin = "round";
  ctx.strokeStyle = "rgba(8, 12, 18, 0.82)";
  ctx.lineWidth = Math.max(2, fontSize * 0.08);
  ctx.strokeText(display, screenRect.x + screenRect.w / 2, screenRect.y + screenRect.h / 2);
  ctx.fillStyle = `rgb(${valueColor(value).join(",")})`;
  ctx.fillText(display, screenRect.x + screenRect.w / 2, screenRect.y + screenRect.h / 2);
  ctx.restore();
}

function drawClockGeneratorContents(component, parts) {
  const clkPin = pinByName(component.outputPins, "CLK_OUT");
  const value = clkPin ? pinValue(clkPin) : "X";
  const screenRect = rectToScreenRect(parts.bodyRect);
  const clipped = clipScreenRect(screenRect, 0);
  if (!clipped || screenRect.w < 42 || screenRect.h < 24) return;

  const padX = Math.max(7, screenRect.w * 0.12);
  const padY = Math.max(5, screenRect.h * 0.18);
  const left = screenRect.x + padX;
  const right = screenRect.x + screenRect.w - padX;
  const top = screenRect.y + padY;
  const bottom = screenRect.y + screenRect.h - padY;
  const highY = top;
  const lowY = bottom;
  const color = `rgb(${valueColor(value).join(",")})`;
  const segments = 8;
  const step = (right - left) / segments;

  ctx.save();
  ctx.beginPath();
  ctx.rect(clipped.x, clipped.y, clipped.w, clipped.h);
  ctx.clip();
  ctx.strokeStyle = color;
  ctx.lineWidth = Math.max(1.5, Math.min(4, Math.min(screenRect.w, screenRect.h) * 0.045));
  ctx.lineJoin = "round";
  ctx.lineCap = "round";
  ctx.beginPath();
  let previousY = value === "1" ? highY : lowY;
  for (let segment = 0; segment < segments; segment += 1) {
    const fromRight = segments - 1 - segment;
    const high = value === "1" ? fromRight % 2 === 0 : fromRight % 2 !== 0;
    const y = high ? highY : lowY;
    const x0 = left + segment * step;
    const x1 = x0 + step;
    if (segment === 0) ctx.moveTo(x0, y);
    else {
      ctx.lineTo(x0, previousY);
      ctx.lineTo(x0, y);
    }
    ctx.lineTo(x1, y);
    previousY = y;
  }
  ctx.stroke();

  const badgeSize = clamp(Math.min(screenRect.w, screenRect.h) * 0.28, 12, 34);
  const badge = {
    x: screenRect.x + screenRect.w / 2 - badgeSize / 2,
    y: screenRect.y + screenRect.h / 2 - badgeSize / 2,
    w: badgeSize,
    h: badgeSize,
  };
  drawScreenRoundedRect(badge, "rgba(8, 12, 18, 0.78)", color, Math.max(1, badgeSize * 0.08), badgeSize * 0.18);
  ctx.font = `800 ${Math.floor(badgeSize * 0.58)}px Arial, Helvetica, sans-serif`;
  ctx.textAlign = "center";
  ctx.textBaseline = "middle";
  ctx.fillStyle = color;
  ctx.fillText(value[0] || "X", badge.x + badge.w / 2, badge.y + badge.h / 2);
  ctx.restore();
}

function drawRegisterFileContents(component, parts) {
  const state = app.componentStates.get(component.id);
  if (!state || !Array.isArray(state.registers)) return;

  const screenRect = rectToScreenRect(parts.bodyRect);
  const clipped = clipScreenRect(screenRect, 0);
  if (!clipped || screenRect.w < 96 || screenRect.h < 72) return;

  const sideInset = Math.min(
    screenRect.w * 0.18,
    Math.max(8, component.rect.w * component.boundaryRatio * app.camera.zoom * 0.72),
  );
  const padY = screenRect.h * 0.055;
  const gridRect = {
    x: screenRect.x + sideInset,
    y: screenRect.y + padY,
    w: screenRect.w - sideInset * 2,
    h: screenRect.h - padY * 2,
  };
  if (gridRect.w < 72 || gridRect.h < 54) return;

  const gap = Math.min(gridRect.w, gridRect.h) * 0.008;
  const cellW = (gridRect.w - gap * (REGISTER_GRID_COLS - 1)) / REGISTER_GRID_COLS;
  const cellH = (gridRect.h - gap * (REGISTER_GRID_ROWS - 1)) / REGISTER_GRID_ROWS;
  if (cellW < 16 || cellH < 9) return;

  const ports = state.ports || {};
  const rs1 = parseKnownBinary(ports.RS1_ADDR && ports.RS1_ADDR.value);
  const rs2 = parseKnownBinary(ports.RS2_ADDR && ports.RS2_ADDR.value);
  const rd = parseKnownBinary(ports.RD_ADDR && ports.RD_ADDR.value);
  const writeActive = ports.REG_WRITE && ports.REG_WRITE.value === "1";
  const highlightByRegister = new Map();
  if (rs1 != null) highlightByRegister.set(rs1, [...(highlightByRegister.get(rs1) || []), "rgb(64, 156, 255)"]);
  if (rs2 != null) highlightByRegister.set(rs2, [...(highlightByRegister.get(rs2) || []), "rgb(89, 214, 141)"]);
  if (writeActive && rd != null && rd !== 0) highlightByRegister.set(rd, [...(highlightByRegister.get(rd) || []), "rgb(255, 184, 77)"]);

  ctx.save();
  ctx.beginPath();
  ctx.rect(clipped.x, clipped.y, clipped.w, clipped.h);
  ctx.clip();

  const labelFont = clamp(Math.min(cellW * 0.18, cellH * 0.32), 5, 12);
  const valueFont = clamp(Math.min(cellW * 0.18, cellH * 0.38), 5, 13);
  const radius = Math.min(4, cellW * 0.12, cellH * 0.25);

  for (let index = 0; index < 32; index += 1) {
    const col = index % REGISTER_GRID_COLS;
    const row = Math.floor(index / REGISTER_GRID_COLS);
    const cell = {
      x: gridRect.x + col * (cellW + gap),
      y: gridRect.y + row * (cellH + gap),
      w: cellW,
      h: cellH,
    };
    const register = state.registers[index];
    const value = registerWordDisplay(register);
    const valueRgb = valueColor(register && register.bits ? register.bits : "X");
    const highlights = highlightByRegister.get(index) || [];

    drawScreenRoundedRect(
      cell,
      index === 0 ? "rgba(8, 12, 18, 0.66)" : "rgba(8, 12, 18, 0.48)",
      highlights.length ? highlights[0] : "rgba(238, 244, 255, 0.16)",
      highlights.length ? Math.max(1.2, Math.min(2.5, cellH * 0.08)) : 0.7,
      radius,
    );

    if (highlights.length > 1) {
      const barHeight = Math.max(1, Math.min(3, cellH * 0.08));
      highlights.slice(1).forEach((color, highlightIndex) => {
        ctx.fillStyle = color;
        ctx.fillRect(cell.x + 2, cell.y + 2 + highlightIndex * (barHeight + 1), cell.w - 4, barHeight);
      });
    }

    ctx.font = `600 ${Math.floor(labelFont)}px Arial, Helvetica, sans-serif`;
    ctx.fillStyle = index === 0 ? "rgba(238, 244, 255, 0.58)" : "rgba(238, 244, 255, 0.74)";
    ctx.textAlign = "left";
    ctx.textBaseline = "top";
    ctx.fillText(`x${index}`, cell.x + Math.max(3, cell.w * 0.06), cell.y + Math.max(2, cell.h * 0.08));

    ctx.font = `700 ${Math.floor(valueFont)}px ui-monospace, SFMono-Regular, Consolas, monospace`;
    ctx.fillStyle = `rgb(${valueRgb.join(",")})`;
    ctx.textAlign = "right";
    ctx.textBaseline = "bottom";
    const fittedValue = ellipsizeText(value, Math.max(0, cell.w - 6));
    if (fittedValue) ctx.fillText(fittedValue, cell.x + cell.w - Math.max(3, cell.w * 0.06), cell.y + cell.h - Math.max(2, cell.h * 0.08));
  }

  ctx.restore();
}

function memoryRowsForDisplay(state) {
  const rows = [];
  const seen = new Set();
  const append = (row, source) => {
    if (!row || seen.has(row.address)) return;
    seen.add(row.address);
    rows.push({ ...row, source });
  };
  (state.touchedWords || []).forEach((row) => append(row, "touched"));
  (state.windowWords || []).forEach((row) => append(row, "window"));
  rows.sort((a, b) => a.address - b.address);
  return rows;
}

function memoryScrollOffset(componentId, maxOffset) {
  const raw = Math.floor(Number(app.memoryScrollOffsets.get(componentId)) || 0);
  const offset = clamp(raw, 0, maxOffset);
  if (offset !== raw) app.memoryScrollOffsets.set(componentId, offset);
  return offset;
}

function setMemoryScrollOffset(componentId, offset, maxOffset) {
  app.memoryScrollOffsets.set(componentId, clamp(Math.floor(Number(offset) || 0), 0, maxOffset));
  requestRender();
}

function hitMemoryScrollControl(screenPoint) {
  for (let index = app.memoryScrollControls.length - 1; index >= 0; index -= 1) {
    const control = app.memoryScrollControls[index];
    if (pointInRect(screenPoint, control.rect)) return control;
  }
  return null;
}

function applyMemoryScrollControl(control) {
  if (!control || !control.enabled) return;
  const current = memoryScrollOffset(control.componentId, control.maxOffset);
  const delta = control.direction === "up" ? -control.step : control.step;
  setMemoryScrollOffset(control.componentId, current + delta, control.maxOffset);
}

function drawMemoryScrollTriangle(rect, direction, enabled) {
  drawScreenRoundedRect(
    rect,
    enabled ? "rgba(8, 12, 18, 0.58)" : "rgba(8, 12, 18, 0.40)",
    enabled ? "rgba(238, 244, 255, 0.22)" : "rgba(238, 244, 255, 0.16)",
    0.8,
    3,
  );

  const cx = rect.x + rect.w / 2;
  const cy = rect.y + rect.h / 2;
  const halfW = rect.w * 0.24;
  const halfH = rect.h * 0.22;
  ctx.beginPath();
  if (direction === "up") {
    ctx.moveTo(cx, cy - halfH);
    ctx.lineTo(cx - halfW, cy + halfH);
    ctx.lineTo(cx + halfW, cy + halfH);
  } else {
    ctx.moveTo(cx, cy + halfH);
    ctx.lineTo(cx - halfW, cy - halfH);
    ctx.lineTo(cx + halfW, cy - halfH);
  }
  ctx.closePath();
  ctx.fillStyle = enabled ? "rgba(238, 244, 255, 0.88)" : "rgba(238, 244, 255, 0.42)";
  ctx.fill();
}

function drawMemoryScrollControls(componentId, controlRect, offset, maxOffset, step) {
  const buttonSize = Math.min(controlRect.h * 0.78, controlRect.w * 0.075);
  if (buttonSize < 4) return;
  const gap = buttonSize * 0.38;
  const y = controlRect.y + (controlRect.h - buttonSize) / 2;
  const upRect = {
    x: controlRect.x + controlRect.w / 2 - buttonSize - gap / 2,
    y,
    w: buttonSize,
    h: buttonSize,
  };
  const downRect = {
    x: controlRect.x + controlRect.w / 2 + gap / 2,
    y,
    w: buttonSize,
    h: buttonSize,
  };
  const upEnabled = offset > 0;
  const downEnabled = offset < maxOffset;

  drawMemoryScrollTriangle(upRect, "up", upEnabled);
  drawMemoryScrollTriangle(downRect, "down", downEnabled);
  app.memoryScrollControls.push({ componentId, direction: "up", rect: upRect, enabled: upEnabled, maxOffset, step });
  app.memoryScrollControls.push({ componentId, direction: "down", rect: downRect, enabled: downEnabled, maxOffset, step });
}

function drawMemory64Kx32Contents(component, parts) {
  const state = app.componentStates.get(component.id);
  if (!state || !state.supported) return;

  const screenRect = rectToScreenRect(parts.bodyRect);
  const clipped = clipScreenRect(screenRect, 0);
  if (!clipped || screenRect.w < 140 || screenRect.h < 86) return;

  const sideInset = Math.min(
    screenRect.w * 0.24,
    Math.max(screenRect.w * 0.13, component.rect.w * component.boundaryRatio * app.camera.zoom),
  );
  const padY = screenRect.h * 0.055;
  const rect = {
    x: screenRect.x + sideInset,
    y: screenRect.y + padY,
    w: screenRect.w - sideInset * 2,
    h: screenRect.h - padY * 2,
  };
  if (rect.w < 100 || rect.h < 62) return;

  const ports = state.ports || {};
  const fault = ports.FAULT && ports.FAULT.value;
  const readActive = ports.READ_EN && ports.READ_EN.value === "1";
  const writeActive = ports.WRITE_EN && ports.WRITE_EN.value === "1";
  const accessMode = writeActive && readActive ? "WRITE+READ" : writeActive ? "WRITE" : readActive ? "READ" : "IDLE";
  const mode = fault === "1" ? (accessMode === "IDLE" ? "FAULT" : `FAULT ${accessMode}`) : accessMode;
  const modeColor = fault === "1"
    ? "rgb(211, 47, 47)"
    : writeActive && readActive
      ? "rgb(124, 197, 255)"
      : writeActive
      ? "rgb(255, 184, 77)"
      : readActive
        ? "rgb(89, 214, 141)"
        : "rgba(238, 244, 255, 0.65)";
  const activeHex = state.activeAddress == null ? "ADDR XXXXXXXX" : `ADDR ${state.activeAddress.toString(16).toUpperCase().padStart(8, "0")}`;
  const showingInstructions = state.memoryRole === "instruction";
  const headerH = rect.h * 0.18;
  const headerGap = rect.h * 0.01;
  const tableY = rect.y + headerH + headerGap;
  const tableH = Math.max(0, rect.h - headerH - headerGap);
  const rows = memoryRowsForDisplay(state);
  const visibleRowCount = Math.min(rows.length, MEMORY_WORD_WINDOW_ROWS);
  const showScrollControls = rows.length >= MEMORY_WORD_WINDOW_ROWS;
  const scrollable = rows.length > visibleRowCount;
  const scrollControlH = showScrollControls ? tableH * 0.16 : 0;
  const scrollControlGap = showScrollControls ? tableH * 0.015 : 0;
  const rowsAreaH = Math.max(0, tableH - scrollControlH - scrollControlGap);
  const maxOffset = Math.max(0, rows.length - visibleRowCount);
  const offset = memoryScrollOffset(component.id, maxOffset);
  const shownRows = rows.slice(offset, offset + visibleRowCount);
  const rowH = visibleRowCount ? rowsAreaH / visibleRowCount : rowsAreaH;
  const rowGap = rowH * 0.12;
  const activeBase = state.activeBaseAddress;

  ctx.save();
  ctx.beginPath();
  ctx.rect(clipped.x, clipped.y, clipped.w, clipped.h);
  ctx.clip();

  drawScreenRoundedRect(
    { x: rect.x, y: rect.y, w: rect.w, h: headerH },
    "rgba(8, 12, 18, 0.54)",
    fault === "1" ? "rgba(211, 47, 47, 0.9)" : "rgba(238, 244, 255, 0.18)",
    1,
    4,
  );
  const headerFont = Math.min(Math.min(headerH * 0.42, rect.w * 0.045), MAX_COMPONENT_FONT_PX);
  ctx.font = `700 ${Math.floor(headerFont)}px ui-monospace, SFMono-Regular, Consolas, monospace`;
  ctx.textBaseline = "middle";
  ctx.textAlign = "left";
  ctx.fillStyle = "rgba(238, 244, 255, 0.84)";
  ctx.fillText(activeHex, rect.x + 7, rect.y + headerH / 2);
  ctx.textAlign = "right";
  ctx.fillStyle = modeColor;
  ctx.fillText(mode, rect.x + rect.w - 7, rect.y + headerH / 2);

  const addressW = showingInstructions ? rect.w * 0.33 : rect.w * 0.43;
  const valueW = rect.w - addressW;
  const rowFont = Math.min(Math.min(rowH * 0.46, rect.w * 0.04), MAX_COMPONENT_FONT_PX);

  shownRows.forEach((row, index) => {
    const rowRect = {
      x: rect.x,
      y: tableY + index * rowH,
      w: rect.w,
      h: Math.max(0, rowH - rowGap),
    };
    const active = activeBase != null && row.address === activeBase;
    const value = showingInstructions && row.instructionText ? row.instructionText : registerWordDisplay(row);
    const valueRgb = valueColor(row.bits || "X");
    drawScreenRoundedRect(
      rowRect,
      active ? "rgba(21, 42, 64, 0.78)" : "rgba(8, 12, 18, 0.42)",
      active ? modeColor : "rgba(238, 244, 255, 0.13)",
      active ? 1.5 : 0.7,
      3,
    );
    ctx.font = `700 ${Math.floor(rowFont)}px ui-monospace, SFMono-Regular, Consolas, monospace`;
    ctx.textBaseline = "middle";
    ctx.textAlign = "left";
    ctx.fillStyle = row.source === "touched" ? "rgba(238, 244, 255, 0.82)" : "rgba(238, 244, 255, 0.62)";
    ctx.fillText((row.addressHex || "0x????????").replace(/^0x/i, ""), rowRect.x + 7, rowRect.y + rowRect.h / 2);
    ctx.textAlign = showingInstructions ? "left" : "right";
    ctx.fillStyle = `rgb(${valueRgb.join(",")})`;
    const fitted = ellipsizeText(value, Math.max(0, valueW - 12));
    if (fitted) {
      const valueX = showingInstructions ? rowRect.x + addressW + 6 : rowRect.x + rowRect.w - 7;
      ctx.fillText(fitted, valueX, rowRect.y + rowRect.h / 2);
    }
  });

  if (!rows.length) {
    ctx.font = `700 ${Math.floor(Math.min(rect.h * 0.12, MAX_COMPONENT_FONT_PX))}px Arial, Helvetica, sans-serif`;
    ctx.textAlign = "center";
    ctx.textBaseline = "middle";
    ctx.fillStyle = "rgba(238, 244, 255, 0.58)";
    ctx.fillText("0 touched words", rect.x + rect.w / 2, tableY + rowsAreaH / 2);
  }

  if (showScrollControls) {
    drawMemoryScrollControls(
      component.id,
      { x: rect.x, y: tableY + rowsAreaH + scrollControlGap, w: rect.w, h: scrollControlH },
      offset,
      maxOffset,
      1,
    );
  }

  ctx.restore();
}

function drawComponentBackground(component, vr) {
  if (!component.rect || !rectIntersects(component.rect, vr)) return false;
  const parts = componentDrawParts(component);
  drawDefaultComponentBackground(parts);
  return true;
}

function drawComponentForeground(component, vr) {
  if (!component.rect || !rectIntersects(component.rect, vr)) return;
  const parts = componentDrawParts(component);
  drawDefaultComponentForeground(component, parts);
  const gateKind = basicLogicGateKind(component);
  if (gateKind) drawLogicGateSymbol(component, parts, gateKind);
  if (component.type === "ClockGenerator") drawClockGeneratorContents(component, parts);
  if (component.type === "ConstantValue") drawConstantValueContents(component, parts);
  if (component.type === "BitSplitter") drawBitSplitterContents(component, parts);
  if (component.type === "BitJoiner") drawBitJoinerContents(component, parts);
  if (component.type === "Rewire") drawRewireContents(component, parts);
  if (component.fidelity === "behavioral") {
    if (component.type === "MemoryBit") {
      drawMemoryBitValue(component, parts);
    }
    if (component.type === "Register32") {
      drawRegister32Value(component, parts);
    }
    if (component.type === "RegisterFile32x32") {
      drawRegisterFileContents(component, parts);
    }
    if (component.type === "Memory64Kx32") {
      drawMemory64Kx32Contents(component, parts);
    }
  }

  for (const pin of [...component.inputPins, ...component.outputPins]) drawPin(pin);
}

function drawComponent(component, vr) {
  if (!drawComponentBackground(component, vr)) return;
  if (!componentCanDescend(component)) return;
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

  const color = `rgb(${valueColor(pinValue(pin)).join(",")})`;
  ctx.fillStyle = color;
  const stateColor = valueColor(pinValue(pin));
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
  app.renderScheduled = false;
  resizeCanvas();
  updateCameraAnimation(performance.now());
  if (app.geometryDirty) computeGeometry();
  scheduleVisibleTopology();

  ctx.fillStyle = "rgb(30, 30, 30)";
  ctx.fillRect(0, 0, app.width, app.height);
  drawGrid();

  const vr = viewRect();
  const root = app.components.get(app.rootId);
  app.memoryScrollControls = [];
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

  if (app.playing || app.cameraAnimation) requestRender();
}

function requestRender() {
  if (app.renderScheduled) return;
  app.renderScheduled = true;
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
  if (app.hover && app.hover.type === "memory-scroll") {
    canvas.style.cursor = app.hover.enabled ? "pointer" : "default";
    return;
  }
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
  const scrollControl = hitMemoryScrollControl(screenPoint);
  let hover = scrollControl
    ? { type: "memory-scroll", id: scrollControl.componentId, direction: scrollControl.direction, enabled: scrollControl.enabled }
    : null;

  if (!hover) {
    const pinCandidates = app.pinIndex.queryPoint(world).sort((a, b) => b.z - a.z);
    for (const candidate of pinCandidates) {
      const pin = app.pins.get(candidate.id);
      if (pin && pointInRect(world, pin.rect, 4 / app.camera.zoom)) {
        hover = { type: "pin", id: pin.id };
        break;
      }
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
  requestRender();
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
    text = `${label} = ${pinValue(pin)}`;
  } else if (app.hover.type === "wire") {
    const wire = app.wires.get(app.hover.id);
    text = `${wire.name}[${wire.width}]=${wireValue(wire)}`;
  } else if (app.hover.type === "memory-scroll") {
    const component = app.components.get(app.hover.id);
    const direction = app.hover.direction === "up" ? "previous" : "next";
    const prefix = app.hover.enabled ? direction : `no ${direction}`;
    text = component ? `${component.name}: ${prefix} memory rows` : `${prefix} memory rows`;
  } else {
    const component = app.components.get(app.hover.id);
    text = `${component.name} (${component.type})`;
  }

  tooltip.textContent = text;
  tooltip.style.display = "block";
  const canvasRect = canvas.getBoundingClientRect();
  const pagePoint = {
    x: canvasRect.left + screenPoint.x,
    y: canvasRect.top + screenPoint.y,
  };
  const x = Math.min(window.innerWidth - tooltip.offsetWidth - 6, pagePoint.x + 14);
  const y = Math.min(window.innerHeight - tooltip.offsetHeight - 6, pagePoint.y + 14);
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
  rebuildSelectionPath();
  if (app.explorer) {
    app.explorer.setSelected(
      type === "component" ? id : null,
    );
  }
  updateInspector();
  requestRender();
}

function rebuildSelectionPath() {
  app.selectionPath = new Set();
  if (!app.selection) return;

  let component = null;
  if (app.selection.type === "component") component = app.components.get(app.selection.id);
  if (app.selection.type === "pin") {
    const pin = app.pins.get(app.selection.id);
    component = pin ? app.components.get(pin.componentId) : null;
  }
  if (app.selection.type === "wire") {
    const wire = app.wires.get(app.selection.id);
    component = wire ? app.components.get(wire.ownerId) : null;
  }

  while (component && !app.selectionPath.has(component.id)) {
    app.selectionPath.add(component.id);
    component = component.parentId ? app.components.get(component.parentId) : null;
  }
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
  const values = app.state && app.state.pins;
  if (Array.isArray(values)) return values[pin.stateIndex] || "X";
  return (values && values[pin.id]) || "X";
}

function wireValue(wire) {
  const values = app.state && app.state.wires;
  if (Array.isArray(values)) return values[wire.stateIndex] || "X";
  return (values && values[wire.id]) || "X";
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
  const baseSubtitle = layoutKey === component.type
    ? `${component.type} | depth ${component.depth}`
    : `${component.type} | layout ${layoutKey} | depth ${component.depth}`;
  const selectionSubtitle = component.fidelity
    ? ` | ${component.fidelity} | ${component.implementationId}`
    : "";
  ui.inspectorSubtitle.textContent = `${baseSubtitle}${selectionSubtitle}`;
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
    rebuildSelectionPath();
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
  requestRender();
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
  app.cameraAnimation = null;
  canvas.setPointerCapture(event.pointerId);
  const screen = pointerPosition(event);
  updateHover(screen);
  const scrollControl = hitMemoryScrollControl(screen);
  if (scrollControl) {
    event.preventDefault();
    applyMemoryScrollControl(scrollControl);
    updateHover(screen);
    return;
  }
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
    requestRender();
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
  app.cameraAnimation = null;
  const root = app.components.get(app.rootId);
  if (!root || !root.rect) return;
  const viewport = cameraSafeViewport();
  const screenCenter = focusScreenCenter(viewport);
  const fittingViewport = centeredFittingViewport(
    viewport,
    screenCenter,
  );
  app.camera.zoom = fitZoomForRect(root.rect, fittingViewport);
  if (!Number.isFinite(app.camera.zoom) || app.camera.zoom <= 0) app.camera.zoom = 1;
  const worldCenter = { x: root.rect.x + root.rect.w / 2, y: root.rect.y + root.rect.h / 2 };
  app.camera.offset.x = worldCenter.x - screenCenter.x / app.camera.zoom;
  app.camera.offset.y = worldCenter.y - screenCenter.y / app.camera.zoom;
  requestRender();
}

function elementRectInCanvas(element, canvasRect) {
  if (!element || element.classList.contains("hidden")) return null;
  const rect = element.getBoundingClientRect();
  return {
    left: rect.left - canvasRect.left,
    top: rect.top - canvasRect.top,
    right: rect.right - canvasRect.left,
    bottom: rect.bottom - canvasRect.top,
  };
}

function rangesOverlap(startA, endA, startB, endB) {
  return endA > startB && endB > startA;
}

function cameraViewport(avoidInspector) {
  const canvasRect = canvas.getBoundingClientRect();
  let left = 0;
  let top = 0;
  let right = app.width;
  let bottom = app.height;

  const topbar = elementRectInCanvas(ui.topbar, canvasRect);
  if (
    topbar
    && rangesOverlap(topbar.left, topbar.right, left, right)
    && topbar.top <= top
  ) {
    top = Math.max(top, topbar.bottom);
  }

  const viewbar = elementRectInCanvas(ui.viewbar, canvasRect);
  if (
    viewbar
    && rangesOverlap(viewbar.left, viewbar.right, left, right)
    && viewbar.top > (top + bottom) / 2
    && viewbar.top < bottom
  ) {
    bottom = Math.min(bottom, viewbar.top);
  }

  const explorer = document.body.classList.contains("explorer-open")
    ? elementRectInCanvas(ui.explorerPanel, canvasRect)
    : null;
  if (
    explorer
    && rangesOverlap(explorer.top, explorer.bottom, top, bottom)
    && explorer.left <= left
    && explorer.right > left
  ) {
    left = Math.min(right, explorer.right);
  }

  if (avoidInspector) {
    const inspector = elementRectInCanvas(ui.inspector, canvasRect);
    if (
      inspector
      && rangesOverlap(inspector.top, inspector.bottom, top, bottom)
      && inspector.right > right - 80
      && inspector.left < right
    ) {
      right = Math.max(left, inspector.left);
    }
  }

  const availableWidth = Math.max(1, right - left);
  const availableHeight = Math.max(1, bottom - top);
  const padding = Math.min(
    FOCUS_PADDING_PX,
    Math.max(0, (availableWidth - 1) / 4),
    Math.max(0, (availableHeight - 1) / 4),
  );
  return {
    x: left + padding,
    y: top + padding,
    w: Math.max(1, availableWidth - padding * 2),
    h: Math.max(1, availableHeight - padding * 2),
  };
}

function cameraSafeViewport() {
  return cameraViewport(true);
}

function cameraNavigationViewport() {
  return cameraViewport(false);
}

function viewportCenter(viewport) {
  return {
    x: viewport.x + viewport.w / 2,
    y: viewport.y + viewport.h / 2,
  };
}

function focusScreenCenter(containmentViewport) {
  const preferred = viewportCenter(cameraNavigationViewport());
  const fallback = viewportCenter(containmentViewport);
  const right =
    containmentViewport.x + containmentViewport.w;
  const bottom =
    containmentViewport.y + containmentViewport.h;
  return {
    x:
      preferred.x >= containmentViewport.x
      && preferred.x <= right
        ? preferred.x
        : fallback.x,
    y:
      preferred.y >= containmentViewport.y
      && preferred.y <= bottom
        ? preferred.y
        : fallback.y,
  };
}

function centeredFittingViewport(viewport, center) {
  const right = viewport.x + viewport.w;
  const bottom = viewport.y + viewport.h;
  const halfWidth = Math.max(
    0.5,
    Math.min(center.x - viewport.x, right - center.x),
  );
  const halfHeight = Math.max(
    0.5,
    Math.min(center.y - viewport.y, bottom - center.y),
  );
  return {
    x: center.x - halfWidth,
    y: center.y - halfHeight,
    w: halfWidth * 2,
    h: halfHeight * 2,
  };
}

function fitZoomForRect(rect, viewport) {
  const width = Math.abs(Number(rect.w));
  const height = Math.abs(Number(rect.h));
  if (
    !Number.isFinite(width)
    || !Number.isFinite(height)
    || width <= 0
    || height <= 0
  ) {
    return 1;
  }
  const zoom = Math.min(
    viewport.w / width,
    viewport.h / height,
  );
  return Number.isFinite(zoom) && zoom > 0
    ? Math.max(0.02, zoom)
    : 1;
}

function componentFocusRect(component) {
  let bounds = { ...component.rect };
  for (const pin of [
    ...component.inputPins,
    ...component.outputPins,
  ]) {
    if (pin.rect) bounds = unionRect(bounds, pin.rect);
    if (pin.outerStub && pin.outerStub.length) {
      bounds = unionRect(bounds, pointsBBox(pin.outerStub));
    }
  }
  return bounds;
}

function easeInOutCubic(value) {
  return value < 0.5
    ? 4 * value * value * value
    : 1 - ((-2 * value + 2) ** 3) / 2;
}

function updateCameraAnimation(now) {
  const animation = app.cameraAnimation;
  if (!animation) return;
  const progress = clamp(
    (now - animation.startedAt) / animation.duration,
    0,
    1,
  );
  const eased = easeInOutCubic(progress);
  const logZoom =
    animation.from.logZoom
    + (animation.to.logZoom - animation.from.logZoom) * eased;
  const worldCenter = {
    x:
      animation.from.worldX
      + (animation.to.worldX - animation.from.worldX) * eased,
    y:
      animation.from.worldY
      + (animation.to.worldY - animation.from.worldY) * eased,
  };
  app.camera.zoom = Math.exp(logZoom);
  app.camera.offset.x =
    worldCenter.x - animation.screenCenter.x / app.camera.zoom;
  app.camera.offset.y =
    worldCenter.y - animation.screenCenter.y / app.camera.zoom;
  if (progress >= 1) app.cameraAnimation = null;
}

function closestExistingComponent(componentId) {
  let path = componentId;
  while (path) {
    const component = app.components.get(path);
    if (component) return component;
    const separator = path.lastIndexOf(".");
    path = separator >= 0 ? path.slice(0, separator) : "";
  }
  return null;
}

function smoothlyFocusComponent(componentId) {
  const component = closestExistingComponent(componentId);
  if (!component || !component.rect) return;
  setSelectedComponent(component.id);
  const viewport = cameraSafeViewport();
  const focusRect = componentFocusRect(component);
  const targetScreenCenter = focusScreenCenter(viewport);
  const fittingViewport = centeredFittingViewport(
    viewport,
    targetScreenCenter,
  );
  const targetZoom = fitZoomForRect(focusRect, fittingViewport);
  const center = {
    x: focusRect.x + focusRect.w / 2,
    y: focusRect.y + focusRect.h / 2,
  };
  const fromScreen = worldToScreen(center);
  const fromWorldCenter = screenToWorld(targetScreenCenter);
  const travel = Math.hypot(
    targetScreenCenter.x - fromScreen.x,
    targetScreenCenter.y - fromScreen.y,
  );
  const zoomStops = Math.abs(
    Math.log2(targetZoom / Math.max(0.0001, app.camera.zoom)),
  );
  const duration = clamp(
    280 + travel * 0.12 + zoomStops * 70,
    FOCUS_ANIMATION_MIN_MS,
    FOCUS_ANIMATION_MAX_MS,
  );
  app.cameraAnimation = {
    startedAt: performance.now(),
    duration,
    screenCenter: targetScreenCenter,
    from: {
      worldX: fromWorldCenter.x,
      worldY: fromWorldCenter.y,
      logZoom: Math.log(Math.max(0.0001, app.camera.zoom)),
    },
    to: {
      worldX: center.x,
      worldY: center.y,
      logZoom: Math.log(targetZoom),
    },
  };
  requestRender();
}

function setPlaying(value) {
  app.playing = value;
  ui.playPause.textContent = value ? "Pause" : "Play";
  app.lastPlaybackTick = performance.now();
  requestRender();
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

function statefulOverlayComponents() {
  return app.componentOrder.filter((component) => (
    component.fidelity === "behavioral"
    && (
      component.type === "RegisterFile32x32"
      || component.type === "Memory64Kx32"
    )
  ));
}

async function refreshComponentStates(index, requestId = null) {
  const components = statefulOverlayComponents();
  if (!components.length) {
    app.componentStates.clear();
    return;
  }

  const states = await Promise.all(components.map(async (component) => {
    const response = await fetch(apiUrl(
      `api/component-state?session=${encodeURIComponent(app.sessionId)}&index=${index}&componentId=${encodeURIComponent(component.id)}`,
    ));
    if (!response.ok) throw new Error(await response.text());
    return [component.id, await response.json()];
  }));

  if (requestId != null && requestId !== app.stateRequestId) return;
  const activeIds = new Set(components.map((component) => component.id));
  for (const componentId of [...app.componentStates.keys()]) {
    if (activeIds.has(componentId)) app.componentStates.delete(componentId);
  }
  for (const [componentId, state] of states) {
    app.componentStates.set(componentId, state);
  }
  requestRender();
}

async function setStateIndex(index) {
  index = Math.max(0, Math.min(index, app.timestamps.length - 1));
  app.requestedIndex = index;
  ui.slider.value = String(index);
  updateCheckpointControls();
  const requestId = ++app.stateRequestId;
  app.stateRequestInFlight = true;
  try {
    const response = await fetch(apiUrl(
      `api/state?session=${encodeURIComponent(app.sessionId)}&index=${index}`,
    ));
    if (!response.ok) throw new Error(await response.text());
    const state = await response.json();
    if (requestId !== app.stateRequestId) return;
    app.state = state;
    app.currentIndex = state.index;
    app.requestedIndex = state.index;
    ui.slider.value = String(state.index);
    setTimeLabel(state);
    updateCheckpointControls();
    requestRender();
    await refreshComponentStates(state.index, requestId);
    if (requestId !== app.stateRequestId) return;
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
  requestRender();
}

function showError(error) {
  console.error(error);
  loading.textContent = String(error.message || error);
  loading.classList.remove("hidden");
}

function nextPaint() {
  return new Promise((resolve) => {
    requestAnimationFrame(() => requestAnimationFrame(resolve));
  });
}

function baseStatsText() {
  if (!app.stats) return "";
  const topology = `${app.stats.componentCount} comps | ${app.stats.wireCount} wires | ${app.stats.pinCount} pins`;
  const metric = (name) => app.performanceMetrics.find(
    (candidate) => candidate.name === name,
  );
  const cycles = metric("cpu.hardware_cycles");
  const cpi = metric("cpu.cpi");
  if (!cycles || !cpi) return topology;
  return `${topology} | ${Number(cycles.value).toLocaleString()} cyc | CPI ${Number(cpi.value).toFixed(3)}`;
}

function performanceMetricsTitle() {
  return app.performanceMetrics.map((metric) => {
    const value = Number(metric.value);
    const formatted = Number.isInteger(value)
      ? value.toLocaleString()
      : value.toFixed(4);
    return `${metric.name}: ${formatted} ${metric.unit}`;
  }).join("\n");
}

function updateTopologyProgress() {
  const base = baseStatsText();
  ui.statsLabel.title = performanceMetricsTitle();
  if (
    app.topologyEncoding !== "progressive-v1"
    || (
      !app.topologyRequestInFlight
      && app.pendingScopeIds.size === 0
    )
  ) {
    ui.statsLabel.textContent = base;
    return;
  }
  ui.statsLabel.textContent = `${base} | preparing visible detail`;
}

function mergeTopologyRecords(payload) {
  const touchedComponents = new Set();
  const addedWires = [];

  for (const [stateIndex, data] of (payload.pins || []).entries()) {
    if (app.pins.has(data.id)) continue;
    const pin = {
      ...data,
      stateIndex: data.stateIndex ?? stateIndex,
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
      touchedComponents.add(component);
    }
  }

  for (const [stateIndex, data] of (payload.wires || []).entries()) {
    if (app.wires.has(data.id)) continue;
    const wire = {
      ...data,
      stateIndex: data.stateIndex ?? stateIndex,
      sourcePin: data.sourcePinId
        ? app.pins.get(data.sourcePinId)
        : null,
      sinkPins: (data.sinkPinIds || [])
        .map((id) => app.pins.get(id))
        .filter(Boolean),
      paths: [],
      branches: [],
      bbox: null,
    };
    app.wires.set(wire.id, wire);
    app.wireOrder.push(wire);
    addedWires.push(wire);
    if (!app.wiresByOwner.has(wire.ownerId)) {
      app.wiresByOwner.set(wire.ownerId, []);
    }
    app.wiresByOwner.get(wire.ownerId).push(wire);
    if (wire.sourcePin) {
      wire.sourcePin.sourceWireIds.push(wire.id);
    }
    wire.sinkPins.forEach(
      (pin) => pin.sinkWireIds.push(wire.id),
    );
  }

  if ([...touchedComponents].some((component) => component.rect)) {
    touchedComponents.forEach((component) => {
      if (component.rect) layoutPins(component);
    });
    addedWires.forEach(buildWirePaths);
    rebuildSpatialIndexes();
    if (app.selection) updateInspector();
  }
}

function visibleMissingScopeIds() {
  if (app.topologyEncoding !== "progressive-v1") return [];
  return visibleGeometryComponents()
    .filter((component) => (
      componentCanDescend(component)
      && !app.loadedScopeIds.has(component.id)
      && !app.pendingScopeIds.has(component.id)
    ))
    .map((component) => component.id);
}

async function loadVisibleTopologyScopes(ownerIds) {
  const sessionId = app.sessionId;
  app.topologyRequestInFlight = true;
  ownerIds.forEach((id) => app.pendingScopeIds.add(id));
  updateTopologyProgress();
  try {
    const response = await fetch(apiUrl("api/topology"), {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        session: sessionId,
        ownerIds,
      }),
    });
    if (!response.ok) throw new Error(await response.text());
    const payload = await response.json();
    if (
      payload.sessionId !== sessionId
      || app.sessionId !== sessionId
    ) {
      return;
    }
    mergeTopologyRecords(payload);
    for (const scopeId of payload.scopeIds || ownerIds) {
      app.loadedScopeIds.add(scopeId);
    }
  } finally {
    if (app.sessionId === sessionId) {
      ownerIds.forEach((id) => app.pendingScopeIds.delete(id));
      app.topologyRequestInFlight = false;
      updateTopologyProgress();
    }
  }
  if (app.sessionId !== sessionId) return;
  requestRender();
  scheduleVisibleTopology();
}

function scheduleVisibleTopology() {
  if (
    app.topologyEncoding !== "progressive-v1"
    || app.topologyRequestInFlight
    || !app.sessionId
  ) {
    return;
  }
  const ownerIds = visibleMissingScopeIds().slice(0, 512);
  if (!ownerIds.length) return;
  setTimeout(() => {
    loadVisibleTopologyScopes(ownerIds).catch(showError);
  }, 0);
}

function buildLocalModel(payload) {
  app.scenario = payload.scenario;
  app.sessionId = payload.sessionId;
  app.profile = payload.profile || {
    editable: false,
    revision: 0,
    exactOverrides: {},
  };
  app.layoutKey = payload.layoutKey || payload.scenario;
  app.rootId = payload.rootId;
  app.timestamps = payload.timestamps && payload.timestamps.length ? payload.timestamps : [0];
  app.checkpoints = (payload.checkpoints || []).filter((checkpoint) => Number.isFinite(Number(checkpoint.index)));
  app.layout = payload.layout;
  normalizeLayoutPayload(payload);
  app.savedLayout = cloneJson(app.layout);
  resetLayoutHistory();
  app.settings = app.layout.default_settings || {};
  app.stats = payload.stats;
  app.performanceMetrics = payload.performanceMetrics || [];
  app.state = payload.state;
  app.currentIndex = 0;
  app.requestedIndex = 0;
  app.stateRequestInFlight = false;
  app.layoutDirty = false;
  app.selection = null;
  app.selectionPath = new Set();
  app.selectedComponentId = null;
  app.components = new Map();
  app.pins = new Map();
  app.wires = new Map();
  app.componentStates = new Map();
  app.memoryScrollOffsets = new Map();
  app.memoryScrollControls = [];
  app.componentOrder = [];
  app.pinOrder = [];
  app.wireOrder = [];
  app.wiresByOwner = new Map();
  app.topologyEncoding =
    payload.topologyEncoding || "complete-v1";
  app.loadedScopeIds = new Set(payload.loadedScopeIds || []);
  app.pendingScopeIds = new Set();
  app.topologyRequestInFlight = false;

  for (const data of payload.components) {
    const parentFromIndex = Number.isInteger(data.parentIndex)
      && data.parentIndex >= 0
      ? payload.components[data.parentIndex]
      : null;
    const component = {
      ...data,
      layoutType: data.layoutType || data.type,
      parentId: data.parentId
        || (parentFromIndex ? parentFromIndex.id : null),
      childIds: [],
      fidelity: data.fidelity || "unspecified",
      availableFidelities: data.availableFidelities || [],
      profileSelectable: Boolean(data.profileSelectable),
      profileFingerprint: data.profileFingerprint || "explicit",
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

  for (const component of app.componentOrder) {
    if (!component.parentId) continue;
    const parent = app.components.get(component.parentId);
    if (parent) parent.childIds.push(component.id);
  }

  mergeTopologyRecords(payload);

  ui.slider.min = "0";
  ui.slider.max = String(Math.max(0, app.timestamps.length - 1));
  ui.slider.value = "0";
  renderCheckpointMarks();
  setTimeLabel(app.state);
  updateTopologyProgress();
  app.geometryDirty = true;
  computeGeometry();
  frameRoot();
  rebuildSpatialIndexes();
  if (app.explorer) {
    app.explorer.setModel(
      app.components,
      app.rootId,
      app.profile,
    );
  }
  updateInspector();
  requestRender();
  scheduleVisibleTopology();
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
  loading.textContent = "Building and simulating circuit…";
  loading.classList.remove("hidden");
  setPlaying(false);
  await nextPaint();
  const response = await fetch(apiUrl(`api/circuit?scenario=${encodeURIComponent(scenario)}`));
  if (!response.ok) throw new Error(await response.text());
  loading.textContent = "Decoding circuit index…";
  await nextPaint();
  const payload = await response.json();
  loading.textContent = "Preparing circuit overview…";
  await nextPaint();
  buildLocalModel(payload);
  await nextPaint();
  await refreshComponentStates(app.currentIndex, app.stateRequestId);
  loading.classList.add("hidden");
  ui.saveStatus.textContent = "";
}

async function applyExplorerProfile(request) {
  if (app.layoutDirty) {
    app.explorer.updateStatus(
      "Save or reset the layout before applying fidelity changes",
      true,
    );
    return;
  }
  app.explorer.setApplying();
  loading.textContent = "Building and simulating selected circuit…";
  loading.classList.remove("hidden");
  setPlaying(false);
  try {
    await nextPaint();
    const response = await fetch(apiUrl("api/circuit/apply"), {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        scenario: app.scenario,
        revision: request.revision,
        exactOverrides: request.exactOverrides,
      }),
    });
    if (!response.ok) throw new Error(await response.text());
    loading.textContent = "Decoding selected circuit index…";
    await nextPaint();
    const payload = await response.json();
    loading.textContent = "Preparing selected circuit overview…";
    await nextPaint();
    buildLocalModel(payload);
    await nextPaint();
    await refreshComponentStates(
      app.currentIndex,
      app.stateRequestId,
    );
    loading.classList.add("hidden");
    app.explorer.updateStatus("Profile applied and simulation complete");
    if (request.pendingFocusId) {
      smoothlyFocusComponent(request.pendingFocusId);
    }
  } catch (error) {
    loading.classList.add("hidden");
    app.explorer.updateStatus(
      String(error.message || error),
      true,
    );
  }
}

app.explorer = new window.CircuitExplorer({
  panel: ui.explorerPanel,
  viewport: ui.explorerViewport,
  spacer: ui.explorerSpacer,
  rowsLayer: ui.explorerRows,
  search: ui.explorerSearch,
  applyButton: ui.explorerApply,
  revertButton: ui.explorerRevert,
  status: ui.explorerStatus,
  toggleButton: ui.explorerToggle,
  resizer: ui.explorerResizer,
  onFocus: smoothlyFocusComponent,
  onApply: (request) => {
    applyExplorerProfile(request).catch(showError);
  },
  onViewportChanged: () => {
    resizeCanvas();
    requestRender();
    setTimeout(() => {
      resizeCanvas();
      requestRender();
    }, 180);
  },
});

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

window.addEventListener("resize", () => {
  resizeCanvas();
  requestRender();
});

(async function init() {
  try {
    resizeCanvas();
    const selected = await loadScenarios();
    await loadCircuit(selected);
    requestRender();
  } catch (error) {
    showError(error);
  }
}());
