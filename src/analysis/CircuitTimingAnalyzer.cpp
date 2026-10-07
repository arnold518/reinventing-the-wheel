#include "analysis/CircuitTimingAnalyzer.hpp"
#include "basic/PinBase.hpp"
#include "basic/WireBase.hpp"
#include "components/BasicComponent.hpp"
#include "components/IOComponent.hpp"
#include "modules/utility/Rewire.hpp"
#include <algorithm>
#include <deque>
#include <set>
#include <unordered_map>

namespace circuit::analysis {
namespace {
bool control(const std::string& n) {
    return n=="CLK" || n=="RST" || n=="MEM_CLK" || n=="MEM_RST";
}
std::optional<ComponentTimingModel> builtin(const IOComponent& c) {
    ComponentTimingModel m;
    const std::string id=c.getContractId(), type=c.getTypeName();
    const bool storage=type=="DFlipFlop" || id=="sequential.d-flip-flop"
        || id=="memory.write-enabled-bit" || id.starts_with("memory.register.")
        || id.starts_with("memory.ff-word.") || id.starts_with("memory.ff-storage-bank.")
        || id.starts_with("rv32i.pipeline-register.");
    const bool rf=id=="rv32i.register-file";
    const bool ff=id.starts_with("memory.container.ff.") || id.starts_with("memory.device.ff.");
    const bool endpoint=id.starts_with("memory.scalable.ff.") || id.starts_with("memory.endpoint.ff.");
    const bool predictor=id=="rv32i.prediction.predictor";
    if(!storage && !rf && !ff && !endpoint && !predictor) return std::nullopt;
    for(const auto& [n,p]:c.getAllInputPins()) if(!control(n)) m.capture_inputs.push_back(n);
    for(const auto& [n,p]:c.getAllOutputPins()) m.launch_outputs.push_back(n);
    if(rf) m.combinational_arcs={{"RS1_ADDR","RS1_DATA"},{"RS2_ADDR","RS2_DATA"}};
    if(ff) for(auto in:{"WORD_ADDR","READ_EN"}) for(auto out:{"READ_DATA","FAULT"})
        if(c.getAllInputPins().contains(in)&&c.getAllOutputPins().contains(out)) m.combinational_arcs.emplace_back(in,out);
    if(endpoint) {
        m.combinational_arcs.emplace_back("RSP_READY","REQ_READY");
        for(auto in:{"RSP_READY","REQ_VALID"}) m.combinational_arcs.emplace_back(in,"RSP_VALID");
        for(auto in:{"REQ_OP","REQ_ADDR","REQ_WRITE_MASK"}) {
            m.combinational_arcs.emplace_back(in,"RSP_STATUS");
            m.combinational_arcs.emplace_back(in,"RSP_READ_DATA");
        }
        for(auto in:{"RSP_READY","REQ_VALID"}) m.combinational_arcs.emplace_back(in,"RSP_READ_DATA");
    }
    if(predictor) {
        m.combinational_arcs.emplace_back("PC","COUNTER");
        for(auto in:{"PC","DIRECT_JUMP","CONDITIONAL","BACKWARD"}) {
            m.combinational_arcs.emplace_back(in,"TAKEN");
            m.combinational_arcs.emplace_back(in,"NEXT_PC");
        }
        for(auto in:{"TARGET","PC_PLUS_4"}) m.combinational_arcs.emplace_back(in,"NEXT_PC");
    }
    return m;
}
struct Node {
    std::shared_ptr<IOComponent> owner;
    std::shared_ptr<PinBase> pin;
    size_t launch=0, capture=0;
};
struct Edge {size_t to,delay;};
}

CircuitTimingReport CircuitTimingAnalyzer::analyze(const std::shared_ptr<Component>& root,
                                                  const CircuitTimingOptions& options) {
    CircuitTimingReport report;
    report.limitation="Normalized pin-arc delays, not physical frequency. No routing, skew or transistor model. "
        "Clock/reset distribution excluded. Compact storage has explicit asynchronous arcs and one-tick launch/capture allowances. "
        "Unmodeled clocked black boxes invalidate the report.";
    if(!root) {report.valid=false;return report;}
    std::vector<Node> nodes;
    std::vector<std::vector<Edge>> edges;
    std::unordered_map<std::shared_ptr<PinBase>,size_t> indices;
    std::vector<std::shared_ptr<IOComponent>> leaves;
    std::function<void(const std::shared_ptr<Component>&)> collect=[&](const auto& c) {
        if(std::string(c->getTypeName())=="DFlipFlop") {
            leaves.push_back(std::dynamic_pointer_cast<IOComponent>(c));return;
        }
        if(auto basic=std::dynamic_pointer_cast<BasicComponent>(c)) {
            if(std::string(c->getTypeName())!="ClockGenerator") leaves.push_back(basic);
        } else for(const auto& child:c->getChildren()) collect(child);
    };
    collect(root);
    for(const auto& c:leaves) {
        for(const auto* pins:{&c->getAllInputPins(),&c->getAllOutputPins()})
            for(const auto& [name,pin]:*pins) if(!control(name)) {
                indices.emplace(pin,nodes.size());nodes.push_back({c,pin});edges.emplace_back();
            }
    }
    auto connect=[&](size_t from,size_t to,size_t delay){edges[from].push_back({to,delay});++report.connection_count;};
    for(const auto& c:leaves) {
        auto m=options.model?options.model(*c):std::nullopt;
        if(!m) m=builtin(*c);
        const auto basic=std::dynamic_pointer_cast<BasicComponent>(c);
        const auto delay=basic?basic->getDelay():1;
        auto arc=[&](const std::string& in,const std::string& out) {
            const auto a=c->getAllInputPins().find(in),b=c->getAllOutputPins().find(out);
            if(a==c->getAllInputPins().end()||b==c->getAllOutputPins().end()
                ||!indices.contains(a->second)||!indices.contains(b->second)) {
                report.valid=false;report.diagnostics.push_back(c->getID()+": invalid timing arc "+in+" -> "+out);return;
            }
            connect(indices.at(a->second),indices.at(b->second),delay);
        };
        if(m) {
            if(!m->capture_inputs.empty()||!m->launch_outputs.empty())++report.sequential_boundary_count;
            else ++report.combinational_component_count;
            for(const auto& name:m->capture_inputs) {
                const auto it=c->getAllInputPins().find(name);
                if(it==c->getAllInputPins().end()||!indices.contains(it->second)) {
                    report.valid=false;report.diagnostics.push_back(c->getID()+": invalid capture "+name);continue;
                }
                nodes[indices.at(it->second)].capture=m->capture_allowance;
            }
            for(const auto& name:m->launch_outputs) {
                const auto it=c->getAllOutputPins().find(name);
                if(it==c->getAllOutputPins().end()||!indices.contains(it->second)) {
                    report.valid=false;report.diagnostics.push_back(c->getID()+": invalid launch "+name);continue;
                }
                nodes[indices.at(it->second)].launch=m->clock_to_output;
            }
            for(const auto& [in,out]:m->combinational_arcs) arc(in,out);
        } else if(c->getAllInputPins().contains("CLK")) {
            report.valid=false;report.diagnostics.push_back(c->getID()+": missing clocked timing model ("+c->getTypeName()+")");
        } else {
            ++report.combinational_component_count;
            if(auto rewire=std::dynamic_pointer_cast<Rewire>(c)) {
                std::set<std::pair<std::string,std::string>> mappings;
                for(const auto& bit:rewire->getBitMappings()) mappings.emplace(bit.src_wire,bit.dst_wire);
                for(const auto& [in,out]:mappings) if(!control(in)&&!control(out)) arc(in,out);
            } else for(const auto& [in,p]:c->getAllInputPins()) if(!control(in))
                for(const auto& [out,q]:c->getAllOutputPins()) if(!control(out)) arc(in,out);
            for(const auto& [out,p]:c->getAllOutputPins()) if(indices.contains(p))
                nodes[indices.at(p)].launch=delay;
        }
    }
    // Trace hierarchy boundaries without modifying the circuit.
    for(size_t source=0;source<nodes.size();++source) {
        if(nodes[source].pin->getType()!=PinType::OUTPUT) continue;
        std::set<std::shared_ptr<WireBase>> visited;
        std::function<void(const std::shared_ptr<WireBase>&)> trace=[&](const auto& wire) {
            if(!wire||!visited.insert(wire).second) return;
            for(const auto& weak:wire->getSinkPinsBase()) if(auto sink=weak.lock()) {
                if(auto it=indices.find(sink);it!=indices.end()) {connect(source,it->second,0);continue;}
                trace(sink->getType()==PinType::INPUT?sink->getInternalWireBase():sink->getExternalWireBase());
            }
        };
        trace(nodes[source].pin->getExternalWireBase());
        // Same-owner feedback is stored as an internal wire by the builder.
        // Do not follow a composite register's incoming internal Q wire.
        if(std::dynamic_pointer_cast<BasicComponent>(nodes[source].owner))
            trace(nodes[source].pin->getInternalWireBase());
    }
    // Linear-time DAG longest path, including disconnected/self-loop detection.
    // A launch never inherits the arrival time at the preceding capture.
    std::vector<size_t> incoming(nodes.size()),arrival(nodes.size()),previous(nodes.size(),nodes.size());
    std::deque<size_t> ready;
    for(const auto& list:edges) for(const auto& e:list) ++incoming[e.to];
    for(size_t i=0;i<nodes.size();++i) {arrival[i]=nodes[i].launch;if(!incoming[i])ready.push_back(i);}
    size_t processed=0,endpoint=0;
    while(!ready.empty()) {
        const auto from=ready.front();ready.pop_front();++processed;
        if(arrival[from]+nodes[from].capture>report.critical_path_delay) {
            report.critical_path_delay=arrival[from]+nodes[from].capture;endpoint=from;
        }
        for(const auto& e:edges[from]) {
            if(arrival[from]+e.delay>arrival[e.to]) {arrival[e.to]=arrival[from]+e.delay;previous[e.to]=from;}
            if(--incoming[e.to]==0)ready.push_back(e.to);
        }
    }
    if(processed!=nodes.size()) {
        report.valid=false;report.combinational_cycle_detected=true;
        std::vector<unsigned> color(nodes.size());
        std::vector<size_t> stack;
        std::function<bool(size_t)> findCycle=[&](size_t n) {
            color[n]=1;stack.push_back(n);
            for(const auto& e:edges[n]) if(incoming[e.to]) {
                if(color[e.to]==1) {
                    for(auto it=std::find(stack.begin(),stack.end(),e.to);it!=stack.end();++it)
                        report.diagnostics.push_back("cycle: "+nodes[*it].owner->getID()+"."+nodes[*it].pin->getName());
                    return true;
                }
                if(!color[e.to]&&findCycle(e.to))return true;
            }
            stack.pop_back();color[n]=2;return false;
        };
        for(size_t i=0;i<nodes.size();++i) if(incoming[i]&&!color[i]&&findCycle(i))break;
    }
    if(!report.valid) {report.critical_path_delay=0;return report;}
    for(auto i=endpoint;i<nodes.size();i=previous[i]) {
        const auto& n=nodes[i];
        report.critical_path.push_back({n.owner->getID()+"."+n.pin->getName(),n.owner->getTypeName(),
            previous[i]<nodes.size()?arrival[i]-arrival[previous[i]]:arrival[i],arrival[i]+n.capture,n.capture>0});
    }
    std::reverse(report.critical_path.begin(),report.critical_path.end());
    return report;
}
}
