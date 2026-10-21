import createCircuitSimModule from './circuitsim_wasm.js?v=10';
self.addEventListener('message', async ({data}) => {
  let bridge;
  try {
    const module = await createCircuitSimModule();
    bridge = new module.WasmVisualizer();
    const record = bridge.runRecordedGuestJson(String(data.guest), false, false, 500000);
    self.postMessage({record});
  } catch (error) { self.postMessage({error:String(error?.message || error)}); }
  finally { if(bridge) bridge.delete(); }
});
