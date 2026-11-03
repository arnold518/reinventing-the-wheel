// Browser layout storage for the serverless (WebAssembly) visualizer.
//
// The browser keeps only the viewer's edits: the entries, fields and child
// placements that differ from the bundled layout.json. Every entry left
// unedited therefore follows the shipped layout of each new release, and a
// save stores kilobytes rather than the whole layout.
import { upgradeDefaultLayout } from "./layout_migration.mjs?v=4";

export const LAYOUT_CHANGES_FORMAT = "layout-changes-v1";

// Kept with every edited entry, so the revision upgrades in
// layout_migration.mjs know which shipped default the edit was made on.
const REVISION_FIELDS = ["layout_revision", "automatic_layout_revision", "layout_mode"];
const SECTIONS = ["type_layouts", "root_layouts"];

const clone = value => JSON.parse(JSON.stringify(value));
const isObject = value => value !== null && typeof value === "object" && !Array.isArray(value);

// Key order does not matter for equality.
function canonical(value) {
  if (Array.isArray(value)) return `[${value.map(canonical).join(",")}]`;
  if (isObject(value)) {
    return `{${Object.keys(value).sort().map(key => `${JSON.stringify(key)}:${canonical(value[key])}`).join(",")}}`;
  }
  return JSON.stringify(value);
}
const same = (a, b) => canonical(a) === canonical(b);

export function mergeLayout(base, override) {
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

// The stored record: the current changes format, or a full snapshot saved
// before it (still accepted; the next save stores it as changes).
export function savedLayoutChanges(stored) {
  if (stored?.format === LAYOUT_CHANGES_FORMAT) return stored.layout || {};
  return stored || {};
}

// Apply saved changes (or an older full snapshot) over the shipped defaults.
// Missing defaults are filled, recognized legacy ALU child topologies reflow,
// and revisioned entries upgrade placements still equal to an old default.
export function mergeLayoutStores(defaults, stored) {
  const saved = savedLayoutChanges(stored);
  const mergeEntries = (base = {}, overrides = {}, fallback = {}) => Object.fromEntries(
    [...new Set([...Object.keys(base), ...Object.keys(overrides)])].map(
      key => [key, mergeLayout(base[key], upgradeDefaultLayout(base[key] || fallback[key], overrides[key]))],
    ),
  );
  const profiles = {};
  for (const key of new Set([
    ...Object.keys(defaults.profile_layouts || {}),
    ...Object.keys(saved.profile_layouts || {}),
  ])) {
    profiles[key] = mergeEntries(defaults.profile_layouts?.[key], saved.profile_layouts?.[key], defaults.type_layouts);
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

// One entry's edits: changed fields, and changed child placements by name.
// A field removed by the viewer cannot be expressed and is not stored.
function entryChanges(base, edited) {
  if (edited === undefined || same(base, edited)) return undefined;
  if (!isObject(base) || !isObject(edited)) return clone(edited);
  const result = {};
  for (const [key, value] of Object.entries(edited)) {
    if (key === "children" && isObject(value)) {
      // mergeLayout gives every entry a children object, empty if none shipped.
      const shipped = isObject(base.children) ? base.children : {};
      const children = {};
      for (const [name, child] of Object.entries(value)) {
        if (!same(shipped[name], child)) children[name] = clone(child);
      }
      if (Object.keys(children).length) result.children = children;
    } else if (!same(base[key], value)) {
      result[key] = clone(value);
    }
  }
  if (!Object.keys(result).length) return undefined;
  for (const key of REVISION_FIELDS) {
    if (edited[key] !== undefined && result[key] === undefined) result[key] = edited[key];
  }
  return result;
}

function sectionChanges(base = {}, edited = {}) {
  const result = {};
  for (const [key, entry] of Object.entries(edited)) {
    const changes = entryChanges(base[key], entry);
    if (changes !== undefined) result[key] = changes;
  }
  return result;
}

// The record to store for `layout`, relative to the `defaults` it was built
// from. mergeLayoutStores(defaults, record) gives back the edited layout.
export function layoutChanges(defaults, layout) {
  const changes = {};
  for (const [key, value] of Object.entries(layout)) {
    if (key === "default_settings" || key === "profile_layouts" || SECTIONS.includes(key)) continue;
    if (!same(defaults[key], value)) changes[key] = clone(value);
  }
  const settings = {};
  for (const [key, value] of Object.entries(layout.default_settings || {})) {
    if (!same(defaults.default_settings?.[key], value)) settings[key] = clone(value);
  }
  if (Object.keys(settings).length) changes.default_settings = settings;
  for (const section of SECTIONS) {
    const entries = sectionChanges(defaults[section], layout[section]);
    if (Object.keys(entries).length) changes[section] = entries;
  }
  const profiles = {};
  for (const [fingerprint, entries] of Object.entries(layout.profile_layouts || {})) {
    const changed = sectionChanges(defaults.profile_layouts?.[fingerprint], entries);
    if (Object.keys(changed).length) profiles[fingerprint] = changed;
  }
  if (Object.keys(profiles).length) changes.profile_layouts = profiles;
  return { format: LAYOUT_CHANGES_FORMAT, layout: changes };
}
