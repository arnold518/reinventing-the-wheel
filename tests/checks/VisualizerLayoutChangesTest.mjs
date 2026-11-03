// The serverless visualizer stores only layout edits relative to the shipped
// layout.json: a save round-trips, stays small, and later shipped defaults
// still reach every entry and child placement the viewer did not edit.
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {LAYOUT_CHANGES_FORMAT, layoutChanges, mergeLayoutStores, savedLayoutChanges}
  from '../../visualizer-v2/static/layout_changes.mjs';

const shipped = JSON.parse(readFileSync(new URL('../../visualizer-v2/layout.json', import.meta.url)));
const clone = value => structuredClone(value);
const canonical = value => Array.isArray(value) ? `[${value.map(canonical).join(',')}]`
  : value && typeof value === 'object'
    ? `{${Object.keys(value).sort().map(key => `${JSON.stringify(key)}:${canonical(value[key])}`).join(',')}}`
    : JSON.stringify(value);
const assertSameLayout = (actual, expected, message) => assert.equal(canonical(actual), canonical(expected), message);

// An entry with at least two children to edit one and leave the other.
const [typeKey] = Object.entries(shipped.type_layouts)
  .find(([, entry]) => Object.keys(entry.children || {}).length >= 2 && !entry.automatic_layout_revision);
const [moved, untouched] = Object.keys(shipped.type_layouts[typeKey].children);
const [colorKey] = Object.keys(shipped.type_layouts).filter(key => key !== typeKey);

// Nothing edited: nothing stored.
const loaded = mergeLayoutStores(shipped, {});
assert.deepEqual(layoutChanges(shipped, loaded), {format: LAYOUT_CHANGES_FORMAT, layout: {}});

// Edits: one child placement, one color, one setting, one new root layout.
const edited = clone(loaded);
edited.type_layouts[typeKey].children[moved] = {...edited.type_layouts[typeKey].children[moved], rel_pos: [0.123, 0.456]};
edited.type_layouts[colorKey].color = [1, 2, 3, 200];
edited.default_settings.title_bar_ratio = 0.2;
edited.root_layouts['test-scenario::Root'] = {aspect_ratio: 1.5, children: {}};
const record = layoutChanges(shipped, edited);
assert.equal(record.format, LAYOUT_CHANGES_FORMAT);
assert.deepEqual(Object.keys(record.layout).sort(), ['default_settings', 'root_layouts', 'type_layouts']);
assert.deepEqual(Object.keys(record.layout.type_layouts).sort(), [typeKey, colorKey].sort());
assert.deepEqual(Object.keys(record.layout.type_layouts[typeKey].children), [moved], 'only the moved child');
assert.deepEqual(record.layout.default_settings, {title_bar_ratio: 0.2});
const bytes = JSON.stringify(record).length, full = JSON.stringify(edited).length;
assert(bytes < 4096 && bytes * 1000 < full, `stored ${bytes} bytes of ${full}`);
assertSameLayout(mergeLayoutStores(shipped, record), edited, 'saved edits round-trip');

// A later release changes the edited entry's other child and an unedited entry.
const release = clone(shipped);
release.type_layouts[typeKey].children[untouched] = {...release.type_layouts[typeKey].children[untouched], rel_width: 0.0777};
const [otherKey] = Object.keys(release.type_layouts).filter(key => key !== typeKey && key !== colorKey);
release.type_layouts[otherKey] = {...release.type_layouts[otherKey], aspect_ratio: 3.21};
const upgraded = mergeLayoutStores(release, record);
assert.deepEqual(upgraded.type_layouts[typeKey].children[moved].rel_pos, [0.123, 0.456], 'edit kept');
assert.equal(upgraded.type_layouts[typeKey].children[untouched].rel_width, 0.0777, 'new default reaches the unedited child');
assert.equal(upgraded.type_layouts[otherKey].aspect_ratio, 3.21, 'new default reaches the unedited entry');
assert.deepEqual(upgraded.type_layouts[colorKey].color, [1, 2, 3, 200]);

// Revision fields stay with an edited entry, so revision upgrades still apply.
const [revisedKey] = Object.entries(shipped.type_layouts)
  .find(([, entry]) => entry.layout_revision && Object.keys(entry.children || {}).length) || [];
if (revisedKey) {
  const revised = clone(loaded);
  revised.type_layouts[revisedKey].aspect_ratio = 7.5;
  const entry = layoutChanges(shipped, revised).layout.type_layouts[revisedKey];
  assert.equal(entry.layout_revision, shipped.type_layouts[revisedKey].layout_revision);
  assert.equal(entry.children, undefined);
}

// Restoring an edit to its shipped value removes it from storage.
const restored = clone(edited);
restored.type_layouts[colorKey].color = clone(shipped.type_layouts[colorKey].color);
assert.equal(layoutChanges(shipped, restored).layout.type_layouts[colorKey], undefined);

// A full snapshot saved before this format still loads as before.
assert.equal(savedLayoutChanges(edited), edited);
assertSameLayout(mergeLayoutStores(shipped, edited), edited, 'older full snapshot');
assert.deepEqual(savedLayoutChanges(null), {});

console.log(`Layout changes: ${bytes} bytes stored instead of ${full}; round trip, later defaults and older snapshots pass`);
