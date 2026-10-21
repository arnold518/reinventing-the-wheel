#!/usr/bin/env python3
"""Verify lossless deterministic build-only partitioning of canonical tables."""
import importlib.util,pathlib,tempfile
ROOT=pathlib.Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('delay_chunks',ROOT/'tools/build/split-delay-tables.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
source=ROOT/'src/components/selection'
with tempfile.TemporaryDirectory(prefix='circuitsim-delay-chunks-') as temporary:
    a=pathlib.Path(temporary)/'a';b=pathlib.Path(temporary)/'b'
    summaries=m.generate(source,a);m.generate(source,b)
    assert sorted(p.name for p in a.iterdir())==sorted(p.name for p in b.iterdir())
    for path in a.iterdir():assert path.read_bytes()==(b/path.name).read_bytes(),path.name
    for profile,name in m.PROFILES.items():
        expected=m.entries((source/name).read_text());actual=[]
        for index in range(8):
            text=(a/f'provenPartDelays{profile}Chunk{index}.cpp').read_text()
            body=text.split('    std::map<std::string, PartDelays> chunk = {\n',1)[1].rsplit('    };\n',1)[0]
            if body:actual.extend(m.entries(body))
        assert actual==expected,profile
        assert summaries[profile]['entries']==len(expected)
        print(f'PASS {profile}: {len(expected)} verbatim entries, deterministic chunks')
    for malformed in ['// empty\n','{"a", [] {\n    return t;\n}()},\n{"a", [] {\n    return t;\n}()},\n','{"a", [] {\nreturn t;\n}()},\n']:
        try:m.entries(malformed)
        except ValueError:pass
        else:raise AssertionError('Malformed/duplicate input accepted')
