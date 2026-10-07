const assert = require('node:assert/strict');
const { model } = require('../../visualizer-v2/static/memory_cell_view.js');
for (const type of ['FlipFlopCell', 'SRAMCell', 'DRAMCell', 'ROMCell', 'NORFlashCell']) {
  for (const value of ['0','1','X','Z']) {
    const view = model(type, 'behavioral', {Q:value,Q_BAR:'0',SAMPLE:'1',WL:'X',RESTORE:'0',PROGRAM:'1',ERASE:'Z'});
    assert.equal(view.fields[0].value, value);
    if (type === 'DRAMCell' || type === 'NORFlashCell')
      assert.equal(model(type,'structural',{Q:value}).fields[0].value, value);
    else assert.equal(model(type,'structural',{Q:value}), null);
    if(type === 'DRAMCell') assert.equal(view.fields[1].value, '1');
    if(type === 'SRAMCell') assert.equal(view.fields[1].value, '0');
  }
  assert.equal(model(type, 'behavioral', {}).fields[0].value, 'X');
}
assert.equal(model('Register<32>','behavioral',{Q:'1'}), null);
assert.equal(model('DRAMCell','behavioral',{Q:'1',SAMPLE:'X'}).fields[1].value, 'X');
assert.deepEqual(model('NORFlashCell','behavioral',{WL:'1',PROGRAM:'0',ERASE:'Z'}).controls,
  [{pin:'WL',value:'1'},{pin:'PROGRAM',value:'0'},{pin:'ERASE',value:'Z'}]);
console.log('Memory-cell recorded-pin displays passed (five cell types, four-state values, structural exclusion).');
