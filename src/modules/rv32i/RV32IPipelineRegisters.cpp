#include "modules/rv32i/RV32IPipelineRegisters.hpp"

#include "components/BasicComponent.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "modules/basic/Mux.hpp"
#include "modules/memory/MemoryBit.hpp"
#include "modules/memory/Register32.hpp"
#include "modules/utility/Constant.hpp"
#include <map>
#include <utility>

namespace {
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

class RV32IPipelineRegisterDirect : public BasicComponent {
public:
    RV32IPipelineRegisterDirect(
        std::string name,
        std::string type_name,
        std::vector<std::string> fields)
        : BasicComponent(
              std::move(name),
              1,
              pipelineRegisterPins(fields)),
          type_name_(std::move(type_name)),
          fields_(std::move(fields)) {
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
                    for (size_t bit = 0; bit < held.size(); ++bit) {
                        held[bit] = merge(held[bit], input[bit]);
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
            context, name);
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<RV32IPipelineRegisterDirect>(
            context,
            name,
            RV32IIFIDPipelineRegister::TypeName,
            ifidFields());
    }};

const ComponentFamily RV32IIDEXPipelineRegister{
    "rv32i.pipeline-register.id-ex",
    "RV32IIDEXPipelineRegister",
    pipelineRegisterPins(idexFields()),
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::RV32IIDEXPipelineRegister>(
            context, name);
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<RV32IPipelineRegisterDirect>(
            context,
            name,
            RV32IIDEXPipelineRegister::TypeName,
            idexFields());
    }};

const ComponentFamily RV32IEXMEMPipelineRegister{
    "rv32i.pipeline-register.ex-mem",
    "RV32IEXMEMPipelineRegister",
    pipelineRegisterPins(exmemFields()),
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::RV32IEXMEMPipelineRegister>(
            context, name);
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<RV32IPipelineRegisterDirect>(
            context,
            name,
            RV32IEXMEMPipelineRegister::TypeName,
            exmemFields());
    }};

const ComponentFamily RV32IMEMWBPipelineRegister{
    "rv32i.pipeline-register.mem-wb",
    "RV32IMEMWBPipelineRegister",
    pipelineRegisterPins(memwbFields()),
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::RV32IMEMWBPipelineRegister>(
            context, name);
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<RV32IPipelineRegisterDirect>(
            context,
            name,
            RV32IMEMWBPipelineRegister::TypeName,
            memwbFields());
    }};
} // namespace circuit::families

RV32IPipelineRegisterBlock::RV32IPipelineRegisterBlock(
    std::string name,
    std::vector<std::string> fields,
    PinInitFunction initializer)
    : IOComponent(std::move(name), std::move(initializer)),
      fields_(std::move(fields)) {}

void RV32IPipelineRegisterBlock::buildInternals(
    ComponentBuilder& builder) {
    builder.addNewComponent<NOTGate>("NOT_FLUSH");
    builder.addNewComponent<ANDGate>("VALID_GATE");
    builder.add(circuit::families::MemoryBit, "VALID_STATE");

    builder.addNewWire(
        "D_VALID_to_gate",
        getInputPin("D_VALID"),
        {builder.getInputPin<ANDGate>("VALID_GATE", "A")});
    builder.addNewWire(
        "NOT_FLUSH_to_gate",
        builder.getOutputPin<NOTGate>("NOT_FLUSH", "OUT"),
        {builder.getInputPin<ANDGate>("VALID_GATE", "B")});
    builder.addNewWire(
        "VALID_D_to_state",
        builder.getOutputPin<ANDGate>("VALID_GATE", "OUT"),
        {builder.getInputPin<IOComponent>("VALID_STATE", "D")});
    builder.addNewWire(
        "VALID_Q_to_output",
        builder.getOutputPin<IOComponent>("VALID_STATE", "Q"),
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
        builder.add(circuit::families::Register32, child);
        builder.addNewWire<32>(
            field + "_D_to_state",
            getInputPin<32>("D_" + field),
            {builder.getInputPin<IOComponent, 32>(child, "D")});
        builder.addNewWire<32>(
            field + "_Q_to_output",
            builder.getOutputPin<IOComponent, 32>(child, "Q"),
            {getOutputPin<32>("Q_" + field)});
        write_enable_sinks.push_back(
            builder.getInputPin<IOComponent>(child, "WE"));
        clock_sinks.push_back(
            builder.getInputPin<IOComponent>(child, "CLK"));
        reset_sinks.push_back(
            builder.getInputPin<IOComponent>(child, "RST"));
    }

    builder.addNewWire(
        "WRITE_ENABLE_fanout",
        getInputPin("WRITE_ENABLE"),
        write_enable_sinks);
    builder.addNewWire("CLK_fanout", getInputPin("CLK"), clock_sinks);
    builder.addNewWire("RST_fanout", getInputPin("RST"), reset_sinks);
    builder.addNewWire(
        "FLUSH_fanout", getInputPin("FLUSH"), flush_sinks);
}

RV32IIFIDPipelineRegister::RV32IIFIDPipelineRegister(std::string name)
    : RV32IPipelineRegisterBlock(
          std::move(name),
          ifidFields(),
          circuit::families::RV32IIFIDPipelineRegister.pinInitializer()) {}

RV32IIDEXPipelineRegister::RV32IIDEXPipelineRegister(std::string name)
    : RV32IPipelineRegisterBlock(
          std::move(name),
          idexFields(),
          circuit::families::RV32IIDEXPipelineRegister.pinInitializer()) {}

RV32IEXMEMPipelineRegister::RV32IEXMEMPipelineRegister(std::string name)
    : RV32IPipelineRegisterBlock(
          std::move(name),
          exmemFields(),
          circuit::families::RV32IEXMEMPipelineRegister.pinInitializer()) {}

RV32IMEMWBPipelineRegister::RV32IMEMWBPipelineRegister(std::string name)
    : RV32IPipelineRegisterBlock(
          std::move(name),
          memwbFields(),
          circuit::families::RV32IMEMWBPipelineRegister.pinInitializer()) {}
