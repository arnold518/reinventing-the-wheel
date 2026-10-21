#include "components/ComponentBuilder.tpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "components/capabilities/MemoryStateView.hpp"
#include "simulator/SimulationTest.hpp"
#include "simulator/Event.hpp"
#include <iostream>
#include <map>
#include <stdexcept>

int main(){
    try{
        for(unsigned mode=0;mode<3;++mode){
            std::cout<<"Provisioning profile "<<mode<<std::endl;
            auto builder=circuit::BuildProfileBuilder("memory-startup-"+std::to_string(mode));
            builder.addRule(circuit::preferFidelity(mode==1?circuit::Fidelity::Behavioral:circuit::Fidelity::Structural,
                circuit::ProfileSelector::any()));
            if(mode==2)builder.addRule(circuit::preferFidelity(circuit::Fidelity::Behavioral,
                circuit::ProfileSelector::contract("memory.ff.controller.32")));
            builder.unavailablePolicy(circuit::UnavailableFidelityPolicy::UseOnlyAvailableAndRecordException);
            auto root=std::dynamic_pointer_cast<IOComponent>(circuit::builtinComponentCatalog().createRoot(
                {"memory.ff.scalable-endpoint.32","MEM",{{"depth_words","4"},{"record_history","0"}}},builder.build()).root);
            auto view=std::dynamic_pointer_cast<MemoryStateView>(root);
            view->clearMemoryImage();view->initializeMemoryBytes(0,{0x42,0xa5,0x5a,0x81});
            const auto expected=view->memoryStorage().readBytes(0,16);
            Simulator sim;sim.setHistoryRecordingEnabled(false);ComponentBuilder wires(root);
            std::map<std::string,std::shared_ptr<WireBase>> inputs;
            for(const auto&[name,pin]:root->getAllInputPins())
                inputs[name]=wires.addNewWireDynamic(name,pin->getWidth(),nullptr,{pin});
            for(const auto&[name,pin]:root->getAllOutputPins())wires.addNewWireDynamic(name,pin->getWidth(),pin,{});
            auto drive=[&](SimulationTime time,const std::string& name,LogicValue value){
                sim.scheduleEvent(makeWireUpdateEvent(time,inputs.at(name),
                    std::vector<LogicValue>(inputs.at(name)->getWidth(),value)));
            };
            SimulationTest::scheduleInitialEventsForTree(root,sim);
            for(const auto&[name,pin]:inputs)drive(0,name,name=="RST"?LogicValue::HIGH:LogicValue::LOW);
            auto retained=[&]{if(view->memoryStorage().readBytes(0,16)!=expected)
                throw std::runtime_error("Provisioned FF bytes changed without a memory write");};
            sim.advanceAndRecord(100);retained();
            drive(101,"RST",LogicValue::LOW);sim.advanceAndRecord(200);retained();
            drive(201,"RST",LogicValue::HIGH);sim.advanceAndRecord(300);retained();
            drive(301,"RST",LogicValue::UNKNOWN);sim.advanceAndRecord(400);retained();
        }
        std::cout<<"PASS FF provisioning survives startup and interface reset in structural, fused and mixed-controller profiles\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
