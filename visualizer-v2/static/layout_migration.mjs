// Upgrade only recognized, unchanged shipped geometry. Keep user styling,
// positions and proportions; never discard the entire browser layout store.
function upgradeLegacyDefaultLayout(base, saved) {
  if (!["sram-matrix-v1", "rv32i-datapath-v1", "rv32i-stages-v2", "rv32i-single-cycle-auto-v3", "rv32i-single-cycle-stage-order-v1", "rv32i-blocks-v4", "rv32i-alu-auto-v5", "rv32i-alu-auto-v6", "rv32i-routing-v1", "register-file-ff-v1", "rv32i-advanced-grouping-v2"].includes(base?.layout_revision) || !saved ||
      saved.layout_revision === base.layout_revision || !base.previous_default) return saved;
  const previous = base.previous_default;
  const result = {...saved, children: {...saved.children}};
  for (const key of ["aspect_ratio", "min_aspect_ratio", "aspect_ratio_floor",
                     "title_bar_ratio", "pin_size_ratio", "boundary_area_ratio"]) {
    if (saved[key] === undefined || saved[key] === previous[key]) {
      if (base[key] !== undefined) result[key] = base[key];
    }
  }
  for (const [name, oldPlacement] of Object.entries(previous.children || {})) {
    const current = saved.children?.[name], next = base.children?.[name];
    if (!current || !next) continue;
    if (JSON.stringify(current.rel_pos) === JSON.stringify(oldPlacement.rel_pos) &&
        current.rel_width === oldPlacement.rel_width) {
      result.children[name] = {...current, rel_pos: next.rel_pos, rel_width: next.rel_width};
    }
  }
  result.layout_revision = base.layout_revision;
  if (base.layout_mode && (saved.layout_mode === undefined || saved.layout_mode === previous.layout_mode))
    result.layout_mode = base.layout_mode;
  return result;
}

export function upgradeDefaultLayout(base, saved) {
  if (saved?.layout_mode === "manual") return saved;
  const result = upgradeLegacyDefaultLayout(base, saved);
  if (!result || !base?.automatic_layout_revision ||
      result.automatic_layout_revision === base.automatic_layout_revision) return result;
  const upgraded = {...result, children: {...result.children}};
  // Retain snapshots across automatic revisions, so an infrequently opened
  // browser need not have installed every intervening set of defaults.
  const history = base.automatic_layout_history || [];
  for (const [name, current] of Object.entries(result.children || {})) {
    const next = base.children?.[name];
    if (!next) continue;
    const unchanged = history.some(snapshot => {
      const old = snapshot.children?.[name];
      return old && JSON.stringify(current.rel_pos) === JSON.stringify(old.rel_pos)
        && current.rel_width === old.rel_width;
    });
    if (unchanged) upgraded.children[name] = {...current, rel_pos: next.rel_pos, rel_width: next.rel_width};
  }
  upgraded.automatic_layout_revision = base.automatic_layout_revision;
  upgraded.automatic_layout_history = history;
  return upgraded;
}
