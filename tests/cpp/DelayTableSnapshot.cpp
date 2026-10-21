#include "components/selection/ProvenPartDelays.hpp"
#include <iostream>
#include <iomanip>
// Complete deterministic value snapshot for build-partition/native/WASM
// comparisons. This does not recompute the tables from circuit gates.
namespace {
void text(const std::string& value){std::cout<<value.size()<<':'<<value<<'\n';}
void arc(const circuit::PartDelays::Arc& a){text(a.input);text(a.output);std::cout<<a.delay.minimum<<' '<<a.delay.maximum<<'\n';}
void table(const std::map<std::string,circuit::PartDelays>& values){
 std::cout<<values.size()<<'\n';
 for(const auto&[name,t]:values){text(name);text(t.evidence);std::cout<<t.arcs.size()<<'\n';for(const auto&a:t.arcs)arc(a);
  text(t.control);std::cout<<t.arcs_by_control.size()<<'\n';for(const auto&[value,arcs]:t.arcs_by_control){std::cout<<value<<' '<<arcs.size()<<'\n';for(const auto&a:arcs)arc(a);}
  text(t.clock);std::cout<<t.clock_outputs.size()<<'\n';for(const auto& value:t.clock_outputs)text(value);
  std::cout<<t.launches.size()<<'\n';for(const auto&[name,range]:t.launches){text(name);std::cout<<range.minimum<<' '<<range.maximum<<'\n';}
  std::cout<<t.setup.size()<<'\n';for(const auto&[name,value]:t.setup){text(name);std::cout<<value<<'\n';}
  std::cout<<t.self_timed<<' '<<t.clocked_interface<<' '<<t.internal_period<<' '<<bool(t.twin)<<'\n';
  std::cout<<t.fixed_inputs.size()<<'\n';for(const auto&[name,value]:t.fixed_inputs){text(name);std::cout<<value<<'\n';}
 }
}
}
int main(){text("unit");table(circuit::provenPartDelaysUnit());text("embedded");table(circuit::provenPartDelaysEmbedded());text("desktop");table(circuit::provenPartDelaysDesktop());return 0;}
