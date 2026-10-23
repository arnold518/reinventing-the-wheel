"""List (or with --apply, remove) layout.json entries for parts that no longer exist.
An entry is retired when its type's base name appears nowhere in src/ or include/,
when it names the retired "container" relay technology, or (for root layouts)
when its scenario alias no longer belongs to any registered test."""
import json, re, sys, pathlib
root = pathlib.Path(sys.argv[1]); apply = '--apply' in sys.argv
if str(root / 'visualizer-v2') not in sys.path:
    sys.path.append(str(root / 'visualizer-v2'))
import circuit_backend
from scenario_catalog import aliases_for, default_alias
source = '\n'.join(p.read_text(errors='ignore') for d in ('src', 'include') for p in (root / d).rglob('*') if p.suffix in ('.cpp', '.hpp', '.tpp'))
tokens = set(re.findall(r'[A-Za-z_][A-Za-z0-9_]*', source))
names = [entry['name'] for entry in circuit_backend.get_registered_test_descriptors()
         if entry['visualizable']]
aliases = set()
for n in names:
    aliases |= {n, n.lower(), default_alias(n), *aliases_for(n)}
# Layout keys the visualizer derives instead of using the type name directly
# (VisualizerSession.cpp and server.py: FFMemory is laid out as FFArray<width>).
DERIVED_KEY_PREFIXES = ('FFArray',)
def retired_type(t):
    if t.startswith(DERIVED_KEY_PREFIXES): return False
    base = re.split(r'[<(\[]', t)[0]
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
    # Same formatting as LayoutManager.save: compact lists, preserve strings.
    pretty = json.dumps(d, indent=4)
    def compact_list(match):
        if match.group(1) is None:
            return match.group(0)
        return "[" + re.sub(r"\s+", "", match.group(1)).replace(",", ", ") + "]"
    path.write_text(re.sub(r'"(?:\\.|[^"\\])*"|\[(.*?)\]', compact_list, pretty, flags=re.DOTALL))
    print('written', path.stat().st_size, 'bytes')
