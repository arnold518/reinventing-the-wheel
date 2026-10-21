#!/usr/bin/env python3
"""Split unchanged generated delay-table entries into bounded compiler units.

The tracked part_delay_tables output remains authoritative. No C++ initializer
is interpreted or rewritten; whole contiguous entries move into chunks.
"""
import argparse, pathlib, re
ROOT=pathlib.Path(__file__).resolve().parents[2]
PROFILES={'Unit':'ProvenPartDelays.generated.inc','Embedded':'ProvenPartDelays.embedded.generated.inc','Desktop':'ProvenPartDelays.desktop.generated.inc'}
def entries(text):
    matches=list(re.finditer(r'^\{"([^"\n]+)", \[\] \{\n',text,re.M))
    if not matches:raise ValueError('No generated table entries')
    preamble=text[:matches[0].start()]
    if any(line and not line.startswith('//') for line in preamble.splitlines()):raise ValueError('Unexpected generated preamble')
    result=[];seen=set()
    for i,m in enumerate(matches):
        key=m.group(1);block=text[m.start():matches[i+1].start() if i+1<len(matches) else len(text)]
        if key in seen:raise ValueError('Duplicate generated table key: '+key)
        if not block.endswith('    return t;\n}()},\n'):raise ValueError('Unexpected generated entry boundary: '+key)
        seen.add(key);result.append((key,block))
    return result

def write(path,text):
    # Avoid rebuilding unchanged chunks when an unrelated table is regenerated.
    if not path.exists() or path.read_text()!=text:path.write_text(text)

def generate(source,output,chunks=8):
    if chunks<1 or chunks>64:raise ValueError('chunks must be 1..64')
    output.mkdir(parents=True,exist_ok=True);summary={}
    for profile,filename in PROFILES.items():
        raw=(source/filename).read_text();items=entries(raw);total=sum(len(block) for _,block in items)
        groups=[[] for _ in range(chunks)];position=0
        for key,block in items:
            index=min(chunks-1,position*chunks//total);groups[index].append(block);position+=len(block)
        function='provenPartDelays'+profile
        declarations=[];calls=[]
        for index,blocks in enumerate(groups):
            name='append'+function[0].upper()+function[1:]+'Chunk'+str(index)
            declarations.append('void '+name+'(std::map<std::string, PartDelays>&);')
            calls.append('        detail::'+name+'(result);')
            body=''.join(blocks)
            text='// Generated compile partition; canonical entries are unchanged.\n#include "components/selection/ProvenPartDelays.hpp"\n#include <map>\nnamespace circuit::detail {\nvoid '+name+'(std::map<std::string, PartDelays>& result) {\n    using Arc = PartDelays::Arc;\n    std::map<std::string, PartDelays> chunk = {\n'+body+'    };\n    result.merge(chunk);\n}\n}\n'
            write(output/(function+'Chunk'+str(index)+'.cpp'),text)
        text='// Generated compile dispatcher; same public table function.\n#include "components/selection/ProvenPartDelays.hpp"\n#include <map>\nnamespace circuit {\nnamespace detail {\n'+'\n'.join(declarations)+'\n}\nconst std::map<std::string, PartDelays>& '+function+'() {\n    static const auto generated = [] {\n        std::map<std::string, PartDelays> result;\n'+'\n'.join(calls)+'\n        return result;\n    }();\n    return generated;\n}\n}\n'
        write(output/(function+'.cpp'),text)
        summary[profile]={'entries':len(items),'chunks':chunks,'largestInitializerBytes':max(map(lambda g:sum(map(len,g)),groups))}
    return summary

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source',type=pathlib.Path,default=ROOT/'src/components/selection')
    p.add_argument('--output',type=pathlib.Path,required=True)
    p.add_argument('--chunks',type=int,default=8)
    a=p.parse_args()
    for profile,summary in generate(a.source,a.output,a.chunks).items():print(profile,summary)
