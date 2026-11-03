#pragma once

// Private device hierarchy. Structural groups build ordinary gates/storage;
// compact groups compile that same control blueprint and retain observer pins.
#include "DeviceLogic.hpp"
#include "components/capabilities/ComponentLayoutView.hpp"
#include "modules/basic/WordMux.hpp"
#include "modules/basic/WordGate.hpp"
#include "modules/utility/Rewire.hpp"
#include "components/GateListPartBehavioral.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "components/selection/BuildManifest.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/capabilities/BehavioralComposition.hpp"

namespace circuit::device::detail {
using Fields = std::vector<std::pair<std::string, size_t>>;
using GroupBody = std::function<void(IOComponent&, ComponentBuilder&)>;

class DeviceGroup : public IOComponent {
    std::string type_;
    GroupBody body_;
public:
    DeviceGroup(std::string name, std::string type, Fields inputs, Fields outputs, GroupBody body)
        : IOComponent(std::move(name), [inputs, outputs](IOComponent* part) {
              for (const auto& [name, width] : inputs) part->addPinDynamic(name, PinType::INPUT, width);
              for (const auto& [name, width] : outputs) part->addPinDynamic(name, PinType::OUTPUT, width);
          }), type_(std::move(type)), body_(std::move(body)) {}
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder& builder) override { body_(*this, builder); }
};
// A group compiled into one model (inside a compact device). It keeps the
// group's type name but lays out as its own variant: its child is the model,
// not the structural group's parts.
class DeviceCompactGroup : public IOComponent, public ComponentLayoutView {
    std::string type_,identity_;
    std::function<std::shared_ptr<IOComponent>()> reference_;
public:
    DeviceCompactGroup(std::string name,std::string type,Fields inputs,Fields outputs,
        std::string identity,std::function<std::shared_ptr<IOComponent>()> reference)
        :IOComponent(std::move(name),[inputs,outputs](IOComponent* part){
            for(const auto& [pin,width]:inputs)part->addPinDynamic(pin,PinType::INPUT,width);
            for(const auto& [pin,width]:outputs)part->addPinDynamic(pin,PinType::OUTPUT,width);
        }),type_(std::move(type)),identity_(std::move(identity)),reference_(std::move(reference)){}
    const char* getTypeName()const override{return type_.c_str();}
    std::string componentLayoutVariant()const override{return "compact";}
    void buildInternals(ComponentBuilder& builder)override{
        Fields ins,outs;
        for(const auto& [pin,p]:getAllInputPins())ins.emplace_back(pin,p->getWidth());
        for(const auto& [pin,p]:getAllOutputPins())outs.emplace_back(pin,p->getWidth());
        auto pins=[ins,outs](IOComponent* part){
            for(const auto& [pin,width]:ins)part->addPinDynamic(pin,PinType::INPUT,width);
            for(const auto& [pin,width]:outs)part->addPinDynamic(pin,PinType::OUTPUT,width);
        };
        auto context=builder.getBuildContext();
        auto part=circuit::detail::createGateListControl("MODEL","CompactModel<"+type_+">",std::move(pins),identity_,reference_,
            context?context->child("MODEL",std::nullopt):nullptr);
        addChild(part);
        Logic logic(builder);
        for(const auto& [name,pin]:getAllInputPins())logic.wire(pin,part->getInputPinDynamic(name));
        for(const auto& [name,pin]:getAllOutputPins())logic.wire(part->getOutputPinDynamic(name),pin);
        logic.finish();
    }
};
template<class StructuralDevice>
class DeviceBehavioral final : public StructuralDevice, public BehavioralComposition {
public:
    using StructuralDevice::StructuralDevice;
    void validateBehavioralComposition()const override{
        const auto validate=[&](const auto& self,const std::shared_ptr<Component>& part)->void{
            if(part->getInstanceMetadata()){
                if(part->getSelectedFidelity()!="behavioral")
                    throw std::logic_error("Compact device retained a structural functional part: "+part->getID());
                return; // the catalog has already validated this child model
            }
            const std::string type=part->getTypeName();
            if(type=="Rewire" || type=="ConstantValue")return;
            if(std::dynamic_pointer_cast<DeviceCompactGroup>(part)){
                for(const auto& child:part->getChildren())self(self,child);
                return;
            }
            if(std::dynamic_pointer_cast<circuit::memory_device::Primitive>(part) && part->getChildren().empty())return;
            throw std::logic_error("Compact device contains unapproved implementation detail: "+part->getID()+" ("+type+")");
        };
        for(const auto& child:this->getChildren())validate(validate,child);
    }
};
inline std::shared_ptr<IOComponent> group(ComponentBuilder& builder, const std::string& name, const std::string& type,
                  Fields inputs, Fields outputs, GroupBody body) {
    const auto& context=builder.getBuildContext();
    if(context && context->selection() && context->selection()->fidelity==circuit::Fidelity::Behavioral &&
       context->selection()->contract_id.starts_with("device.") && !context->selection()->contract_id.starts_with("device.fifo.") &&
       !context->selection()->contract_id.starts_with("device.counter.")) {
        const auto parameters=context->parameters();
        std::string identity=context->selection()->contract_id+"|"+type+"|"+circuit::canonicalizeParameters(parameters);
        for(const auto& [pin,width]:inputs)identity+="|i:"+pin+":"+std::to_string(width);
        for(const auto& [pin,width]:outputs)identity+="|o:"+pin+":"+std::to_string(width);
        auto reference=[inputs,outputs,body,type]()->std::shared_ptr<IOComponent>{
            std::vector<circuit::ProfileRule> storage;
            storage.push_back(circuit::preferFidelity(circuit::Fidelity::Behavioral,circuit::ProfileSelector::contract("storage.state-bit")));
            storage.push_back(circuit::preferFidelity(circuit::Fidelity::Behavioral,circuit::ProfileSelector::contract("memory.ff.cell")));
            for(size_t width:{1,2,3,4,5,8,16,32})storage.push_back(circuit::preferFidelity(circuit::Fidelity::Behavioral,
                circuit::ProfileSelector::contract("storage.register."+std::to_string(width))));
            auto profile=std::make_shared<const circuit::BuildProfile>(circuit::withProfileOverrides(
                circuit::maximallyStructural(),std::move(storage),"device-control-reference"));
            auto manifest=std::make_shared<circuit::BuildManifest>(profile->name(),profile->fingerprint());
            auto scope=circuit::BuildContext::rootScope(circuit::builtinComponentCatalog(),profile,manifest)->child("REFERENCE",std::nullopt);
            return Component::createWithContext<DeviceGroup>(scope,"REFERENCE",type,inputs,outputs,body);
        };
        return builder.addNewComponent<DeviceCompactGroup>(name,type,std::move(inputs),std::move(outputs),identity,std::move(reference));
    }
    return builder.addNewComponent<DeviceGroup>(name, type, std::move(inputs), std::move(outputs), std::move(body));
}
inline P input(IOComponent& part, const std::string& name) { return part.getInputPinDynamic(name); }
inline P output(IOComponent& part, const std::string& name) { return part.getOutputPinDynamic(name); }
inline void inputs(Logic& logic, IOComponent& parent, const std::shared_ptr<IOComponent>& child,
                   std::initializer_list<const char*> names) {
    for (const auto name : names) logic.wire(input(parent, name), child->getInputPinDynamic(name));
}
inline void outputs(Logic& logic, IOComponent& parent, const std::shared_ptr<IOComponent>& child,
                    std::initializer_list<const char*> names) {
    for (const auto name : names) logic.wire(child->getOutputPinDynamic(name), output(parent, name));
}
// Braced choices sequence nested construction identically in GCC and clang.
inline P wordMux(ComponentBuilder& b, Logic& l, const std::string& name, Logic::Choice choice) {
    auto [select,low,high] = choice;
    auto mux = b.add(circuit::word::mux(2, low->getWidth()), name);
    l.wire(select, mux->getInputPin("SEL"));
    l.wire(low, mux->getInputPinDynamic("A"));
    l.wire(high, mux->getInputPinDynamic("B"));
    return mux->getOutputPinDynamic("OUT");
}
// A 2:1 choice of two words on word gates (any word-gate width without a
// Mux<2,W>): PREFIXLOW = LOW & ~SELECT, PREFIXHIGH = HIGH & SELECT, OUT = their
// OR (named NAME). Zero-delay Rewires spread the select and its inverse; per
// bit these are a scalar mux's NOT/AND/OR gates and depth, so timing is the
// same.
inline P wordChoice(ComponentBuilder& b, circuit::structural::Logic& l, const std::string& prefix,
                    const std::string& name, P select, P low, P high) {
    using circuit::word::GateKind;
    const auto width = low->getWidth();
    const auto spread = [&](P bit, const std::string& spread_name) {
        std::vector<Rewire::BitMap> map;
        for (size_t i = 0; i < width; ++i) map.emplace_back("IN", 0, "OUT", i);
        auto r = b.addNewComponent<Rewire>(spread_name,
            std::vector<Rewire::WireSpec>{{"IN", 1}}, std::vector<Rewire::WireSpec>{{"OUT", width}}, map);
        l.wire(bit, r->getInputPinDynamic("IN"));
        return r->getOutputPinDynamic("OUT");
    };
    auto keep = b.add(circuit::families::wordGate(GateKind::And, width), prefix + "LOW");
    auto take = b.add(circuit::families::wordGate(GateKind::And, width), prefix + "HIGH");
    auto either = b.add(circuit::families::wordGate(GateKind::Or, width), name);
    l.wire(low, keep->getInputPinDynamic("A"));
    l.wire(spread(l.inv(select), prefix + "NOT_SELECT"), keep->getInputPinDynamic("B"));
    l.wire(high, take->getInputPinDynamic("A"));
    l.wire(spread(select, prefix + "SELECT"), take->getInputPinDynamic("B"));
    l.wire(keep->getOutputPinDynamic("OUT"), either->getInputPinDynamic("A"));
    l.wire(take->getOutputPinDynamic("OUT"), either->getInputPinDynamic("B"));
    return either->getOutputPinDynamic("OUT");
}
inline P statusMux(ComponentBuilder& b, Logic& l, const std::string& name, Logic::Choice choice) {
    auto [select,low,high] = choice;
    auto mux = group(b,name,"DeviceStatusMux",{{"SELECT",1},{"LOW",3},{"HIGH",3}},{{"OUT",3}},
        [](IOComponent& self,ComponentBuilder& gates) {
            Logic logic(gates);
            logic.wire(wordChoice(gates,logic,"","OUT_OR",input(self,"SELECT"),input(self,"LOW"),input(self,"HIGH")),output(self,"OUT"));
            logic.finish();
        });
    l.wire(select,mux->getInputPin("SELECT"));
    l.wire(low,mux->getInputPin<3>("LOW"));l.wire(high,mux->getInputPin<3>("HIGH"));
    return mux->getOutputPin<3>("OUT");
}
inline auto addressMatch(ComponentBuilder& b, const std::string& name, const std::string& type,
                         size_t ignored_low_bits, uint32_t expected) {
    return group(b,name,type,{{"ADDRESS",32}},{{"MATCH",1}},
        [ignored_low_bits,expected](IOComponent& self,ComponentBuilder& gates) {
            Logic l(gates);auto actual=l.split(input(self,"ADDRESS"));
            auto constant=gates.addNewComponent<ConstantValue<32>>("EXPECTED",expected);
            auto bits=l.split(constant->getOutputPin<32>("OUT"));V matches;
            for(size_t byte=0;byte<4;++byte) {
                if(8*(byte+1)<=ignored_low_bits)continue;
                const size_t skip=ignored_low_bits>8*byte?ignored_low_bits-8*byte:0;
                auto cell=group(gates,"BYTE_"+std::to_string(byte),"DeviceAddressByteMatch<"+std::to_string(skip)+">",
                    {{"ACTUAL",8},{"EXPECTED",8}},{{"MATCH",1}},
                    [skip](IOComponent& part,ComponentBuilder& builder) {
                        Logic c(builder);auto a=c.split(input(part,"ACTUAL")),e=c.split(input(part,"EXPECTED"));
                        c.wire(c.equal({V(a.begin()+skip,a.end()),V(e.begin()+skip,e.end())}),output(part,"MATCH"));c.finish();
                    });
                l.wire(l.join(V(actual.begin()+8*byte,actual.begin()+8*(byte+1))),cell->getInputPin<8>("ACTUAL"));
                l.wire(l.join(V(bits.begin()+8*byte,bits.begin()+8*(byte+1))),cell->getInputPin<8>("EXPECTED"));
                matches.push_back(cell->getOutputPin("MATCH"));
            }
            l.wire(l.all(matches),output(self,"MATCH"));l.finish();
        });
}
inline auto registerDecode(ComponentBuilder& b, const std::string& type, Fields offsets) {
    Fields pins;
    for (const auto& [name, offset] : offsets) pins.emplace_back(name, 1);
    return group(b, "REGISTER_DECODE", type, {{"ADDRESS",32}}, pins,
        [offsets](IOComponent& self, ComponentBuilder& builder) {
            Logic l(builder);
            for (const auto& [name, offset] : offsets) {
                auto match = addressMatch(builder,name,"DeviceRegisterAddressMatch",0,static_cast<uint32_t>(offset));
                l.wire(input(self,"ADDRESS"), match->getInputPin<32>("ADDRESS"));
                l.wire(match->getOutputPin("MATCH"), output(self,name));
            }
            l.finish();
        });
}
inline void decodePort(Logic& l, const std::shared_ptr<IOComponent>& port, const std::shared_ptr<IOComponent>& decode) {
    l.wire(port->getOutputPin<32>("ADDRESS"), decode->getInputPin<32>("ADDRESS"));
}
inline void selections(Logic& l, const std::shared_ptr<IOComponent>& decode,
                       const std::shared_ptr<IOComponent>& consumer, std::initializer_list<const char*> names) {
    for (auto name : names) l.wire(decode->getOutputPin(name), consumer->getInputPin(name));
}
inline void commandFields(Logic& l, const std::shared_ptr<IOComponent>& port,
                          const std::shared_ptr<IOComponent>& consumer) {
    for (auto name : {"IS_WRITE","WRITE_DATA","WRITE_MASK","WRITE_STROBE"})
        l.wire(port->getOutputPinDynamic(name), consumer->getInputPinDynamic(name));
}
// The register port's request fields into a register front end, and its
// answer back.
inline void frontEnd(Logic& l, const std::shared_ptr<IOComponent>& port, const std::shared_ptr<IOComponent>& registers) {
    for (auto name : {"ADDRESS","IS_WRITE","WRITE_DATA","WRITE_MASK","WRITE_STROBE"})
        l.wire(port->getOutputPinDynamic(name), registers->getInputPinDynamic(name));
    for (auto name : {"BACK_STATUS","BACK_READY","BACK_READ_DATA"})
        l.wire(registers->getOutputPinDynamic(name), port->getInputPinDynamic(name));
}
inline Fields commandInputs() {
    return {{"IS_WRITE",1},{"WRITE_DATA",32},{"WRITE_MASK",4},{"WRITE_STROBE",1}};
}
inline P validCommand(Logic& l, ComponentBuilder& b, P data) {
    auto bits = l.split(data);
    return l.land({knownWord(l,b,data),l.inv(l.any(V(bits.begin()+1,bits.end())))});
}
} // namespace circuit::device::detail
