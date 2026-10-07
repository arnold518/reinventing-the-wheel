import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {upgradeDefaultLayout} from '../../visualizer-v2/static/layout_migration.mjs';

const layout = JSON.parse(readFileSync(new URL('../../visualizer-v2/layout.json', import.meta.url)));
let count = 0;
for (const store of [layout.type_layouts, layout.root_layouts, ...Object.values(layout.profile_layouts)]) {
  for (const entry of Object.values(store)) {
    if (!['rv32i-datapath-v1','rv32i-stages-v2','rv32i-single-cycle-auto-v3','rv32i-single-cycle-stage-order-v1','rv32i-blocks-v4','rv32i-alu-auto-v5','rv32i-alu-auto-v6','rv32i-routing-v1'].includes(entry.layout_revision)) continue;
    const old = structuredClone(entry.previous_default);
    const migrated = upgradeDefaultLayout(entry, old);
    for (const key of ['aspect_ratio', 'min_aspect_ratio', 'pin_size_ratio', 'title_bar_ratio'])
      if (entry[key] !== undefined) assert.equal(migrated[key], entry[key]);
    for (const [name, next] of Object.entries(entry.children || {})) {
      if (old.children?.[name]) {
        assert.deepEqual(migrated.children[name].rel_pos, next.rel_pos);
        assert.equal(migrated.children[name].rel_width, next.rel_width);
      }
    }
    assert.deepEqual(upgradeDefaultLayout(entry, migrated), migrated);
    const custom = structuredClone(old);
    custom.color = [1,2,3,190];
    custom.aspect_ratio = 8.765;
    const name = Object.keys(custom.children || {})[0];
    if (name) custom.children[name].rel_pos = [.123,.234];
    const kept = upgradeDefaultLayout(entry, custom);
    assert.deepEqual(kept.color, custom.color);
    assert.equal(kept.aspect_ratio, custom.aspect_ratio);
    if (name) assert.deepEqual(kept.children[name], custom.children[name]);
    count++;
  }
}
assert(count >= 44+20);
const unrelated = {children: {A:{rel_pos:[.1,.2],rel_width:.3}}};
assert.equal(upgradeDefaultLayout({},unrelated),unrelated);
assert.equal(upgradeDefaultLayout(layout.type_layouts.RV32IDecodeStage,undefined),undefined);
console.log(`${count} RV32I layout upgrades passed; user edits preserved`);
