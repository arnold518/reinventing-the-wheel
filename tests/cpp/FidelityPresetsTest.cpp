#include "components/selection/FidelityPresets.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/Component.hpp"
#include "visualizer/VisualizerSession.hpp"
#include "simulator/Simulator.hpp"
#include "runtime/RecordedGuest.hpp"
#include <iostream>
#include <stdexcept>

int main(){
    try{
        const auto& catalog=circuit::builtinComponentCatalog();
        auto logic=circuit::fidelityPreset("logic",catalog);
        auto visual=circuit::fidelityPreset("visual",catalog);
        auto selected=[](const auto& p,const std::string& id,circuit::ParameterMap parameters={}){
            return p.decide("ROOT",0,id,parameters).fidelity;
        };
        const auto require=[](bool ok,const char* why){if(!ok)throw std::runtime_error(why);};
        using circuit::Fidelity;
        const auto unavailable=circuit::fidelityPresetRestrictions("system.computer");
        require(unavailable.size()==1 && unavailable.at("gate").find("main memory")!=std::string::npos,"system gate availability reason");
        require(circuit::fidelityPresetRestrictions("storage.register.32").empty(),"small register incorrectly restricted");
        bool rejected=false;
        try{circuit::visualizer::VisualizerSession blocked("ComputerSystemScenarioTest","computer-system",{},0,"gate");}
        catch(const std::invalid_argument& error){rejected=std::string(error.what()).find("main memory")!=std::string::npos;}
        require(rejected,"system gate must fail before allocating a structural core");
        rejected=false;
        try{circuit::runtime::RecordedGuestOptions options;options.fidelity_profile="gate";(void)circuit::runtime::runRecordedGuest({}, {},options);}
        catch(const std::invalid_argument& error){rejected=std::string(error.what()).find("main memory")!=std::string::npos;}
        require(rejected,"game gate must fail before loading an ELF or building the core");
        require(catalog.resolve({"memory.ff.device.32","M",{{"depth_words","128"}},{},{},{}},"M",0,logic).fidelity==Fidelity::Structural,"4096 cells must stay structural");
        require(catalog.resolve({"memory.ff.device.32","M",{{"depth_words","129"}},{},{},{}},"M",0,logic).fidelity==Fidelity::Behavioral,"larger storage must be compact");
        require(selected(logic,"storage.state-bit")==Fidelity::Behavioral,"logic state storage");
        require(selected(visual,"storage.state-bit")==Fidelity::Structural,"unrendered state bit must stay structural");
        for(auto id:{"rv32im.core.educational-five-stage","arithmetic.multiplier.32","bus.router.32"})
            require(selected(visual,id)==Fidelity::Structural,"unrendered logic became compact");
        for(auto id:{"device.console","device.timer","device.buttons","device.framebuffer","device.fifo.8","rv32i.register-file","storage.register.32","memory.ff.device.32"})
            require(selected(visual,id)==Fidelity::Behavioral,"visual abstraction stayed structural");
        bool invalid=false;try{(void)circuit::fidelityPreset("unknown",catalog);}catch(const std::invalid_argument&){invalid=true;}
        require(invalid,"unknown preset accepted");
        for(auto name:{"logic","visual","compact"}){
            auto built=catalog.createRoot({"memory.ff.device.32","MEMORY",{{"depth_words","129"}},{},{},{}},circuit::fidelityPreset(name,catalog));
            circuit::validateFidelityPreset(built.root,name);
            require(built.root->getSelectedFidelity()=="behavioral","large memory selection mismatch");
        }
        invalid=false;try{(void)catalog.createRoot({"memory.ff.device.32","MEMORY",{{"depth_words","129"}},{},{},{}},circuit::fidelityPreset("gate",catalog));}
        catch(const std::runtime_error&){invalid=true;}
        require(invalid,"gate silently compacted an oversized memory");
        auto machine=catalog.createRoot({"system.computer","MACHINE",{{"machine","games-machine"},{"width","64"},{"height","48"},{"button_depth","7"}},{},{},{}},visual);
        circuit::validateFidelityPreset(machine.root,"visual");
        size_t count=0;
        const auto inspect=[&](const auto& self,const std::shared_ptr<Component>& part)->void{
            ++count;
            if(part->getID()=="MACHINE.CORE")require(part->getSelectedFidelity()=="structural","visual CPU was collapsed");
            if(part->getID()=="MACHINE.DISPLAY")require(part->getSelectedFidelity()=="behavioral","visual display was not abstracted");
            for(const auto& child:part->getChildren())self(self,child);
        };
        inspect(inspect,machine.root);
        Simulator sim;sim.setHistoryRecordingEnabled(false);
        circuit::visualizer::VisualizerSession view(machine.root,sim,"visual-budget");
        const auto topology=view.topologyResponseJson();size_t scopes=0,largest_scope=0,scope_count=0;
        view.forEachTopologySlice([&](const std::string& slice){scopes+=slice.size();largest_scope=std::max(largest_scope,slice.size());++scope_count;});
        const auto checkpoint=view.currentCheckpointJson(0);
        std::cout<<"Visual game topology: "<<count<<" components; "<<topology.size()
            <<" header bytes; "<<scopes<<" scope bytes in "<<scope_count<<" files (largest "<<largest_scope
            <<"); "<<checkpoint.size()<<" checkpoint bytes\n";
        require(count<=65536,"Visual game exceeds the external-topology component limit");
        require(topology.size()+4*1024*1024<32*1024*1024 && largest_scope<32*1024*1024,"Visual game topology file exceeds byte budget");
        require(16*checkpoint.size()+8*1024*1024<32*1024*1024,"Visual game exceeds the bounded recording part budget");
        std::cout<<"Fidelity presets, storage threshold, visual boundaries and strict failures pass\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
