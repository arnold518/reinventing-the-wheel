#include "components/selection/FidelityPresets.hpp"
#include "components/selection/ComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "components/Component.hpp"
#include "components/capabilities/MemoryStateView.hpp"
#include <algorithm>
#include <charconv>
#include <stdexcept>

namespace circuit {
std::vector<std::string> fidelityPresetNames(){return {"gate","logic","visual","compact"};}
std::map<std::string,std::string> fidelityPresetRestrictions(std::string_view contract){
    if(contract=="system.computer")return {{"gate",
        "Gate is unavailable for a computer system: its main memory exceeds the structural storage limit and requires behavioral storage. Choose Logic or Visual."}};
    if(contract.starts_with("memory.") && contract.find(".scalable-endpoint.")!=contract.npos)
        return {{"gate","Gate is unavailable for a scalable memory endpoint: it requires a behavioral native storage device. Choose Logic or Visual."}};
    return {};
}
std::string visualAbstractionKind(std::string_view id){
    if(id.starts_with("storage.register-file.") || id=="rv32i.register-file")return "register-file";
    if(id.starts_with("storage.register."))return "register";
    if(id=="memory.ff.cell" || id=="memory.sram.cell" || id=="memory.rom.cell")return "memory-cell";
    if(id.starts_with("memory.") && (id.find(".device.")!=id.npos || id.find(".endpoint.")!=id.npos || id.find(".scalable-endpoint.")!=id.npos))return "memory";
    if(id=="device.framebuffer")return "display";
    if(id=="device.buttons")return "keyboard";
    if(id=="device.console")return "console";
    if(id=="device.timer")return "timer";
    if(id=="device.leds")return "leds";
    if(id.starts_with("device.fifo."))return "queue";
    return {};
}
namespace {
bool registerStorage(std::string_view id){
    return id.starts_with("storage.register.") || id.starts_with("storage.register-file.") ||
        id=="rv32i.register-file" || id=="storage.state-bit" || id=="memory.ff.cell";
}
}
BuildProfile fidelityPreset(std::string_view name,const ComponentCatalog& catalog){
    if(name=="scenario")throw std::invalid_argument("scenario uses the registered scenario's base profile");
    if(name!="gate" && name!="logic" && name!="visual" && name!="compact")throw std::invalid_argument("Unknown fidelity profile: "+std::string(name));
    BuildProfileBuilder builder("fidelity-"+std::string(name)+"-v1");
    builder.addRule(preferFidelity(name=="compact"?Fidelity::Behavioral:Fidelity::Structural,
        ProfileSelector::any(),"named fidelity preset "+std::string(name)));
    builder.unavailablePolicy(name=="compact" || name=="logic"?UnavailableFidelityPolicy::UseOnlyAvailableAndRecordException:UnavailableFidelityPolicy::Error);
    for(const auto& contract:catalog.contracts()){
        const auto& id=contract.id;
        if((name=="visual" && !visualAbstractionKind(id).empty()) || (name=="logic" && registerStorage(id)))
            builder.addRule(preferFidelity(Fidelity::Behavioral,ProfileSelector::contract(id),"named "+std::string(name)+" abstraction"));
        // Existing device factories advertise structural capacity limits.
        // Logic can use their only available large-storage implementation;
        // the actual-build audit below permits precisely that exception.
    }
    return builder.build();
}
void validateFidelityPreset(const std::shared_ptr<Component>& root,std::string_view name){
    if(!root)throw std::invalid_argument("Fidelity profile requires a constructed circuit");
    if(name=="scenario" || name.empty())return;
    if(name=="compact")return;
    if(name!="gate" && name!="logic" && name!="visual")throw std::invalid_argument("Unknown fidelity profile: "+std::string(name));
    const auto visit=[&](const auto& self,const std::shared_ptr<Component>& component)->void{
        const auto& metadata=component->getInstanceMetadata();
        if(metadata && !metadata->contract_id.empty()){
            bool behavioral=name=="visual"?!visualAbstractionKind(metadata->contract_id).empty():
                name=="logic" && (registerStorage(metadata->contract_id));
            if(name=="logic" && component->getContractId().starts_with("memory.") && metadata->fidelity==Fidelity::Behavioral && !behavioral){
                if(metadata->contract_id.find(".device.")!=std::string::npos)
                    if(auto view=std::dynamic_pointer_cast<MemoryStateView>(component))
                        behavioral=view->memoryStorage().capacityBytes()>512;
            }
            if((metadata->fidelity==Fidelity::Behavioral)!=behavioral)
                throw std::runtime_error("Fidelity profile "+std::string(name)+" cannot honor its rule at "+component->getID()+" ("+metadata->contract_id+")");
            if(behavioral)return; // private implementation models belong to the abstraction
        }
        for(const auto& child:component->getChildren())self(self,child);
    };
    visit(visit,root);
}
}
