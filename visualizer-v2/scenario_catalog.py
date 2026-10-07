"""Presentation for the shared test registry; saved scenario keys stay stable."""
import json
from pathlib import Path
import re

CATALOG = json.loads((Path(__file__).parent / 'static/scenarios.json').read_text())
PREFERRED_ALIASES = CATALOG['aliases']


def default_alias(name):
    stem = name[:-4] if name.endswith('Test') else name
    stem = re.sub(r'([A-Z]+)([A-Z][a-z])', r'\1-\2', stem.replace('_', '-'))
    return re.sub(r'([a-z0-9])([A-Z])', r'\1-\2', stem).lower()


def program_identity(name):
    match = re.fullmatch(r'(RV32ISingleCycleSystemTest|RV32IBaselinePipelineProgramTest|RV32IFiveStageCoreProgramTest)/program-(\d+)', name)
    if match:
        core = {'RV32ISingleCycleSystemTest': 'single',
                'RV32IBaselinePipelineProgramTest': 'baseline',
                'RV32IFiveStageCoreProgramTest': 'advanced'}[match[1]]
        return int(match[2]), core, 'not-taken' if core == 'advanced' else ''
    match = re.fullmatch(r'RV32IPredictionProgramTest/([^/]+)/program-(\d+)', name)
    return (int(match[2]), 'advanced', match[1]) if match else None


def aliases_for(name):
    identity = program_identity(name)
    if identity:
        number, core, policy = identity
        if name.startswith('RV32IPredictionProgramTest/'):
            return [f'rv32i-predict-{policy}-program{number}']
        prefix = {'single': 'rv32i-program', 'baseline': 'rv32i-baseline-program',
                  'advanced': 'rv32i-five-stage-program'}[core]
        return [f'{prefix}{number}']
    return PREFERRED_ALIASES.get(name, [default_alias(name)])


def scenario_option(descriptor):
    name = descriptor['name']
    identity = program_identity(name)
    if identity:
        number, core, policy = identity
        program = next(p for p in CATALOG['programs'] if p['id'] == f'program-{number:02d}')
        return dict(key=aliases_for(name)[0], className=name,
                    title=f'{number:02d} · {program["title"]}', group=program['group'],
                    workload=program['id'], core=core, policy=policy)
    component = CATALOG['components'][name]
    return dict(key=aliases_for(name)[0], className=name, **component)


def scenario_options(descriptors):
    return [scenario_option(d) for d in descriptors if d['visualizable']]


def registered_scenario(descriptors, value):
    key = value.strip().lower().replace('_', '-')
    for d in descriptors:
        if not d['visualizable'] or d['name'] in {'WireTemplateTest', 'RewireValidationTest'}:
            continue
        name = d['name']
        if key in {alias.lower().replace('_', '-')
                   for alias in (name, default_alias(name), *aliases_for(name))}:
            return name
    raise ValueError(f'Unknown visual scenario: {value}')
