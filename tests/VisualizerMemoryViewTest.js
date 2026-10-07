const assert = require("node:assert/strict");
const view = require("../visualizer-v2/static/memory_view.js");
for (const operation of ["READ", "WRITE", "PROGRAM", "ERASE"]) {
  const result = view.transactionStyle({transaction:{operation,phase:"response-held",status:"OK"}});
  assert.equal(result.mode, `${operation} HELD`);
  assert.equal(result.color, operation === "READ" ? "rgb(89, 214, 141)" : "rgb(255, 184, 77)");
}
assert.equal(view.transactionStyle({transaction:{operation:"READ",phase:"response",status:"RANGE"}}).fault,true);
assert.equal(view.transactionStyle({}), null);
assert.equal(view.parseAddress("0x640", 2048),1600);
for(const bad of ["", "-1", "0x", "2e3", "2048", "4294967296"]) assert.throws(()=>view.parseAddress(bad,2048));
const state={touchedWords:[{address:0},{address:4}],windowWords:[{address:4},{address:1600}]};
assert.deepEqual(view.rows(state).map(r=>r.address),[0,4,1600]);
assert.deepEqual(view.rows({...state,manualPage:true}).map(r=>r.address),[4,1600]);
console.log("Memory UI: operation colors, held responses, errors, address validation and page isolation passed");
