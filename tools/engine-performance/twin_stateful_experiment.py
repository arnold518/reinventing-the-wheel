#!/usr/bin/env python3
"""Guarded exact-repeat caching for successful GateTwin timing answers."""
import argparse
from pathlib import Path
import experiment as e
import wire_timing_experiment as w
import twin_memo_experiment as m


def wrapper():
    source=m.WRAPPER
    source=w.replace(source,'    bool enabled_;\n','')
    source=w.replace(source,'        bool valid=false,answered=false;',
                     '        std::unique_ptr<State> inner;\n        bool valid=false,answered=false;')
    source=w.replace(source,':inner_(std::move(inner)),enabled_(inner_->flops.empty()&&inner_->clock_input<0){',
                     ':inner_(std::move(inner)){')
    source=w.replace(source,'''        if(!enabled_)return inner_->start();
        return std::make_unique<MemoState>();''','''        auto memo=std::make_unique<MemoState>();memo->inner=inner_->start();return memo;''')
    source=w.replace(source,'''        if(!enabled_){MEMO_COUNT(stateful);return inner_->settle(state,now,values,changed,settled,result);}
''','')
    source=w.replace(source,'''        const bool answered=inner_->settle(nullptr,now,values,changed,settled,result);''','''        const auto* memory=static_cast<const GateTwin::Memory*>(memo.inner.get());
        const uint8_t clock=inner_->clock_input>=0 ?
            uint8_t(values[inner_->clock_input][0]==LogicValue::LOW?0:
                    values[inner_->clock_input][0]==LogicValue::HIGH?1:UNKNOWN) : UNKNOWN;
        // The original first falling-only call can return without filling its
        // output buffers. A second identical call can then compute an answer.
        bool falling_alone=memory&&memory->started&&memory->clock==1&&clock==0;
        if(falling_alone)for(size_t pin=0;falling_alone&&pin<changed.size();++pin)
            if(int(pin)!=inner_->clock_input)for(auto at:changed[pin])falling_alone=falling_alone&&at!=now;
        const bool answered=inner_->settle(memo.inner.get(),now,values,changed,settled,result);''')
    source=w.replace(source,'memo.outputs=result;memo.answered=answered;memo.valid=true;return answered;',
        'memo.outputs=result;memo.answered=answered;memo.valid=answered&&(!memory||clock<UNKNOWN)&&!falling_alone;'
        'if(!memo.valid)MEMO_COUNT(stateful);return answered;')
    source=source.replace('statefulBypass','noncacheable')
    return source


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,default=e.ROOT/'build-milestone/engine-performance-20261026/wire-timing')
    p.add_argument('--compiler',type=Path,default=Path('/home/arnold/.cache/circuitsim-emsdk/upstream/bin/clang++'))
    p.add_argument('stage',choices=['build','verify','diagnostics']);a=p.parse_args()
    out=a.output.resolve();prefix='memo-stateful'
    if a.stage=='build':m.build(out,a.compiler.absolute(),prefix,wrapper())
    elif a.stage=='verify':m.verify(out,a.compiler.absolute(),prefix)
    else:m.diagnostics(out,prefix)


if __name__=='__main__':main()
