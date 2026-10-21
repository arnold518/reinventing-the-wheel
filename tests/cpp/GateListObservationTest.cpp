#include "../../src/analysis/GateListModel.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/StructuralNetBuilder.hpp"
#include "../../src/modules/memory/FlipFlopCellBehavioral.hpp"
#include <array>
#include <iostream>
#include <stdexcept>

int main(){
    try{
        using L=LogicValue;
        auto root=Component::create<IOComponent>("ROOT",[](IOComponent* p){
            for(auto n:{"D","WE","RST","CLK"})p->addPin(n,PinType::INPUT);
            p->addPin("Q",PinType::OUTPUT);
        });
        ComponentBuilder b(root);circuit::detail::StructuralWiring w(b);
        auto bank=b.addNewComponent<IOComponent>("BANK",[](IOComponent* p){
            for(auto n:{"D","WE","RST","CLK"})p->addPin(n,PinType::INPUT);
            p->addPin("Q",PinType::OUTPUT);
        });
        ComponentBuilder inner(bank);circuit::detail::StructuralWiring inside(inner);
        auto cell=inner.addNewComponent<FlipFlopCellBehavioral>("CELL");
        for(auto n:{"D","WE","RST","CLK"}){
            w.connect(root->getInputPin(n),bank->getInputPin(n));
            inside.connect(bank->getInputPin(n),cell->getInputPin(n));
        }
        inside.connect(cell->getOutputPin("Q"),bank->getOutputPin("Q"));inside.finish();
        w.connect(bank->getOutputPin("Q"),root->getOutputPin("Q"));w.finish();
        std::string why;
        const auto program=circuit::analysis::GateListModel::compile(root,&why,
            {bank->getOutputPin("Q"),bank->getInputPin("D")});
        if(!program)throw std::runtime_error(why);
        const auto plain=circuit::analysis::GateListModel::compile(root,&why);
        if(!plain||circuit::analysis::GateListModel(plain).nodeCount()!=circuit::analysis::GateListModel(program).nodeCount())
            throw std::runtime_error("Observation changed the circuit graph");
        if(circuit::analysis::GateListModel::compile(root,&why,{nullptr}))
            throw std::runtime_error("Invalid observation accepted");
        const std::array values{L::LOW,L::HIGH,L::UNKNOWN,L::HIGH_Z};
        size_t cases=0;
        for(auto initial:values)for(auto reset:values)for(auto enable:values)for(auto data:values){
            circuit::analysis::GateListModel model(program);
            L expected=L::UNKNOWN,previous=L::UNKNOWN;
            auto drive=[&](L d,L we,L rst,L clk){
                std::vector<std::vector<L>> inputs;
                for(const auto&[name,width]:model.inputs())
                    inputs.push_back({name=="D"?d:name=="WE"?we:name=="RST"?rst:clk});
                model.step(inputs);
                // Independent bit truth rules, with D/WE/RST settled before
                // a sampling edge. X and Z both store an unknown bit.
                if(rst==L::HIGH)expected=L::LOW;
                else if(rst!=L::LOW){if(expected!=L::LOW)expected=L::UNKNOWN;}
                else if(previous==L::LOW&&clk==L::HIGH){
                    if(we==L::HIGH)expected=(d==L::LOW||d==L::HIGH)?d:L::UNKNOWN;
                    else if(we!=L::LOW&&!(expected==L::LOW&&d==L::LOW))expected=L::UNKNOWN;
                }
                previous=clk;
                if(model.output(0)!=std::vector<L>{expected}||model.observation(0)!=model.output(0)||
                    model.observation(1)!=std::vector<L>{d})throw std::runtime_error("Gate-list cell/observation mismatch");
            };
            drive(L::LOW,L::LOW,L::HIGH,L::LOW);
            drive(initial,L::HIGH,L::LOW,L::LOW);drive(initial,L::HIGH,L::LOW,L::HIGH);
            drive(data,enable,reset,L::LOW);drive(data,enable,reset,L::HIGH);
            drive(data,enable,reset,L::LOW);++cases;
        }
        std::cout<<"PASS "<<cases<<" four-state enabled-cell/observation cases; invalid observation rejected\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
