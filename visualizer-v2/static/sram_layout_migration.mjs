import { upgradeDefaultLayout } from "./layout_migration.mjs";

// Compatibility export for the SRAM-specific regression checks.
export function upgradeSramLayout(base, saved) {
  return base?.layout_revision === "sram-matrix-v1" ? upgradeDefaultLayout(base, saved) : saved;
}
