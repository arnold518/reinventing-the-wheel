// Presentation and persistence keys only. C++ owns the selection rules.
// Every scenario opens in one of these: its catalog `fidelity`.
export const FIDELITY_PRESETS = Object.freeze([
  ['gate', 'Gate'], ['logic', 'Logic'], ['visual', 'Visual'], ['compact', 'Compact'],
]);
export function normalizeFidelityPreset(value) {
  if (!FIDELITY_PRESETS.some(([key]) => key === value)) throw new TypeError(`Unknown fidelity profile '${value}'`);
  return value;
}
export function profileStorageKey(scenario, preset) {
  return `${scenario}::${normalizeFidelityPreset(preset)}`;
}
