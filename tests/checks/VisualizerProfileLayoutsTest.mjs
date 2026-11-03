import assert from 'node:assert/strict';
import {completeProfileLayouts} from '../../visualizer-v2/static/profile_layouts.mjs';
const node=(id,name,type,parentIndex,depth)=>({id,name,type,parentIndex,depth,aspectRatio:1,minAspectRatio:.35,profileFingerprint:'explicit'});
const payload={rootId:'root',layoutKey:'demo',components:[node('root','ROOT','System',null,0),
 node('s','STRUCTURAL','Group',0,1),node('a','A','Leaf',1,2),node('b','B','Leaf',1,2),
 node('c','COMPACT','Group',0,1),node('m','MODEL','Leaf',4,2)]};
const source={type_layouts:{Group:{color:[1,2,3,255],layout_mode:'manual',children:{A:{rel_pos:[.1,.25],rel_width:.2},B:{rel_pos:[.6,.25],rel_width:.2}}},
 Unrelated:{children:{KEEP:{rel_pos:[.13,.27],rel_width:.1}}}},profile_layouts:{previous:{Group:{children:{MODEL:{rel_pos:[.2,.3],rel_width:.3}}}}},
 root_layouts:{demo:{children:{STRUCTURAL:{rel_pos:[.05,.3],rel_width:.4},COMPACT:{rel_pos:[.55,.3],rel_width:.4}}}}};
function resolve(c,layout,key){return {...layout.type_layouts[c.type],...layout.type_layouts[c.layoutType],
 ...(c.depth===0?layout.root_layouts[key]:{})};}
const data=structuredClone(payload),layout=structuredClone(source);
assert.deepEqual(completeProfileLayouts(data,layout,resolve),['c']);
assert.deepEqual(layout.type_layouts.Group,source.type_layouts.Group,'Original manual family layout changed');
assert.deepEqual(layout.type_layouts.Unrelated,source.type_layouts.Unrelated,'Unrelated layout changed');
assert.equal(data.components[1].layoutType,undefined,'Compatible structural layout changed');
assert.match(data.components[4].layoutType,/^Group\[config-fidelity-/);
assert.deepEqual(resolve(data.components[4],layout,'demo').children.MODEL,source.profile_layouts.previous.Group.children.MODEL,'Compatible profile placement not inherited');
assert.deepEqual(completeProfileLayouts(data,layout,resolve),[],'Repair is not idempotent');
const missing=structuredClone(source);delete missing.profile_layouts.previous;
const other=structuredClone(payload);completeProfileLayouts(other,missing,resolve);
const placement=resolve(other.components[4],missing,'demo').children.MODEL;
assert(placement.rel_width>0&&placement.rel_pos[0]>=0&&placement.rel_pos[1]>=.15);
assert(placement.rel_pos[0]+placement.rel_width<=1&&placement.rel_pos[1]+placement.rel_width<=1,'Generated component outside parent');
assert.deepEqual(missing.type_layouts.Group,source.type_layouts.Group,'Fallback overwrote saved manual geometry');
assert.deepEqual(other.components.map(c=>c.type),payload.components.map(c=>c.type),'Hardware types changed during layout repair');
console.log('PASS new profile scopes, compatible placement reuse, geometry bounds, idempotence and saved-layout preservation');
