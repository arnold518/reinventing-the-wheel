import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {upgradeSramLayout} from '../visualizer-v2/static/sram_layout_migration.mjs';
const layout = JSON.parse(readFileSync(new URL('../visualizer-v2/layout.json', import.meta.url)));
let checked = 0;
for (const store of [layout.type_layouts, layout.root_layouts, ...Object.values(layout.profile_layouts)]) {
  for (const entry of Object.values(store)) {
    if (entry.layout_revision !== 'sram-matrix-v1') continue;
    const old = structuredClone(entry.previous_default);
    const upgraded = upgradeSramLayout(entry, old);
    for (const key of ['aspect_ratio', 'min_aspect_ratio', 'pin_size_ratio', 'title_bar_ratio'])
      assert.equal(upgraded[key], entry[key]);
    for (const name of Object.keys(old.children || {}))
      if (entry.children?.[name]) assert.deepEqual(upgraded.children[name], entry.children[name]);
    assert.deepEqual(upgradeSramLayout(entry, upgraded), upgraded);
    const custom = structuredClone(old);
    custom.color = [1,2,3,190];
    custom.aspect_ratio = 1.234;
    const name = Object.keys(custom.children || {})[0];
    if (name) custom.children[name].rel_pos = [0.123, 0.234];
    const preserved = upgradeSramLayout(entry, custom);
    assert.deepEqual(preserved.color, custom.color);
    assert.equal(preserved.aspect_ratio, custom.aspect_ratio);
    if (name) assert.deepEqual(preserved.children[name], custom.children[name]);
    ++checked;
  }
}
assert(checked >= 10);
const unrelated = {aspect_ratio: 9, children:{A:{rel_pos:[0,0],rel_width:0.1}}};
assert.equal(upgradeSramLayout({}, unrelated), unrelated);
console.log(`${checked} SRAM layout migrations passed; custom and unrelated layouts preserved`);
