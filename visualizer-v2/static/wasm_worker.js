import createCircuitSimModule from "./circuitsim_wasm.js";

const DB_NAME = "circuitsim-visualizer-v2";
const DB_VERSION = 1;
const LAYOUT_KEY = "layout";
const PROFILE_KEY = "profiles";
const NON_VISUALIZABLE = new Set(["WireTemplateTest", "RewireValidationTest"]);

const PREFERRED_ALIASES = {
  FullAdderTest: ["full-adder", "fulladder"],
  HalfAdderTest: ["half-adder", "halfadder"],
  FullCircuitTest: ["full-circuit", "fullcircuit"],
  SRLatchTest: ["sr-latch", "srlatch"],
  GatedDLatchTest: ["gated-d-latch", "gateddlatch", "d-latch"],
  DFlipFlopTest: ["dff", "d-flip-flop"],
  ClockGeneratorTest: ["clock"],
  MemoryBitTest: ["memory-bit", "memorybit"],
  Memory64Kx32Test: ["memory64kx32", "memory-64kx32"],
  Register32Test: ["register32", "register-32"],
  RegisterFile4x32Test: ["register-file4x32", "register-file-4x32", "registerfile4x32", "rf4x32"],
  RegisterFile32x32Test: ["rv32i-register-file", "register-file32x32", "register-file-32x32", "registerfile32x32", "rf32x32"],
  Memory4x32Test: ["memory4x32", "memory-4x32", "mem4x32"],
  Memory32x32Test: ["memory32x32", "memory-32x32", "mem32x32"],
  ConstantValue1Test: ["constant1"],
  ConstantValue2Test: ["constant2"],
  ConstantValue4Test: ["constant4"],
  ConstantValue32Test: ["constant32"],
  Adder8Test: ["adder8", "8-bit-adder"],
  ZeroDetect8Test: ["zero-detect8"],
  ALU8Test: ["alu8"],
  AddSub8Test: ["addsub8"],
  ArithmeticUnit8Test: ["arithmetic8"],
  LogicUnit8Test: ["logic8"],
  BarrelShifter8Test: ["barrel-shifter8"],
  CompareSelect8Test: ["compare-select8"],
  ALU8FlagUnitTest: ["alu8-flags"],
  ALU8ControlDecoderTest: ["alu8-control"],
  ALU8ResultSelectorTest: ["alu8-result-selector"],
  ALU32ControlDecoderTest: ["alu32-control"],
  ALU32CompareUnitTest: ["alu32-compare"],
  ALU32ResultSelectorTest: ["alu32-result-selector"],
  ALU32FlagUnitTest: ["alu32-flags"],
  Mux32to1Test: ["mux32to1"],
  Mux4to1_32bitTest: ["mux4to1-32bit"],
  Mux32to1_32bitTest: ["mux32to1-32bit"],
  Decoder2to4Test: ["decoder2to4", "decoder-2to4", "decoder-2-4"],
  Decoder5to32Test: ["decoder5to32", "decoder-5to32", "decoder-5-32"],
  RewireTest: ["rewire"],
  Adder32Test: ["adder32"],
  AddSub32Test: ["addsub32", "add-sub32"],
  Logic32Test: ["logic32"],
  ZeroDetect32Test: ["zero-detect32"],
  Comparator32Test: ["comparator32"],
  Shifter32Test: ["shifter32"],
  ALU32Test: ["alu32", "rv32i-alu32"],
  ALU32RepresentativeSliceTest: ["alu32-slice"],
  RV32ISingleCycleCoreTest: ["rv32i-core"],
  RV32IFiveStageCoreTest: ["rv32i-five-stage-core", "rv32i-pipeline-core"],
  RV32IControlFlowUnitTest: ["rv32i-control-flow"],
  RV32IDecodeControlUnitTest: ["rv32i-decode-control", "rv32i-decode"],
  RV32IExecutionControlStatusUnitTest: ["rv32i-execution-status", "rv32i-status"],
  RV32IProgramLoaderTest: ["rv32i-program-loader", "rv32i-program", "program-loader"],
};

let modulePromise = null;
let bridge = null;
let defaultLayout = null;
let defaultProfiles = { schema_version: 1, scenarios: {} };
let palette = null;
let currentLayout = null;
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

function migrateProfileOverrides(overrides) {
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
  return Object.fromEntries(Object.entries(migrated).sort(([left], [right]) => left.localeCompare(right)));
}

function normalizeProfileStore(value) {
  const normalized = { schema_version: 1, scenarios: {} };
  if (!value || value.schema_version !== 1 || !value.scenarios || typeof value.scenarios !== "object") {
    return normalized;
  }
  for (const [scenario, entry] of Object.entries(value.scenarios)) {
    if (!entry || typeof entry !== "object") continue;
    normalized.scenarios[scenario] = {
      revision: Math.max(0, Number.isInteger(entry.revision) ? entry.revision : 0),
      exact_overrides: migrateProfileOverrides(entry.exact_overrides || {}),
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

function defaultAlias(className) {
  const stem = className.endsWith("Test") ? className.slice(0, -4) : className;
  return stem
    .replaceAll("_", "-")
    .replace(/([A-Z]+)([A-Z][a-z])/g, "$1-$2")
    .replace(/([a-z0-9])([A-Z])/g, "$1-$2")
    .toLowerCase();
}

function aliasesFor(name) {
  const single = name.match(/^RV32ISingleCycleSystemTest\/program-([0-9]+)$/);
  if (single) return [`rv32i-program${Number(single[1])}`];
  const five = name.match(/^RV32IFiveStageCoreProgramTest\/program-([0-9]+)$/);
  if (five) return [`rv32i-five-stage-program${Number(five[1])}`];
  return PREFERRED_ALIASES[name] || [defaultAlias(name)];
}

function mergeLayout(base, override) {
  // Only the two reorganized ALUs need a one-time child-topology reflow.
  // Keep styling and all unrelated/user-adjusted current-topology layouts.
  const regroupedAlu = base?.children?.CONTROL && base?.children?.RESULT_SELECT;
  const legacyAlu = override?.children?.OPERATION_DECODER || override?.children?.RESULT_LOW_MUX;
  return {
    ...(base || {}),
    ...(override || {}),
    children: {
      ...(((base || {}).children) || {}),
      ...((regroupedAlu && legacyAlu) ? {} : (((override || {}).children) || {})),
    },
  };
}

// A saved browser snapshot can predate newly shipped component layouts.
// Fill missing defaults, reflow recognized legacy ALU child topologies, and
// keep other saved settings and placements.
function mergeLayoutStores(defaults, saved) {
  const mergeEntries = (base = {}, overrides = {}) => Object.fromEntries(
    [...new Set([...Object.keys(base), ...Object.keys(overrides)])].map(
      key => [key, mergeLayout(base[key], overrides[key])],
    ),
  );
  const profiles = {};
  for (const key of new Set([
    ...Object.keys(defaults.profile_layouts || {}),
    ...Object.keys(saved.profile_layouts || {}),
  ])) {
    profiles[key] = mergeEntries(defaults.profile_layouts?.[key], saved.profile_layouts?.[key]);
  }
  return {
    ...defaults,
    ...saved,
    default_settings: { ...defaults.default_settings, ...saved.default_settings },
    type_layouts: mergeEntries(defaults.type_layouts, saved.type_layouts),
    root_layouts: mergeEntries(defaults.root_layouts, saved.root_layouts),
    profile_layouts: profiles,
  };
}

function componentLayout(component, layout, layoutKey) {
  const types = layout.type_layouts || {};
  const base = types[component.type] || {};
  const specialized = component.layoutType && component.layoutType !== component.type
    ? mergeLayout(base, types[component.layoutType] || {})
    : base;
  const fingerprint = component.profileFingerprint;
  const profile = fingerprint && fingerprint !== "explicit"
    ? ((((layout.profile_layouts || {})[fingerprint] || {})[component.layoutType || component.type]) || {})
    : {};
  const typed = mergeLayout(specialized, profile);
  if (component.depth !== 0) return typed;
  return mergeLayout(typed, ((layout.root_layouts || {})[layoutKey]) || {});
}

function enrichPayload(payload, layout) {
  const settings = layout.default_settings || {};
  for (const component of payload.components || []) {
    const entry = componentLayout(component, layout, payload.layoutKey);
    const titleRatio = Number(entry.title_bar_ratio ?? settings.title_bar_ratio ?? 0.15);
    const pinRatio = Number(entry.pin_size_ratio ?? settings.pin_size_ratio ?? 0.1);
    const pinCount = Math.max(component.inputPinCount || 0, component.outputPinCount || 0);
    const computedMinimum = pinCount
      ? Math.max(0.35, titleRatio + pinRatio * 1.15 * (pinCount + 1))
      : Math.max(0.35, titleRatio + pinRatio);
    component.minAspectRatio = Number(entry.min_aspect_ratio ?? computedMinimum);
    component.aspectRatio = Math.max(
      component.minAspectRatio,
      Number(entry.aspect_ratio ?? component.minAspectRatio),
    );
    component.color = clone(entry.color || colorForLayoutType(component.layoutType || component.type));
  }
  payload.layout = clone(layout);
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
    ]);
    const module = await createCircuitSimModule({
      locateFile: (path) => new URL(path, import.meta.url).toString(),
      print: (...args) => console.log("[CircuitSim WASM]", ...args),
      printErr: (...args) => console.error("[CircuitSim WASM]", ...args),
    });
    bridge = new module.WasmVisualizer();
    descriptors = JSON.parse(bridge.visualizableTestsJson())
      .filter((item) => item.visualizable && !NON_VISUALIZABLE.has(item.name));
    scenarioOptions = descriptors.map((item) => {
      const aliases = aliasesFor(item.name);
      const key = aliases[0] || defaultAlias(item.name);
      const allAliases = new Set([key, defaultAlias(item.name), item.name.toLowerCase(), ...aliases]);
      for (const alias of allAliases) scenariosByAlias.set(alias, { ...item, key });
      return { key, className: item.name };
    });
    const [layoutResponse, profileResponse, paletteResponse] = await assetsPromise;
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
  const key = String(value || "adder8").trim().toLowerCase().replaceAll("_", "-");
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

async function loadScenario(scenarioValue, requestedOverrides = null, requestedRevision = null) {
  const descriptor = normalizeScenario(scenarioValue);
  const store = await profileStore();
  const saved = store.scenarios[descriptor.key] || { revision: 0, exact_overrides: {} };
  const revision = requestedRevision == null ? Number(saved.revision || 0) : requestedRevision;
  const overrides = requestedOverrides == null ? (saved.exact_overrides || {}) : requestedOverrides;
  bridge.clearProfileOverrides();
  for (const [path, fidelity] of Object.entries(overrides)) {
    bridge.setProfileOverride(path, fidelity);
  }
  const payload = JSON.parse(bridge.loadScenario(descriptor.name, descriptor.key, revision));
  currentLayout = mergeLayoutStores(defaultLayout, await loadStored(LAYOUT_KEY, defaultLayout));
  currentScenario = descriptor;
  currentPayload = enrichPayload(payload, currentLayout);
  return currentPayload;
}

function parseBody(message) {
  return message.body ? JSON.parse(message.body) : {};
}

function defaultPlacements(body) {
  if (!currentPayload) throw new Error("Load a scenario before requesting layouts");
  const parent = currentPayload.components.find((component) => component.id === body.parentId);
  if (!parent) throw new Error(`Unknown parent component '${body.parentId}'`);
  const entry = componentLayout(parent, defaultLayout, currentPayload.layoutKey);
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
      default: scenariosByAlias.has("adder8") ? "adder8" : fallback,
      scenarios: scenarioOptions,
      runtime: "wasm",
    });
  }
  if (message.method === "GET" && path === "/api/circuit") {
    return JSON.stringify(await loadScenario(query.get("scenario") || "adder8"));
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
    return bridge.componentStateJson(
      Number(query.get("index") || 0),
      String(query.get("componentId") || ""),
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
    await saveStored(LAYOUT_KEY, currentLayout);
    return JSON.stringify({ ok: true, storage: "indexeddb" });
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
  if (message.method === "POST" && path === "/api/circuit/apply") {
    const body = parseBody(message);
    const descriptor = normalizeScenario(body.scenario);
    const store = await profileStore();
    const localStore = await localProfileStore();
    const saved = store.scenarios[descriptor.key] || { revision: 0, exact_overrides: {} };
    if (Number(body.revision) !== Number(saved.revision || 0)) {
      throw new Error("Profile changed since it was loaded; reload the scenario before applying again");
    }
    const overrides = body.exactOverrides || {};
    const componentById = new Map((currentPayload?.components || []).map((component) => [component.id, component]));
    for (const [path, fidelity] of Object.entries(overrides)) {
      const component = componentById.get(path);
      if (!component) throw new Error(`Cannot add an override for unknown component '${path}'`);
      if (!(component.availableFidelities || []).includes(fidelity)) {
        throw new Error(`Component '${path}' does not provide ${fidelity} fidelity`);
      }
    }
    const revision = Number(saved.revision || 0) + 1;
    const payload = await loadScenario(descriptor.key, overrides, revision);
    localStore.scenarios[descriptor.key] = { revision, exact_overrides: clone(overrides) };
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
      body: String(error && error.message ? error.message : error),
    });
  }
});
