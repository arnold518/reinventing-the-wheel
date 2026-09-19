import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {upgradeDefaultLayout} from '../visualizer-v2/static/layout_migration.mjs';
const layout=JSON.parse(readFileSync(new URL('../visualizer-v2/layout.json',import.meta.url)));
let count=0;
for(const store of [layout.type_layouts,layout.root_layouts,...Object.values(layout.profile_layouts)]) {
  for(const entry of Object.values(store)) {
    if(!entry.automatic_layout_revision) continue;
    for(const snapshot of entry.automatic_layout_history) {
      const saved={children:structuredClone(snapshot.children),color:[1,2,3,190],aspect_ratio:9.876};
      const migrated=upgradeDefaultLayout(entry,saved);
      for(const [name,old] of Object.entries(snapshot.children)) {
        const next=entry.children[name];
        if(next) {
          assert.deepEqual(migrated.children[name].rel_pos,next.rel_pos);
          assert.equal(migrated.children[name].rel_width,next.rel_width);
        }
      }
      assert.equal(migrated.aspect_ratio,9.876);
      assert.deepEqual(migrated.color,saved.color);
      assert.deepEqual(upgradeDefaultLayout(entry,migrated),migrated);
      const name=Object.keys(saved.children)[0];
      if(name) {
        saved.children[name].rel_pos=[.123456,.654321];
        assert.deepEqual(upgradeDefaultLayout(entry,saved).children[name],saved.children[name]);
        saved.children[name].rel_width=.123456;
        assert.deepEqual(upgradeDefaultLayout(entry,saved).children[name],saved.children[name]);
      }
      saved.layout_mode='manual';
      assert.equal(upgradeDefaultLayout(entry,saved),saved);
    }
    count++;
  }
}
assert(count>100,`Expected broad refresh, only ${count} entries`);
// Skipping an intermediate release must still migrate old defaults.
const latest={automatic_layout_revision:'v3',children:{A:{rel_pos:[.4,.4],rel_width:.2}},
  automatic_layout_history:[{revision:'v1',children:{A:{rel_pos:[.1,.1],rel_width:.2}}},
    {revision:'v2',children:{A:{rel_pos:[.2,.2],rel_width:.2}}}]};
assert.deepEqual(upgradeDefaultLayout(latest,{automatic_layout_revision:'v1',children:latest.automatic_layout_history[0].children}).children,latest.children);
console.log(`${count} automatic layout migrations passed; styles, manual positions and skipped revisions preserved`);
