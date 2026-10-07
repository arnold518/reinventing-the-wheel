"use strict";
(function (root, factory) {
  const api = factory();
  if (typeof module === "object" && module.exports) module.exports = api;
  else root.CircuitMemoryView = api;
}(typeof globalThis !== "undefined" ? globalThis : this, () => {
  function transactionStyle(state) {
    const tx = state.transaction;
    if (!tx) return null;
    const error = tx.status && tx.status !== "OK" && tx.status !== "UNKNOWN";
    const names = {reset: "RESET", idle: "IDLE", offered: "READY", "request-wait": "WAIT",
      pending: "PENDING", response: "RESPONSE", "response-held": "HELD"};
    const mode = [tx.operation, names[tx.phase] || "UNKNOWN"].filter(Boolean).join(" ");
    const color = error ? "rgb(211, 47, 47)"
      : ["WRITE", "PROGRAM", "ERASE"].includes(tx.operation) ? "rgb(255, 184, 77)"
      : tx.operation === "READ" ? "rgb(89, 214, 141)" : "rgba(238, 244, 255, 0.65)";
    return {mode: error ? `ERROR ${tx.status} ${mode}` : mode, color, fault: Boolean(error)};
  }
  function rows(state) {
    const byAddress = new Map();
    if (!state.manualPage) for (const row of state.touchedWords || [])
      byAddress.set(row.address, {...row, source: "touched"});
    for (const row of state.windowWords || []) if (!byAddress.has(row.address))
      byAddress.set(row.address, {...row, source: "window"});
    return [...byAddress.values()].sort((a, b) => a.address - b.address);
  }
  function parseAddress(text, capacity) {
    const value = String(text).trim();
    if (!/^(?:0x[0-9a-f]+|[0-9]+)$/i.test(value)) throw new Error("Enter a byte address in decimal or 0x hexadecimal.");
    const address = Number(value);
    if (!Number.isSafeInteger(address) || address < 0 || address > 0xffffffff || address >= capacity)
      throw new Error("Address is outside this memory.");
    return address;
  }
  return {transactionStyle, rows, parseAddress};
}));
