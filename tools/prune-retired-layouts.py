"""List (or with --apply, remove) layout.json entries for parts that no longer exist.
An entry is retired when its type's base name appears nowhere in src/ or include/,
when it names the retired "container" relay technology, or (for root layouts)
when its scenario alias no longer belongs to any registered test."""
import json, re, sys, pathlib
root = pathlib.Path(sys.argv[1]); apply = '--apply' in sys.argv
sys.path.insert(0, str(root / 'visualizer-v2'))
import circuit_backend
from scenario_catalog import aliases_for, default_alias
source = '\n'.join(p.read_text(errors='ignore') for d in ('src', 'include') for p in (root / d).rglob('*') if p.suffix in ('.cpp', '.hpp', '.tpp'))
tokens = set(re.findall(r'[A-Za-z_][A-Za-z0-9_]*', source))
registry = (root / 'src/tests/TestRegistry.cpp').read_text()
names = [n for n in circuit_backend.get_registered_test_names() if f'"{n.split("/")[0]}"' in registry or '/' in n]
aliases = set()
for n in names:
    aliases |= {n, n.lower(), default_alias(n), *aliases_for(n)}
def retired_type(t):
    base = re.split(r'[<(]', t)[0]
    return base not in tokens or '<container,' in t
path = root / 'visualizer-v2/layout.json'
text = path.read_text(); d = json.loads(text)
gone = {'type': [k for k in d['type_layouts'] if retired_type(k)],
        'profile': [(fp, k) for fp, v in d['profile_layouts'].items() for k in v if retired_type(k)],
        'root': [k for k in d['root_layouts'] if k.split('@')[0] not in aliases]}
for kind, items in gone.items():
    print(kind, len(items))
    for i in sorted(items, key=str): print('   ', i)
if apply:
    for k in gone['type']: del d['type_layouts'][k]
    for fp, k in gone['profile']: del d['profile_layouts'][fp][k]
    for fp in [fp for fp, v in d['profile_layouts'].items() if not v]: del d['profile_layouts'][fp]
    for k in gone['root']: del d['root_layouts'][k]
    # Same formatting as LayoutManager.save: indent 4, lists on one line.
    pretty = json.dumps(d, indent=4)
    path.write_text(re.sub(r"\[(.*?)\]", lambda m: "[" + re.sub(r"\s+", "", m.group(1)).replace(",", ", ") + "]", pretty, flags=re.DOTALL))
    print('written', path.stat().st_size, 'bytes')
