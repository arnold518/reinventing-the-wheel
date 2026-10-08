#include "components/StructuralNetBuilder.hpp"
#include "modules/rv32i/RV32IPipelineRegisters.hpp"

#include "components/BasicComponent.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "modules/basic/Mux.hpp"
#include "modules/memory/FlipFlopCell.hpp"
#include "modules/memory/Register.hpp"
#include "modules/memory/MemoryController.hpp"
#include "modules/utility/Constant.hpp"
#include "modules/utility/Rewire.hpp"
#include <map>
#include <stdexcept>
#include <utility>

namespace {
struct PacketSlice { size_t offset,width; };
std::vector<PacketSlice> compactSlices(const std::string& field,const std::string& type_name) {
    if (field=="FETCH_STATUS" || field=="MEMORY_STATUS") return {{0,2}};
    if (field=="EXECUTION_STATUS") return {{0,1}};
    if (field=="REGISTER_ADDRESSES") {
        if (type_name=="RV32IIDEXPipelineRegister") return {{0,5},{5,5},{10,5}};
        return {{10,5}}; // only RD is used after Execute
    }
    if (field=="CONTROL") return {{0,16},{16,8},{24,4},{28,2},{30,1}};
    return {{0,32}};
}
bool compactPacket(const std::shared_ptr<circuit::BuildContext>& context) {
    if (!context || context->parameters().empty()) return false;
    const auto& p=context->parameters();
    if (p.size()!=1 || !p.contains("compact_packet") || p.at("compact_packet")!="1")
        throw std::invalid_argument("pipeline register requires compact_packet=1");
    return true;
}
const std::vector<std::string>& ifidFields() {
    static const std::vector<std::string> fields{
        "PC",
        "INSTRUCTION",
        "FETCH_STATUS",
    };
    return fields;
}

const std::vector<std::string>& idexFields() {
    static const std::vector<std::string> fields{
        "PC",
        "INSTRUCTION",
        "RS1_VALUE",
        "RS2_VALUE",
        "IMMEDIATE",
        "CONTROL",
        "REGISTER_ADDRESSES",
        "FETCH_STATUS",
    };
    return fields;
}

const std::vector<std::string>& exmemFields() {
    static const std::vector<std::string> fields{
        "PC",
        "INSTRUCTION",
        "ALU_RESULT",
        "STORE_DATA",
        "PC_PLUS_4",
        "NEXT_PC",
        "CONTROL",
        "REGISTER_ADDRESSES",
        "FETCH_STATUS",
        "EXECUTION_STATUS",
    };
    return fields;
}

const std::vector<std::string>& memwbFields() {
    static const std::vector<std::string> fields{
        "PC",
        "INSTRUCTION",
        "ALU_RESULT",
        "MEMORY_DATA",
        "STORE_DATA",
        "PC_PLUS_4",
        "NEXT_PC",
        "CONTROL",
        "REGISTER_ADDRESSES",
        "FETCH_STATUS",
        "EXECUTION_STATUS",
        "MEMORY_STATUS",
    };
    return fields;
}

circuit::PinInitializer pipelineRegisterPins(
    std::vector<std::string> fields) {
    return [fields = std::move(fields)](IOComponent* self) {
        self->addPin("CLK", PinType::INPUT);
        self->addPin("RST", PinType::INPUT);
        self->addPin("WRITE_ENABLE", PinType::INPUT);
        self->addPin("FLUSH", PinType::INPUT);
        self->addPin("D_VALID", PinType::INPUT);
        for (const auto& field : fields) {
            self->addPin<32>("D_" + field, PinType::INPUT);
        }
        self->addPin("Q_VALID", PinType::OUTPUT);
        for (const auto& field : fields) {
            self->addPin<32>("Q_" + field, PinType::OUTPUT);
        }
    };
}

LogicValue binaryOrUnknown(LogicValue value) {
    return value == LogicValue::LOW || value == LogicValue::HIGH
        ? value
        : LogicValue::UNKNOWN;
}

LogicValue merge(LogicValue held, LogicValue incoming) {
    held = binaryOrUnknown(held);
    incoming = binaryOrUnknown(incoming);
    return held == incoming ? held : LogicValue::UNKNOWN;
}

class RV32IPipelineRegisterBehavioral : public BasicComponent {
public:
    RV32IPipelineRegisterBehavioral(
        std::string name,
        std::string type_name,
        std::vector<std::string> fields,
        bool compact)
        : BasicComponent(
              std::move(name),
              1,
              pipelineRegisterPins(fields)),
          type_name_(std::move(type_name)),
          fields_(std::move(fields)),compact_(compact) {
        for (const auto& field : fields_) {
            stored_.emplace(
                field,
                std::vector<LogicValue>(32, LogicValue::UNKNOWN));
        }
    }

    const char* getTypeName() const override {
        return type_name_.c_str();
    }

    void evaluate(size_t current_time, Simulator& simulator) override {
        const auto clk = getInputValue("CLK");
        const auto rst = getInputValue("RST");
        const auto write_enable = getInputValue("WRITE_ENABLE");
        const auto flush = getInputValue("FLUSH");

        if (rst == LogicValue::HIGH) {
            valid_ = LogicValue::LOW;
            for (auto& [_, value] : stored_) {
                value.assign(32, LogicValue::LOW);
            }
        } else if (rst != LogicValue::LOW) {
            valid_ = merge(valid_, LogicValue::LOW);
            for (auto& [_, value] : stored_) {
                for (auto& bit : value) {
                    bit = merge(bit, LogicValue::LOW);
                }
            }
        } else if (previous_clk_ == LogicValue::LOW
                   && clk == LogicValue::HIGH) {
            if (write_enable == LogicValue::HIGH) {
                valid_ = flush == LogicValue::HIGH
                    ? LogicValue::LOW
                    : flush == LogicValue::LOW
                        ? binaryOrUnknown(getInputValue("D_VALID"))
                        : LogicValue::UNKNOWN;
                for (const auto& field : fields_) {
                    auto input = getInputPin<32>("D_" + field)
                        ->getValueAsVector();
                    if (compact_) {
                        std::vector<bool> used(32,false);
                        for (auto [offset,width]:compactSlices(field,type_name_))
                            for (size_t bit=offset;bit<offset+width;++bit) used[bit]=true;
                        for (size_t bit=0;bit<32;++bit) if (!used[bit]) input[bit]=LogicValue::LOW;
                    }
                    for (auto& bit : input) {
                        bit = binaryOrUnknown(bit);
                    }
                    stored_.at(field) = std::move(input);
                }
            } else if (write_enable != LogicValue::LOW) {
                const auto candidate_valid =
                    flush == LogicValue::HIGH
                    ? LogicValue::LOW
                    : flush == LogicValue::LOW
                        ? binaryOrUnknown(getInputValue("D_VALID"))
                        : LogicValue::UNKNOWN;
                valid_ = merge(valid_, candidate_valid);
                for (const auto& field : fields_) {
                    const auto input =
                        getInputPin<32>("D_" + field)->getValueAsVector();
                    auto& held = stored_.at(field);
                    std::vector<bool> used(32,true);
                    if (compact_) {
                        used.assign(32,false);
                        for (auto [offset,width]:compactSlices(field,type_name_))
                            for (size_t bit=offset;bit<offset+width;++bit) used[bit]=true;
                    }
                    for (size_t bit = 0; bit < held.size(); ++bit) {
                        held[bit] = used[bit]?merge(held[bit], input[bit]):LogicValue::LOW;
                    }
                }
            }
        }

        previous_clk_ = clk;
        _updateOutputWire(simulator, "Q_VALID", valid_, current_time);
        for (const auto& field : fields_) {
            _updateOutputWire<32>(
                simulator,
                "Q_" + field,
                stored_.at(field),
                current_time);
        }
    }

private:
    std::string type_name_;
    std::vector<std::string> fields_;
    bool compact_ = false;
    std::map<std::string, std::vector<LogicValue>> stored_;
    LogicValue valid_ = LogicValue::UNKNOWN;
    LogicValue previous_clk_ = LogicValue::UNKNOWN;
};
} // namespace

namespace circuit::families {
const ComponentFamily RV32IIFIDPipelineRegister{
    "rv32i.pipeline-register.if-id",
    "RV32IIFIDPipelineRegister",
    pipelineRegisterPins(ifidFields()),
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::RV32IIFIDPipelineRegister>(
            context, name,compactPacket(context));
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<RV32IPipelineRegisterBehavioral>(
            context,
            name,
            RV32IIFIDPipelineRegister::TypeName,
            ifidFields(),compactPacket(context));
    }};

const ComponentFamily RV32IIDEXPipelineRegister{
    "rv32i.pipeline-register.id-ex",
    "RV32IIDEXPipelineRegister",
    pipelineRegisterPins(idexFields()),
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::RV32IIDEXPipelineRegister>(
            context, name,compactPacket(context));
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<RV32IPipelineRegisterBehavioral>(
            context,
            name,
            RV32IIDEXPipelineRegister::TypeName,
            idexFields(),compactPacket(context));
    }};

const ComponentFamily RV32IEXMEMPipelineRegister{
    "rv32i.pipeline-register.ex-mem",
    "RV32IEXMEMPipelineRegister",
    pipelineRegisterPins(exmemFields()),
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::RV32IEXMEMPipelineRegister>(
            context, name,compactPacket(context));
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<RV32IPipelineRegisterBehavioral>(
            context,
            name,
            RV32IEXMEMPipelineRegister::TypeName,
            exmemFields(),compactPacket(context));
    }};

const ComponentFamily RV32IMEMWBPipelineRegister{
    "rv32i.pipeline-register.mem-wb",
    "RV32IMEMWBPipelineRegister",
    pipelineRegisterPins(memwbFields()),
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::RV32IMEMWBPipelineRegister>(
            context, name,compactPacket(context));
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<RV32IPipelineRegisterBehavioral>(
            context,
            name,
            RV32IMEMWBPipelineRegister::TypeName,
            memwbFields(),compactPacket(context));
    }};
} // namespace circuit::families

RV32IPipelineRegisterBlock::RV32IPipelineRegisterBlock(
    std::string name,
    std::vector<std::string> fields,
    PinInitFunction initializer,
    bool compact)
    : IOComponent(std::move(name), std::move(initializer)),
      fields_(std::move(fields)),compact_(compact) {}

void RV32IPipelineRegisterBlock::buildInternals(
    ComponentBuilder& builder) {
    circuit::detail::StructuralWiring w(builder);
    builder.addNewComponent<NOTGate>("NOT_FLUSH");
    builder.addNewComponent<ANDGate>("VALID_GATE");
    builder.add(circuit::families::FlipFlopCell, "VALID_STATE");

    w.connect(getInputPin("D_VALID"),
        {builder.getInputPin<ANDGate>("VALID_GATE", "A")});
    w.connect(builder.getOutputPin<NOTGate>("NOT_FLUSH", "OUT"),
        {builder.getInputPin<ANDGate>("VALID_GATE", "B")});
    w.connect(builder.getOutputPin<ANDGate>("VALID_GATE", "OUT"),
        {builder.getInputPin<IOComponent>("VALID_STATE", "D")});
    w.connect(builder.getOutputPin<IOComponent>("VALID_STATE", "Q"),
        {getOutputPin("Q_VALID")});

    std::vector<std::shared_ptr<Pin<>>> write_enable_sinks{
        builder.getInputPin<IOComponent>("VALID_STATE", "WE")};
    std::vector<std::shared_ptr<Pin<>>> clock_sinks{
        builder.getInputPin<IOComponent>("VALID_STATE", "CLK")};
    std::vector<std::shared_ptr<Pin<>>> reset_sinks{
        builder.getInputPin<IOComponent>("VALID_STATE", "RST")};
    std::vector<std::shared_ptr<Pin<>>> flush_sinks{
        builder.getInputPin<NOTGate>("NOT_FLUSH", "IN")};

    for (const auto& field : fields_) {
        const auto child = field + "_STATE";
        auto slices=compact_?compactSlices(field,getTypeName()):std::vector<PacketSlice>{{0,32}};
        if (slices.size()==1 && slices.front().offset==0 && slices.front().width==32) {
            builder.add(circuit::families::registerWord(32), child);
            w.connect(getInputPin<32>("D_" + field),
                {builder.getInputPin<IOComponent,32>(child,"D")});
            w.connect(builder.getOutputPin<IOComponent,32>(child,"Q"),
                {getOutputPin<32>("Q_" + field)});
            write_enable_sinks.push_back(builder.getInputPin<IOComponent>(child,"WE"));
            clock_sinks.push_back(builder.getInputPin<IOComponent>(child,"CLK"));
            reset_sinks.push_back(builder.getInputPin<IOComponent>(child,"RST"));
            continue;
        }
        std::vector<Rewire::WireSpec> slice_ports;
        std::vector<Rewire::BitMap> input_maps,output_maps;
        for (size_t i=0;i<slices.size();++i) {
            const auto name="SLICE_"+std::to_string(i);
            slice_ports.push_back({name,slices[i].width});
            auto in=identity_mapping("IN",slices[i].offset,slices[i].width,name);
            input_maps.insert(input_maps.end(),in.begin(),in.end());
            auto out=identity_mapping(name,0,slices[i].width,"OUT",slices[i].offset);
            output_maps.insert(output_maps.end(),out.begin(),out.end());
        }
        auto split=builder.addNewComponent<Rewire>(field+"_SPLIT",
            std::vector<Rewire::WireSpec>{{"IN",32}},slice_ports,input_maps);
        auto join=builder.addNewComponent<Rewire>(field+"_JOIN",slice_ports,
            std::vector<Rewire::WireSpec>{{"OUT",32}},output_maps,Rewire::UnmappedBitValue::LOW);
        w.connect(getInputPin<32>("D_"+field),{split->getInputPin<32>("IN")});
        w.connect(join->getOutputPin<32>("OUT"),{getOutputPin<32>("Q_"+field)});
        for (size_t i=0;i<slices.size();++i) {
            const auto name="SLICE_"+std::to_string(i);
            const auto part=child+"_"+std::to_string(i);
            auto state=builder.add(circuit::families::registerWord(slices[i].width),part);
            w.connect(split->getOutputPinDynamic(name),
                {state->getInputPinDynamic("D")});
            w.connect(state->getOutputPinDynamic("Q"),
                {join->getInputPinDynamic(name)});
            write_enable_sinks.push_back(builder.getInputPin<IOComponent>(part,"WE"));
            clock_sinks.push_back(builder.getInputPin<IOComponent>(part,"CLK"));
            reset_sinks.push_back(builder.getInputPin<IOComponent>(part,"RST"));
        }
    }

    w.connect(getInputPin("WRITE_ENABLE"),
        write_enable_sinks);
    w.connect(getInputPin("CLK"), clock_sinks);
    w.connect(getInputPin("RST"), reset_sinks);
    w.connect(getInputPin("FLUSH"), flush_sinks);
    w.finish();
}

RV32IIFIDPipelineRegister::RV32IIFIDPipelineRegister(std::string name,bool compact)
    : RV32IPipelineRegisterBlock(
          std::move(name),
          ifidFields(),
          circuit::families::RV32IIFIDPipelineRegister.pinInitializer(),compact) {}

RV32IIDEXPipelineRegister::RV32IIDEXPipelineRegister(std::string name,bool compact)
    : RV32IPipelineRegisterBlock(
          std::move(name),
          idexFields(),
          circuit::families::RV32IIDEXPipelineRegister.pinInitializer(),compact) {}

RV32IEXMEMPipelineRegister::RV32IEXMEMPipelineRegister(std::string name,bool compact)
    : RV32IPipelineRegisterBlock(
          std::move(name),
          exmemFields(),
          circuit::families::RV32IEXMEMPipelineRegister.pinInitializer(),compact) {}

RV32IMEMWBPipelineRegister::RV32IMEMWBPipelineRegister(std::string name,bool compact)
    : RV32IPipelineRegisterBlock(
          std::move(name),
          memwbFields(),
          circuit::families::RV32IMEMWBPipelineRegister.pinInitializer(),compact) {}
