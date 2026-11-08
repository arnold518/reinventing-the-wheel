import createCircuitSimModule from "./circuitsim_wasm.js?v=18";
import {completeProfileLayouts} from "./profile_layouts.mjs?v=1";
import { LAYOUT_CHANGES_FORMAT, layoutChanges, mergeLayout, mergeLayoutStores } from "./layout_changes.mjs?v=3";
import { defaultAlias, aliasesFor as catalogAliasesFor, scenarioOptions as makeScenarioOptions } from './scenario_catalog.mjs?v=2';
import {normalizeFidelityPreset,profileStorageKey} from './fidelity_profiles.mjs';

const DB_NAME = "circuitsim-visualizer-v2";
const DB_VERSION = 1;
const LAYOUT_KEY = "layout";
const PROFILE_KEY = "profiles";
const NON_VISUALIZABLE = new Set(["WireTemplateTest", "RewireValidationTest"]);

let scenarioCatalog = null;
let modulePromise = null;
let bridge = null;
let defaultLayout = null;
let currentDefaults = null;
let wasmModule = null;
let defaultProfiles = { schema_version: 1, scenarios: {} };
let palette = null;
let currentLayout = null;
let previewComponentsById = new Map();
let currentPayload = null;
let currentScenario = null;
let descriptors = [];
let scenarioOptions = [];
let scenariosByAlias = new Map();
let memoryStore = new Map();
let databasePromise = null;

function clone(value) {
  return JSON.parse(JSON.stringify(value));
}

function migrateProfileOverrides(overrides, rootName = null) {
  const migrations = [
    ["RV32I_SINGLE_CYCLE_SYSTEM_ROOT", "RV32I_PROGRAM_ROOT"],
    ["RV32I_FIVE_STAGE_PROGRAM_ROOT", "RV32I_PROGRAM_ROOT"],
  ];
  const migrated = {};
  for (const [path, fidelity] of Object.entries(overrides || {})) {
    let migratedPath = path;
    for (const [oldRoot, newRoot] of migrations) {
      if (path === oldRoot) {
        migratedPath = `${newRoot}.CORE`;
        break;
      }
      if (path.startsWith(`${oldRoot}.`)) {
        migratedPath = `${newRoot}${path.slice(oldRoot.length)}`;
        break;
      }
    }
    if (!(migratedPath in migrated)) migrated[migratedPath] = fidelity;
  }
  for (const [path, fidelity] of Object.entries(overrides || {})) {
    if (path === "RV32I_PROGRAM_ROOT" || path.startsWith("RV32I_PROGRAM_ROOT.")) {
      migrated[path] = fidelity;
    }
  }
  const ffPath = path => path.replace(
    /(^|\.)(REGISTER_FILE|REGISTER_FILE_W32_N32_ROOT)\.X([1-9]|[12][0-9]|3[01])(?=\.|$)(.*)$/,
    (_match, dot, parent, index, tail) => `${dot}${parent}.STORAGE.WORD_${Number(index)-1}${tail
      .replace(/^\.BIT_(\d+)(?=\.|$)/, '.CELL_$1')
      .replace(/^\.D_SPLIT(?=\.|$)/, '.DATA')
      .replace(/^\.Q_JOIN(?=\.|$)/, '.OUTPUT')}`,
  );
  const stagePath = path => {
    const moves = {
      'CONTROL_FLOW.PC_IMM_ADD': 'EXECUTE.BRANCH_TARGET_ADD',
      'CONTROL_FLOW.RS1_IMM_ADD': 'EXECUTE.ALU.ARITHMETIC',
      'CONTROL_FLOW.JALR_MASK': 'EXECUTE.JALR_MASK',
      'CONTROL_FLOW': 'FETCH', 'DECODE_CONTROL': 'DECODE.CONTROL',
      'REGISTER_FILE': 'DECODE.REGISTER_FILE', 'ALU_A_MUX': 'EXECUTE.OPERAND_A',
      'ALU_B_MUX': 'EXECUTE.OPERAND_B', 'ALU': 'EXECUTE.ALU',
      'WRITEBACK_MUX': 'WRITEBACK.RESULT_SELECT', 'FETCH_REQUEST': 'FETCH.REQUEST',
      'DATA_REQUEST': 'MEMORY.REQUEST', 'LOAD_STORE': 'MEMORY.LANES',
    };
    const match = path.match(/^(.*\.CORE\.|RV32I_SINGLE_CYCLE_CORE_ROOT\.)(.*)$/);
    if (match) for (const [old, next] of Object.entries(moves)) {
      if (match[2] === old || match[2].startsWith(old + '.')) return match[1] + next + match[2].slice(old.length);
    }
    return path;
  };
  const rootPath = path => rootName && (path === "DUT" || path.startsWith("DUT."))
    ? rootName + path.slice(3) : path;
  const arithmeticPath = path => path.replace(
    /^((?:MULTIPLIER_W\d+_ROOT|DIVIDER_W\d+_ROOT|.*\.(?:MULTIPLY|DIVIDE)))\.(BUSY_STATE|DONE_STATE|ITERATION|MULTIPLICAND|ORIGINAL_MULTIPLIER|NEGATIVE_A|NEGATIVE_B|DIVISOR|QUOTIENT_NEGATIVE|REMAINDER_NEGATIVE|LOW_OR_QUOTIENT|HIGH_OR_REMAINDER|STEP)(?=\.|$)/,
    (_, prefix, child) => prefix + '.' + (
      ['BUSY_STATE','DONE_STATE','ITERATION'].includes(child) ? 'CONTROL' :
      ['LOW_OR_QUOTIENT','HIGH_OR_REMAINDER','STEP'].includes(child) ? 'DATAPATH' : 'OPERANDS') + '.' + child);
  const devicePath = path => {
    const parts=path.split('.');
    if(!/^(CLOCKED_COUNTER|SYNCHRONOUS_FIFO|DEVICE_REGISTER_PORT|BUS_ERROR_TARGET|ADDRESS_ROUTER|INSTRUCTION_ACCESS_GUARD|CONSOLE_DEVICE|TIMER_DEVICE|BUTTON_DEVICE|FRAMEBUFFER_DISPLAY|RV32IM_DEVICE_SYSTEM).*_ROOT$/.test(parts[0]))return path;
    for(let index=0;index<parts.length-1;index++) {
      const name=parts[index],child=parts[index+1];let move=null;
      if(name==='PORT'||(index===0&&name.startsWith('DEVICE_REGISTER_PORT'))) {
        if(['OP','ADDRESS','DATA','MASK','CHECKED_STATUS'].includes(child))move=['REQUEST',child];
        else if(['PENDING','EXECUTED'].includes(child))move=['CONTROL',child];
      } else if(['TX_FIFO','EVENTS'].includes(name)||(index===0&&name.startsWith('SYNCHRONOUS_FIFO'))) {
        if(['HEAD','TAIL'].includes(child))move=['POINTERS',child];
        else if(['COUNT_STATE','OVERFLOW_STATE'].includes(child))move=['CONTROL',child];
        else if(/^SLOT_\d+$/.test(child))move=['STORAGE',child];
      } else if(name==='COUNTER'||(index===0&&name.startsWith('CLOCKED_COUNTER'))) {
        if(child==='DIVIDER')move=['TICK_CONTROL','PHASE'];
      } else if(['DATA_ROUTER','FETCH_GUARD'].includes(name)||(index===0&&/^(ADDRESS_ROUTER|INSTRUCTION_ACCESS_GUARD)/.test(name))) {
        if(['BUSY','SENT'].includes(child))move=['CONTROL',child];
        else if(child==='STATUS'||/^(HELD_|OWNER_)/.test(child))move=['REQUEST',child];
      } else if(name==='DISPLAY'||(index===0&&name.startsWith('FRAMEBUFFER_DISPLAY'))) {
        if(child==='FRAME_COUNTER')move=['FRAME_CONTROL',child];
      }
      if(move)return [...parts.slice(0,index+1),...move,...parts.slice(index+2)].join('.');
    }
    return path;
  };
  const currentPath = path => devicePath(stagePath(ffPath(arithmeticPath(rootPath(path)))))
    .replace(/^(REGISTER32_ROOT|RV32I[^.]*\.(?:[A-Za-z0-9_]+\.)*[A-Za-z0-9_]+_STATE)\.STORAGE$/, '$1')
    .replace(
      /^(REGISTER32_ROOT|RV32I[^.]*\.(?:[A-Za-z0-9_]+\.)*[A-Za-z0-9_]+_STATE)\.(?:STORAGE\.)?(CELL_([0-9]|[12][0-9]|3[01])|DATA|OUTPUT|D_SPLIT|Q_JOIN)(?=\.|$)/,
      (_match, parent, old, bit) => `${parent}.${bit !== undefined ? 'BIT_' + bit : {DATA:'INPUT_BITS',OUTPUT:'OUTPUT_BITS',D_SPLIT:'INPUT_BITS',Q_JOIN:'OUTPUT_BITS'}[old]}`,
    )
    .replace(
      /(^|\.)(REGISTER_FILE|REGISTER_FILE_W32_N32_ROOT)\.STORAGE\.WORD_(\d+)\.(CELL_(\d+)|DATA|OUTPUT)(?=\.|$)/,
      (_match, dot, parent, word, old, bit) => `${dot}${parent}.STORAGE.WORD_${word}.${bit !== undefined ? 'BIT_' + bit : {DATA:'INPUT_BITS',OUTPUT:'OUTPUT_BITS'}[old]}`,
    );
  const result = Object.fromEntries(Object.entries(migrated).map(([path, fidelity]) => [currentPath(path), fidelity]));
  for (const [path, fidelity] of Object.entries(migrated)) if (currentPath(path) === path) result[path] = fidelity;
  return Object.fromEntries(Object.entries(result).sort(([left], [right]) => left.localeCompare(right)));
}

function normalizeProfileStore(value) {
  const normalized = { schema_version: 1, scenarios: {} };
  if (!value || value.schema_version !== 1 || !value.scenarios || typeof value.scenarios !== "object") {
    return normalized;
  }
  for (let [scenario, entry] of Object.entries(value.scenarios)) {
    if (!entry || typeof entry !== "object") continue;
    const baseScenario=scenario.split('::')[0];
    // A profile saved before every scenario had a named fidelity profile
    // belongs to the scenario's default profile.
    if (!scenario.includes('::')) {
      const descriptor = scenariosByAlias.get(baseScenario.toLowerCase().replaceAll('_', '-'));
      if (!descriptor) continue;
      scenario = `${scenario}::${defaultFidelity(descriptor)}`;
      if (scenario in value.scenarios) continue;
    }
    const rootName = Object.entries(scenarioCatalog?.components || {}).find(
      ([test]) => test === baseScenario || (scenarioCatalog.aliases?.[test] || []).includes(baseScenario),
    )?.[1]?.root;
    normalized.scenarios[scenario] = {
      revision: Math.max(0, Number.isInteger(entry.revision) ? entry.revision : 0),
      exact_overrides: migrateProfileOverrides(entry.exact_overrides || {}, rootName),
    };
  }
  return normalized;
}

function colorForLayoutType(layoutType) {
  const fallback = (palette && palette.default_color) || [61, 90, 128, 190];
  if (!palette) return clone(fallback);
  for (const [prefix, color] of Object.entries(palette.prefix_colors || {})) {
    if (layoutType.startsWith(prefix)) return clone(color);
  }
  return clone((palette.type_colors || {})[layoutType] || fallback);
}

function aliasesFor(name) { return catalogAliasesFor(name, scenarioCatalog); }
function componentLayout(component, layout, layoutKey) {
  const types = layout.type_layouts || {};
  const base = { ...(types[component.type] || {}) };
  if (component.layoutType?.startsWith(`${component.type}[config-`)) delete base.children;
  if (component.type === "Rewire" && component.layoutType && component.layoutType !== component.type) {
    delete base.aspect_ratio;
    delete base.min_aspect_ratio;
  }
  let specialized = component.layoutType && component.layoutType !== component.type
    ? mergeLayout(base, types[component.layoutType] || {})
    : base;
  const fingerprint = component.profileFingerprint;
  if (fingerprint && fingerprint !== "explicit" && component.type === "Rewire" && component.layoutType !== component.type) {
    const legacy = { ...(layout.profile_layouts?.[fingerprint]?.Rewire || {}) };
    delete legacy.aspect_ratio;
    delete legacy.min_aspect_ratio;
    specialized = mergeLayout(specialized, legacy);
  }
  const profile = fingerprint && fingerprint !== "explicit"
    ? ((((layout.profile_layouts || {})[fingerprint] || {})[component.layoutType || component.type]) || {})
    : {};
  const typed = mergeLayout(specialized, profile);
  if (component.depth !== 0) return typed;
  return mergeLayout(typed, ((layout.root_layouts || {})[layoutKey]) || {});
}

function enrichPayload(payload, layout, attachLayout = true) {
  const settings = layout.default_settings || {};
  for (const component of payload.components || []) {
    const entry = componentLayout(component, layout, payload.layoutKey);
    const titleRatio = Number(entry.title_bar_ratio ?? settings.title_bar_ratio ?? 0.15);
    const pinRatio = Number(entry.pin_size_ratio ?? settings.pin_size_ratio ?? 0.1);
    const pinCount = Math.max(component.inputPinCount || 0, component.outputPinCount || 0);
    const aspectFloor = Number(entry.aspect_ratio_floor ?? 0.35);
    const computedMinimum = pinCount
      ? Math.max(aspectFloor, titleRatio + pinRatio * 1.15 * (pinCount + 1))
      : Math.max(aspectFloor, titleRatio + pinRatio);
    component.minAspectRatio = Math.max(computedMinimum, Number(entry.min_aspect_ratio ?? computedMinimum));
    component.aspectRatio = Math.max(
      component.minAspectRatio,
      Number(entry.aspect_ratio ?? component.minAspectRatio),
    );
    component.color = clone(entry.color || colorForLayoutType(component.layoutType || component.type));
  }
  if(attachLayout)payload.layout = clone(layout);
  payload.runtime = "wasm";
  return payload;
}

function openDatabase() {
  if (typeof indexedDB === "undefined") return Promise.resolve(null);
  if (databasePromise) return databasePromise;
  databasePromise = new Promise((resolve, reject) => {
    const request = indexedDB.open(DB_NAME, DB_VERSION);
    request.addEventListener("upgradeneeded", () => {
      if (!request.result.objectStoreNames.contains("state")) {
        request.result.createObjectStore("state");
      }
    });
    request.addEventListener("success", () => resolve(request.result));
    request.addEventListener("error", () => {
      databasePromise = null;
      reject(request.error);
    });
  });
  return databasePromise;
}

async function loadStored(key, fallback) {
  const database = await openDatabase();
  if (!database) return clone(memoryStore.has(key) ? memoryStore.get(key) : fallback);
  return new Promise((resolve, reject) => {
    const transaction = database.transaction("state", "readonly");
    const request = transaction.objectStore("state").get(key);
    request.addEventListener("success", () => resolve(clone(request.result ?? fallback)));
    request.addEventListener("error", () => reject(request.error));
  });
}

async function saveStored(key, value) {
  const database = await openDatabase();
  if (!database) {
    memoryStore.set(key, clone(value));
    return;
  }
  await new Promise((resolve, reject) => {
    const transaction = database.transaction("state", "readwrite");
    transaction.objectStore("state").put(clone(value), key);
    transaction.addEventListener("complete", resolve);
    transaction.addEventListener("error", () => reject(transaction.error));
  });
}

async function initialize() {
  if (modulePromise) return modulePromise;
  modulePromise = (async () => {
    const started = performance.now();
    const assetsPromise = Promise.all([
      fetch(new URL("./layout.json", import.meta.url)),
      fetch(new URL("./profiles.json", import.meta.url)),
      fetch(new URL("./palette.json", import.meta.url)),
      fetch(new URL('./scenarios.json', import.meta.url)),
    ]);
    const module = await createCircuitSimModule({
      locateFile: (path) => new URL(`${path}?v=16`, import.meta.url).toString(),
      print: (...args) => console.log("[CircuitSim WASM]", ...args),
      printErr: (...args) => console.error("[CircuitSim WASM]", ...args),
    });
    wasmModule=module;
    bridge = new module.WasmVisualizer();
    descriptors = JSON.parse(bridge.visualizableTestsJson())
      .filter((item) => item.visualizable && !NON_VISUALIZABLE.has(item.name));
    const [layoutResponse, profileResponse, paletteResponse, catalogResponse] = await assetsPromise;
    if (!catalogResponse.ok) throw new Error('Unable to load the scenario catalog');
    scenarioCatalog = await catalogResponse.json();
    scenarioOptions = makeScenarioOptions(descriptors, scenarioCatalog);
    for (const item of descriptors) {
      const aliases = aliasesFor(item.name);
      const key = aliases[0] || defaultAlias(item.name);
      const allAliases = new Set([key, defaultAlias(item.name), item.name.toLowerCase(), ...aliases]);
      for (const alias of allAliases) scenariosByAlias.set(alias.toLowerCase().replaceAll('_', '-'), { ...item, key });
    }
    if (!layoutResponse.ok) throw new Error("Unable to load the bundled CircuitSim layout");
    if (!profileResponse.ok) throw new Error("Unable to load the bundled CircuitSim profiles");
    if (!paletteResponse.ok) throw new Error("Unable to load the bundled CircuitSim palette");
    defaultLayout = await layoutResponse.json();
    defaultProfiles = normalizeProfileStore(await profileResponse.json());
    palette = await paletteResponse.json();
    if (palette.schema_version !== 1) throw new Error("Unsupported CircuitSim palette schema");
    currentLayout = mergeLayoutStores(defaultLayout, await loadStored(LAYOUT_KEY, defaultLayout));
    console.log(`CircuitSim WebAssembly initialized in ${(performance.now() - started).toFixed(1)} ms`);
  })();
  return modulePromise;
}

function normalizeScenario(value) {
  const key = String(value || "ripple-carry-adder-w8").trim().toLowerCase().replaceAll("_", "-");
  const descriptor = scenariosByAlias.get(key);
  if (!descriptor) throw new Error(`Unknown scenario '${value}'`);
  return descriptor;
}

async function profileStore() {
  const local = normalizeProfileStore(await loadStored(
    PROFILE_KEY,
    { schema_version: 1, scenarios: {} },
  ));
  return {
    schema_version: 1,
    scenarios: {
      ...clone(defaultProfiles.scenarios),
      ...clone(local.scenarios),
    },
  };
}

async function localProfileStore() {
  return normalizeProfileStore(await loadStored(
    PROFILE_KEY,
    { schema_version: 1, scenarios: {} },
  ));
}

// The fidelity profile a scenario opens in (its catalog `fidelity`).
function defaultFidelity(descriptor) {
  const option = scenarioOptions.find(o => o.className === descriptor.name);
  if (!option?.fidelity) throw new Error(`No fidelity profile for scenario '${descriptor.key}'`);
  return option.fidelity;
}

async function loadScenario(scenarioValue, requestedOverrides = null, requestedRevision = null, coreFidelity = null, timing = null, fidelity = null) {
  const descriptor = normalizeScenario(scenarioValue);
  fidelity=normalizeFidelityPreset(fidelity || defaultFidelity(descriptor));
  // The timing profile the circuit is built and simulated under.
  bridge.setTimingProfile(timing || "unit");
  bridge.setFidelityProfile(fidelity);
  if (coreFidelity && (!/^rv32i-(?:(?:five-stage|baseline)-|predict-[a-z-]+-)?program[0-9]+$/.test(descriptor.key)
      || !["structural", "behavioral"].includes(coreFidelity))) {
    throw new TypeError("core_fidelity requires an RV32I program and structural or behavioral");
  }
  const store = await profileStore();
  const saved = store.scenarios[profileStorageKey(descriptor.key,fidelity)] || { revision: 0, exact_overrides: {} };
  const revision = requestedRevision == null ? Number(saved.revision || 0) : requestedRevision;
  const overrides = { ...(requestedOverrides == null ? (saved.exact_overrides || {}) : requestedOverrides) };
  if (coreFidelity) overrides["RV32I_PROGRAM_ROOT.CORE"] = coreFidelity;
  bridge.clearProfileOverrides();
  for (const [path, fidelity] of Object.entries(overrides)) {
    bridge.setProfileOverride(path, fidelity);
  }
  const payload = JSON.parse(bridge.loadScenario(descriptor.name, descriptor.key, revision));
  currentDefaults=clone(defaultLayout);
  enrichPayload(payload,currentDefaults,false);
  completeProfileLayouts(payload,currentDefaults,componentLayout);
  currentLayout = mergeLayoutStores(currentDefaults, await loadStored(LAYOUT_KEY, defaultLayout));
  enrichPayload(payload,currentLayout,false);
  completeProfileLayouts(payload,currentLayout,componentLayout);
  currentScenario = descriptor;
  payload.layout=clone(currentLayout);
  currentPayload = payload;
  currentPayload.timing = JSON.parse(bridge.timingProfileJson());
  return currentPayload;
}

function parseBody(message) {
  return message.body ? JSON.parse(message.body) : {};
}

function defaultPlacements(body) {
  if (!currentPayload) throw new Error("Load a scenario before requesting layouts");
  const parent = currentPayload.components.find((component) => component.id === body.parentId);
  if (!parent) throw new Error(`Unknown parent component '${body.parentId}'`);
  const entry = componentLayout(parent, currentDefaults || defaultLayout, currentPayload.layoutKey);
  const parentIndex = currentPayload.components.indexOf(parent);
  const childIndexes = [];
  currentPayload.components.forEach((component, index) => {
    if (component.parentIndex === parentIndex) childIndexes.push(index);
  });
  const placements = {};
  for (const childIndex of childIndexes) {
    const child = currentPayload.components[childIndex];
    const saved = (entry.children || {})[child.name];
    if (saved && saved.rel_pos && Number.isFinite(Number(saved.rel_width))) {
      placements[child.name] = clone(saved);
    }
  }
  if (body.childName) {
    if (!placements[body.childName]) {
      throw new Error(`Parent '${parent.name}' has no child layout named '${body.childName}'`);
    }
    return { [body.childName]: placements[body.childName] };
  }
  // Current committed topologies all have generated defaults.  A deterministic
  // grid remains available for a newly introduced component before regeneration.
  if (Object.keys(placements).length !== childIndexes.length && childIndexes.length) {
    const columns = Math.ceil(Math.sqrt(childIndexes.length));
    const rows = Math.ceil(childIndexes.length / columns);
    childIndexes.forEach((childIndex, index) => {
      const child = currentPayload.components[childIndex];
      if (placements[child.name]) return;
      placements[child.name] = {
        rel_pos: [(index % columns + 0.5) / columns, (Math.floor(index / columns) + 0.5) / rows],
        rel_width: Math.min(0.8 / columns, 0.8 / rows),
      };
    });
  }
  return placements;
}

// A long game's replay: the save named by the page (a file of this site,
// gzip-compressed) is handed to the module before the game-replay scenario
// is built from it.
const MAX_REPLAY_SAVE_BYTES = 64 * 1024 * 1024;
// A save of this site, gzip-compressed or not, as bytes.
async function fetchSave(path) {
  const url = new URL(path, self.location.href);
  if (url.origin !== self.location.origin) throw new Error("A replay save must come from this site");
  const response = await fetch(url);
  if (!response.ok) throw new Error(`Replay save ${url.pathname}: HTTP ${response.status}`);
  let bytes = new Uint8Array(await response.arrayBuffer());
  if (bytes[0] === 0x1f && bytes[1] === 0x8b) {
    bytes = new Uint8Array(await new Response(new Blob([bytes]).stream().pipeThrough(new DecompressionStream("gzip"))).arrayBuffer());
  }
  if (bytes.length > MAX_REPLAY_SAVE_BYTES) throw new Error("Replay save is too large");
  return bytes;
}
async function prepareReplay(query) {
  const url = new URL(query.get("replay"), self.location.href);
  if (url.origin !== self.location.origin) throw new Error("A replay save must come from this site");
  const fixture = query.get("fixture") || "";
  const cycles = Number(query.get("cycles") || 0);
  if (!/^[a-z0-9-]+-long$/.test(fixture)) throw new Error("A replay names its long game");
  if (!Number.isSafeInteger(cycles) || cycles < 0 || cycles > 100000) throw new Error("A replay runs 0 to 100000 cycles");
  bridge.setGameReplay(fixture, await fetchSave(url.pathname), cycles);
}

async function handleRequest(message) {
  await initialize();
  const path = message.path.includes("/api/")
    ? message.path.slice(message.path.indexOf("/api/"))
    : message.path;
  const query = new URLSearchParams(message.query || "");
  if (message.method === "GET" && path === "/api/health") {
    return JSON.stringify({ ok: true, runtime: "wasm" });
  }
  if (message.method === "GET" && path === "/api/scenarios") {
    const fallback = scenarioOptions[0] ? scenarioOptions[0].key : "";
    return JSON.stringify({
      default: scenariosByAlias.has("ripple-carry-adder-w8") ? "ripple-carry-adder-w8" : fallback,
      scenarios: scenarioOptions,
      groups: scenarioCatalog.groups,
      catalog: scenarioCatalog,
      runtime: "wasm",
    });
  }
  if (message.method === "GET" && path === "/api/circuit") {
    if (query.get("replay")) await prepareReplay(query);
    return JSON.stringify(await loadScenario(query.get("scenario") || "ripple-carry-adder-w8", null, null, query.get("core_fidelity"), query.get("timing"),query.get('fidelity')));
  }
  // A recorded game's session: more cycles, or another frame's save.
  if (message.method === "GET" && (path === "/api/game/advance" || path === "/api/game/frame")) {
    if (query.get("session") !== currentPayload?.sessionId) throw new Error("Unknown or expired visualizer session");
    if (path === "/api/game/advance") {
      const cycles = Number(query.get("cycles"));
      if (!Number.isSafeInteger(cycles) || cycles < 1 || cycles > 100000) throw new TypeError("Invalid cycle count");
      return query.get("history") === "0" ? bridge.gameFastForward(cycles) : bridge.gameAdvance(cycles);
    }
    return bridge.gameJump(await fetchSave(String(query.get("save") || "")));
  }
  if (message.method === "GET" && path === "/api/state") {
    if (query.get("session") && query.get("session") !== currentPayload?.sessionId) {
      throw new Error("Unknown or expired visualizer session");
    }
    return bridge.stateJson(Number(query.get("index") || 0));
  }
  if (message.method === "GET" && path === "/api/component-state") {
    if (query.get("session") && query.get("session") !== currentPayload?.sessionId) {
      throw new Error("Unknown or expired visualizer session");
    }
    const address = query.has("address") ? Number(query.get("address")) : -1;
    const count = query.has("count") ? Number(query.get("count")) : 64;
    if (!Number.isInteger(address) || address < -1 || address > 0xffffffff
        || !Number.isInteger(count) || count < 1 || count > 256) throw new TypeError("Invalid memory page");
    return bridge.memoryPageJson(
      Number(query.get("index") || 0),
      String(query.get("componentId") || ""),
      address, count,
    );
  }
  if (message.method === "POST" && path === "/api/topology") {
    const body = parseBody(message);
    if (body.session !== currentPayload?.sessionId) {
      throw new Error("Unknown or expired visualizer session");
    }
    return bridge.topologySliceJson(body.ownerIds || []);
  }
  if (message.method === "POST" && path === "/api/layout") {
    const body = parseBody(message);
    if (!body.layout || typeof body.layout !== "object") throw new Error("layout must be an object");
    currentLayout = clone(body.layout);
    // Store only the edits relative to the shipped layout (layout_changes.mjs).
    await saveStored(LAYOUT_KEY, layoutChanges(currentDefaults || defaultLayout, currentLayout));
    return JSON.stringify({ ok: true, storage: "indexeddb", format: LAYOUT_CHANGES_FORMAT });
  }
  if (message.method === "POST" && path === "/api/layout/defaults") {
    const body = parseBody(message);
    const parent = currentPayload?.components.find(
      (component) => component.id === body.parentId,
    );
    if (!parent) throw new Error(`Unknown parent component '${body.parentId}'`);
    return JSON.stringify({
      scenario: currentScenario ? currentScenario.key : "",
      layoutKey: currentPayload ? currentPayload.layoutKey : "",
      parentId: body.parentId,
      parentType: parent.type,
      parentLayoutType: parent.layoutType || parent.type,
      isRoot: parent.depth === 0,
      placements: defaultPlacements(body),
    });
  }
  if (message.method === "POST" && path === "/api/circuit/preview") {
    // The parts the circuit would have with the draft choices, built without
    // simulating: the explorer shows them in gray before Apply & Run.
    const body = parseBody(message);
    const descriptor = normalizeScenario(body.scenario);
    bridge.setFidelityProfile(normalizeFidelityPreset(body.fidelity || defaultFidelity(descriptor)));
    bridge.clearProfileOverrides();
    for (const [componentPath, fidelity] of Object.entries(body.exactOverrides || {})) {
      bridge.setProfileOverride(componentPath, fidelity);
    }
    const preview = bridge.previewComponentsJson(descriptor.name);
    previewComponentsById = new Map(JSON.parse(preview).components.map((component) => [component.id, component]));
    return preview;
  }
  if (message.method === "POST" && path === "/api/circuit/apply") {
    const body = parseBody(message);
    const descriptor = normalizeScenario(body.scenario);
    const fidelity=normalizeFidelityPreset(body.fidelity || defaultFidelity(descriptor));
    const storageKey=profileStorageKey(descriptor.key,fidelity);
    const store = await profileStore();
    const localStore = await localProfileStore();
    const saved = store.scenarios[storageKey] || { revision: 0, exact_overrides: {} };
    if (Number(body.revision) !== Number(saved.revision || 0)) {
      throw new Error("Profile changed since it was loaded; reload the scenario before applying again");
    }
    const overrides = body.exactOverrides || {};
    const componentById = new Map((currentPayload?.components || []).map((component) => [component.id, component]));
    for (const [path, fidelity] of Object.entries(overrides)) {
      // Match the native profile store: a saved descendant can be temporarily
      // absent beneath a behavioral parent. Preserve it on expand/collapse;
      // only newly added or changed overrides must target a visible component.
      if (saved.exact_overrides?.[path] === fidelity) continue;
      // A part shown only in the preview may also receive a choice.
      const component = componentById.get(path) || previewComponentsById.get(path);
      if (!component) throw new Error(`Cannot add an override for unknown component '${path}'`);
      if (!(component.availableFidelities || []).includes(fidelity)) {
        throw new Error(`Component '${path}' does not provide ${fidelity} fidelity`);
      }
    }
    const revision = Number(saved.revision || 0) + 1;
    const payload = await loadScenario(descriptor.key, overrides, revision, null, body.timing || currentPayload?.timing?.name,fidelity);
    localStore.scenarios[storageKey] = { revision, exact_overrides: clone(overrides) };
    await saveStored(PROFILE_KEY, localStore);
    return JSON.stringify(payload);
  }
  throw new Error(`Unsupported WebAssembly visualizer request: ${message.method} ${path}`);
}

self.addEventListener("message", async (event) => {
  const message = event.data || {};
  try {
    const body = await handleRequest(message);
    self.postMessage({ id: message.id, status: 200, body });
  } catch (error) {
    console.error(error);
    self.postMessage({
      id: message.id,
      status: error instanceof TypeError ? 400 : 500,
      body: (()=>{
        if(Array.isArray(error?.message))return String(error.message[1] || error.message[0]);
        if(error?.message)return String(error.message);
        if(error?.excPtr && wasmModule?.getExceptionMessage){
          try{const details=wasmModule.getExceptionMessage(error);return String(details[1] || details[0]);}catch{}
        }
        return String(error);
      })(),
    });
  }
});
