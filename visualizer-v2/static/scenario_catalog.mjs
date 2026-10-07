// Shared presentation rules for the WASM worker and the browser selector.
export function defaultAlias(name) {
  return (name.endsWith('Test') ? name.slice(0, -4) : name).replaceAll('_', '-')
    .replace(/([A-Z]+)([A-Z][a-z])/g, '$1-$2').replace(/([a-z0-9])([A-Z])/g, '$1-$2').toLowerCase();
}

export function programIdentity(name) {
  const match = name.match(/^(RV32ISingleCycleSystemTest|RV32IBaselinePipelineProgramTest|RV32IFiveStageCoreProgramTest)\/program-(\d+)$/);
  if (match) {
    const core = {RV32ISingleCycleSystemTest: 'single', RV32IBaselinePipelineProgramTest: 'baseline',
      RV32IFiveStageCoreProgramTest: 'advanced'}[match[1]];
    return {number: Number(match[2]), core, policy: core === 'advanced' ? 'not-taken' : ''};
  }
  const prediction = name.match(/^RV32IPredictionProgramTest\/([^/]+)\/program-(\d+)$/);
  return prediction ? {number: Number(prediction[2]), core: 'advanced', policy: prediction[1]} : null;
}

export function aliasesFor(name, catalog) {
  const identity = programIdentity(name);
  if (identity) {
    if (name.startsWith('RV32IPredictionProgramTest/')) return [`rv32i-predict-${identity.policy}-program${identity.number}`];
    const prefix = {single: 'rv32i-program', baseline: 'rv32i-baseline-program', advanced: 'rv32i-five-stage-program'}[identity.core];
    return [`${prefix}${identity.number}`];
  }
  return catalog.aliases[name] || [defaultAlias(name)];
}

export function scenarioOptions(descriptors, catalog) {
  if (catalog.schema !== 1) throw new Error('Unsupported scenario catalog');
  return descriptors.filter(d => d.visualizable).map(d => {
    const name = d.name, identity = programIdentity(name);
    const base = {key: aliasesFor(name, catalog)[0], className: name};
    if (identity) {
      const workload = `program-${String(identity.number).padStart(2, '0')}`;
      const p = catalog.programs.find(p => p.id === workload);
      if (!p) throw new Error(`Program missing from scenario catalog: ${name}`);
      return {...base, title: `${String(identity.number).padStart(2, '0')} · ${p.title}`, group: p.group,
        workload, core: identity.core, policy: identity.policy};
    }
    const component = catalog.components[name];
    if (!component) throw new Error(`Component missing from scenario catalog: ${name}`);
    return {...base, ...component};
  });
}

// A fixed program occupies one menu row. Core and prediction choose the
// existing registered scenario; aliases/layout/profile identities do not move.
export function menuOptions(options, groups) {
  const visible = options.filter(o => !o.workload || o.core === 'single');
  return groups.map(group => ({...group, options: visible.filter(o => o.group === group.id)
    .sort((a, b) => a.title.localeCompare(b.title, 'en', {numeric: true}))})).filter(g => g.options.length);
}

export function findScenario(options, value, catalog) {
  const normalized = value.trim().toLowerCase().replaceAll('_', '-');
  return options.find(o => new Set([o.key, defaultAlias(o.className), o.className.toLowerCase(),
    ...aliasesFor(o.className, catalog)].map(a => a.toLowerCase().replaceAll('_', '-'))).has(normalized));
}

export function resolveProgram(options, workload, core, policy) {
  // Prefer the original advanced-program key for the default policy. Explicit
  // prediction aliases remain loadable and continue using their saved profiles.
  return options.find(o => o.workload === workload && o.core === core &&
    (core !== 'advanced' || o.policy === policy));
}
