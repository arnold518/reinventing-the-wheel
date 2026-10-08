#include "modules/rv32i/RV32IDecodeControlUnit.hpp"
#include "modules/rv32i/RV32IDecodeControlBehavioral.hpp"

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "components/StructuralNetBuilder.hpp"
#include "modules/basic/Mux.hpp"
#include "modules/composite/ALU32.hpp"
#include "modules/rv32i/RV32IBitPatternMatcher.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Constant.hpp"
#include "modules/utility/Rewire.hpp"
#include <cstdint>
#include <functional>
#include <iomanip>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
enum ImmediateSelect : uint8_t {
    IMM_ZERO = 0,
    IMM_I = 1,
    IMM_S = 2,
    IMM_B = 3,
    IMM_U = 4,
    IMM_J = 5,
};

constexpr uint8_t ALU_A_RS1 = 0;
constexpr uint8_t ALU_A_PC = 1;
constexpr uint8_t ALU_A_ZERO = 2;
constexpr uint8_t ALU_B_RS2 = 0;
constexpr uint8_t ALU_B_IMMEDIATE = 1;
constexpr uint8_t ALU_B_ZERO = 2;
constexpr uint8_t WB_NONE = 0;
constexpr uint8_t WB_ALU = 1;
constexpr uint8_t WB_MEMORY = 2;
constexpr uint8_t WB_PC_PLUS_4 = 3;
constexpr uint8_t MEM_BYTE = 0;
constexpr uint8_t MEM_HALF = 1;
constexpr uint8_t MEM_WORD = 2;
constexpr uint8_t TRAP_NONE = 0;
constexpr uint8_t TRAP_ILLEGAL = 1;
constexpr uint8_t TRAP_ENVIRONMENT = 2;

struct ControlWord {
    uint8_t alu_op = ALU32Op::ZERO;
    uint8_t alu_a = ALU_A_ZERO;
    uint8_t alu_b = ALU_B_ZERO;
    bool reg_write = false;
    bool mem_read = false;
    bool mem_write = false;
    uint8_t writeback = WB_NONE;
    uint8_t mem_size = MEM_WORD;
    bool load_sign_extend = false;
    bool uses_rs1 = false;
    bool uses_rs2 = false;
    uint8_t branch = 0;
    uint8_t jump = 0;
    bool halt = false;
    bool trap = false;
    uint8_t trap_cause = TRAP_NONE;
    uint8_t immediate = IMM_ZERO;
};

struct InstructionPattern {
    std::string name;
    uint32_t mask;
    uint32_t value;
    ControlWord control;
};

ControlWord regAlu(uint8_t op, uint8_t source_a, uint8_t source_b, uint8_t immediate) {
    ControlWord control;
    control.alu_op = op;
    control.alu_a = source_a;
    control.alu_b = source_b;
    control.reg_write = true;
    control.writeback = WB_ALU;
    control.immediate = immediate;
    control.uses_rs1 = source_a == ALU_A_RS1;
    control.uses_rs2 = source_b == ALU_B_RS2;
    return control;
}

InstructionPattern rType(const char* name, uint8_t funct3, uint8_t funct7, uint8_t alu_op) {
    return {name,
            0xFE00707FU,
            (static_cast<uint32_t>(funct7) << 25U) | (static_cast<uint32_t>(funct3) << 12U) | 0x33U,
            regAlu(alu_op, ALU_A_RS1, ALU_B_RS2, IMM_ZERO)};
}

InstructionPattern iAlu(const char* name, uint8_t funct3, uint8_t alu_op) {
    return {name,
            0x0000707FU,
            (static_cast<uint32_t>(funct3) << 12U) | 0x13U,
            regAlu(alu_op, ALU_A_RS1, ALU_B_IMMEDIATE, IMM_I)};
}

InstructionPattern iShift(const char* name, uint8_t funct3, uint8_t funct7, uint8_t alu_op) {
    return {name,
            0xFE00707FU,
            (static_cast<uint32_t>(funct7) << 25U) | (static_cast<uint32_t>(funct3) << 12U) | 0x13U,
            regAlu(alu_op, ALU_A_RS1, ALU_B_IMMEDIATE, IMM_I)};
}

InstructionPattern load(const char* name, uint8_t funct3, uint8_t size, bool sign_extend) {
    ControlWord control = regAlu(ALU32Op::ADD, ALU_A_RS1, ALU_B_IMMEDIATE, IMM_I);
    control.mem_read = true;
    control.writeback = WB_MEMORY;
    control.mem_size = size;
    control.load_sign_extend = sign_extend;
    return {name, 0x0000707FU, (static_cast<uint32_t>(funct3) << 12U) | 0x03U, control};
}

InstructionPattern store(const char* name, uint8_t funct3, uint8_t size) {
    ControlWord control;
    control.alu_op = ALU32Op::ADD;
    control.alu_a = ALU_A_RS1;
    control.alu_b = ALU_B_IMMEDIATE;
    control.mem_write = true;
    control.mem_size = size;
    control.immediate = IMM_S;
    control.uses_rs1 = true;
    control.uses_rs2 = true;
    return {name, 0x0000707FU, (static_cast<uint32_t>(funct3) << 12U) | 0x23U, control};
}

InstructionPattern branch(const char* name, uint8_t funct3, uint8_t branch_type) {
    ControlWord control;
    control.alu_op = ALU32Op::SUB;
    control.alu_a = ALU_A_RS1;
    control.alu_b = ALU_B_RS2;
    control.branch = branch_type;
    control.immediate = IMM_B;
    control.uses_rs1 = true;
    control.uses_rs2 = true;
    return {name, 0x0000707FU, (static_cast<uint32_t>(funct3) << 12U) | 0x63U, control};
}

const std::vector<InstructionPattern>& patterns() {
    static const std::vector<InstructionPattern> result = [] {
        std::vector<InstructionPattern> values{
            rType("ADD", 0x0, 0x00, ALU32Op::ADD),
            rType("SUB", 0x0, 0x20, ALU32Op::SUB),
            rType("SLL", 0x1, 0x00, ALU32Op::SLL),
            rType("SLT", 0x2, 0x00, ALU32Op::SLT),
            rType("SLTU", 0x3, 0x00, ALU32Op::SLTU),
            rType("XOR", 0x4, 0x00, ALU32Op::XOR),
            rType("SRL", 0x5, 0x00, ALU32Op::SRL),
            rType("SRA", 0x5, 0x20, ALU32Op::SRA),
            rType("OR", 0x6, 0x00, ALU32Op::OR),
            rType("AND", 0x7, 0x00, ALU32Op::AND),

            iAlu("ADDI", 0x0, ALU32Op::ADD),
            iAlu("SLTI", 0x2, ALU32Op::SLT),
            iAlu("SLTIU", 0x3, ALU32Op::SLTU),
            iAlu("XORI", 0x4, ALU32Op::XOR),
            iAlu("ORI", 0x6, ALU32Op::OR),
            iAlu("ANDI", 0x7, ALU32Op::AND),
            iShift("SLLI", 0x1, 0x00, ALU32Op::SLL),
            iShift("SRLI", 0x5, 0x00, ALU32Op::SRL),
            iShift("SRAI", 0x5, 0x20, ALU32Op::SRA),

            load("LB", 0x0, MEM_BYTE, true),
            load("LH", 0x1, MEM_HALF, true),
            load("LW", 0x2, MEM_WORD, false),
            load("LBU", 0x4, MEM_BYTE, false),
            load("LHU", 0x5, MEM_HALF, false),
            store("SB", 0x0, MEM_BYTE),
            store("SH", 0x1, MEM_HALF),
            store("SW", 0x2, MEM_WORD),

            branch("BEQ", 0x0, 1),
            branch("BNE", 0x1, 2),
            branch("BLT", 0x4, 3),
            branch("BGE", 0x5, 4),
            branch("BLTU", 0x6, 5),
            branch("BGEU", 0x7, 6),
        };

        ControlWord jal = regAlu(ALU32Op::ADD, ALU_A_PC, ALU_B_IMMEDIATE, IMM_J);
        jal.writeback = WB_PC_PLUS_4;
        jal.jump = 1;
        values.push_back({"JAL", 0x0000007FU, 0x6FU, jal});

        ControlWord jalr = regAlu(ALU32Op::ADD, ALU_A_RS1, ALU_B_IMMEDIATE, IMM_I);
        jalr.writeback = WB_PC_PLUS_4;
        jalr.jump = 2;
        values.push_back({"JALR", 0x0000707FU, 0x67U, jalr});

        values.push_back({"LUI", 0x0000007FU, 0x37U,
                          regAlu(ALU32Op::PASS_B, ALU_A_ZERO, ALU_B_IMMEDIATE, IMM_U)});
        values.push_back({"AUIPC", 0x0000007FU, 0x17U,
                          regAlu(ALU32Op::ADD, ALU_A_PC, ALU_B_IMMEDIATE, IMM_U)});

        ControlWord fence;
        fence.immediate = IMM_I;
        values.push_back({"FENCE", 0x0000707FU, 0x0FU, fence});

        ControlWord ecall;
        ecall.trap = true;
        ecall.trap_cause = TRAP_ENVIRONMENT;
        ecall.immediate = IMM_I;
        values.push_back({"ECALL", 0xFFFFFFFFU, 0x00000073U, ecall});

        ControlWord ebreak;
        ebreak.halt = true;
        ebreak.immediate = IMM_I;
        values.push_back({"EBREAK", 0xFFFFFFFFU, 0x00100073U, ebreak});
        return values;
    }();
    return result;
}

void appendMappings(std::vector<Rewire::BitMap>& destination, std::vector<Rewire::BitMap> source) {
    destination.insert(destination.end(), source.begin(), source.end());
}

std::string hexText(uint32_t value, size_t width) {
    std::ostringstream out;
    out << std::uppercase << std::hex << std::setfill('0')
        << std::setw(static_cast<int>(width)) << value;
    return out.str();
}

std::vector<Rewire::BitMap> decodeMappings() {
    std::vector<Rewire::BitMap> mappings;
    appendMappings(mappings, identity_mapping("INSTRUCTION", 15, 5, "RS1"));
    appendMappings(mappings, identity_mapping("INSTRUCTION", 20, 5, "RS2"));
    appendMappings(mappings, identity_mapping("INSTRUCTION", 7, 5, "RD"));

    appendMappings(mappings, identity_mapping("INSTRUCTION", 20, 12, "IMM_I"));
    for (size_t bit = 12; bit < 32; ++bit) mappings.emplace_back("INSTRUCTION", 31, "IMM_I", bit);

    appendMappings(mappings, identity_mapping("INSTRUCTION", 7, 5, "IMM_S"));
    appendMappings(mappings, identity_mapping("INSTRUCTION", 25, 7, "IMM_S", 5));
    for (size_t bit = 12; bit < 32; ++bit) mappings.emplace_back("INSTRUCTION", 31, "IMM_S", bit);

    appendMappings(mappings, identity_mapping("INSTRUCTION", 8, 4, "IMM_B", 1));
    appendMappings(mappings, identity_mapping("INSTRUCTION", 25, 6, "IMM_B", 5));
    mappings.emplace_back("INSTRUCTION", 7, "IMM_B", 11);
    mappings.emplace_back("INSTRUCTION", 31, "IMM_B", 12);
    for (size_t bit = 13; bit < 32; ++bit) mappings.emplace_back("INSTRUCTION", 31, "IMM_B", bit);

    appendMappings(mappings, identity_mapping("INSTRUCTION", 12, 20, "IMM_U", 12));

    appendMappings(mappings, identity_mapping("INSTRUCTION", 21, 10, "IMM_J", 1));
    mappings.emplace_back("INSTRUCTION", 20, "IMM_J", 11);
    appendMappings(mappings, identity_mapping("INSTRUCTION", 12, 8, "IMM_J", 12));
    mappings.emplace_back("INSTRUCTION", 31, "IMM_J", 20);
    for (size_t bit = 21; bit < 32; ++bit) mappings.emplace_back("INSTRUCTION", 31, "IMM_J", bit);
    return mappings;
}
}

namespace {
// Fixed structural subcircuits. These boundaries add no logic or state:
// the shared predicates and bit equations remain expandable.
void controlOutputs(IOComponent* self) {
    self->addPin<5>("ALU_OP", PinType::OUTPUT);
    self->addPin<2>("ALU_A_SEL", PinType::OUTPUT);
    self->addPin<2>("ALU_B_SEL", PinType::OUTPUT);
    self->addPin("LEGAL", PinType::OUTPUT);
    self->addPin("REG_WRITE", PinType::OUTPUT);
    self->addPin("MEM_READ", PinType::OUTPUT);
    self->addPin("MEM_WRITE", PinType::OUTPUT);
    self->addPin<2>("WRITEBACK_SEL", PinType::OUTPUT);
    self->addPin<2>("MEM_SIZE", PinType::OUTPUT);
    self->addPin("LOAD_SIGN_EXTEND", PinType::OUTPUT);
    self->addPin("USES_RS1", PinType::OUTPUT);
    self->addPin("USES_RS2", PinType::OUTPUT);
    self->addPin<3>("BRANCH_TYPE", PinType::OUTPUT);
    self->addPin<2>("JUMP_TYPE", PinType::OUTPUT);
    self->addPin("HALT_REQUEST", PinType::OUTPUT);
    self->addPin("TRAP_REQUEST", PinType::OUTPUT);
    self->addPin<4>("DECODE_TRAP_CAUSE", PinType::OUTPUT);
    self->addPin<3>("IMMEDIATE_SELECT", PinType::OUTPUT);
}

class RV32IInstructionRecognition : public IOComponent {
public:
    explicit RV32IInstructionRecognition(std::string name)
        : IOComponent(std::move(name), [](IOComponent* s) {
            s->addPin<32>("INSTRUCTION", PinType::INPUT);
            s->addPin<32>("MATCH_LO", PinType::OUTPUT);
            s->addPin<8>("MATCH_HI", PinType::OUTPUT);
        }) {}
    const char* getTypeName() const override { return "RV32IInstructionRecognition"; }
    void buildInternals(ComponentBuilder& builder) override {
        circuit::detail::StructuralWiring w(builder);
        const auto& instruction_patterns = patterns();
        std::vector<Rewire::WireSpec> inputs;
        std::vector<Rewire::BitMap> mappings;
        for (size_t i = 0; i < instruction_patterns.size(); ++i) {
            const auto& name = instruction_patterns[i].name;
            inputs.push_back({name, 1});
            mappings.emplace_back(name, 0, i < 32 ? "MATCH_LO" : "MATCH_HI", i % 32);
        }
        auto pack = builder.addNewComponent<Rewire>("MATCH_LINES", inputs,
            std::vector<Rewire::WireSpec>{{"MATCH_LO",32},{"MATCH_HI",8}}, mappings);
        std::vector<std::vector<std::shared_ptr<Pin<>>>> match_sinks(instruction_patterns.size());
        for (size_t i = 0; i < instruction_patterns.size(); ++i)
            match_sinks[i].push_back(pack->getInputPin(instruction_patterns[i].name));
        std::vector<std::shared_ptr<Pin<32>>> instruction_sinks;
        struct SharedPredicate {
            std::string name;
            uint32_t mask;
            uint32_t value;
            std::vector<std::shared_ptr<Pin<>>> sinks;
        };
        std::vector<SharedPredicate> predicates;
        std::map<std::pair<uint32_t, uint32_t>, size_t> predicate_indexes;

        auto addPredicate = [&](uint32_t mask,
                                uint32_t value,
                                std::string name) {
            const auto key = std::make_pair(mask, value & mask);
            if (const auto found = predicate_indexes.find(key);
                found != predicate_indexes.end()) {
                return found->second;
            }

            const size_t index = predicates.size();
            predicates.push_back(
                {std::move(name), mask, value & mask, {}});
            predicate_indexes.emplace(key, index);
            const auto& predicate = predicates.back();
            builder.addNewComponent<RV32IBitPatternMatcher>(
                predicate.name, predicate.mask, predicate.value);
            instruction_sinks.push_back(
                builder.getInputPin<RV32IBitPatternMatcher, 32>(
                    predicate.name, "INPUT"));
            return index;
        };

        constexpr uint32_t OpcodeMask = 0x0000007FU;
        constexpr uint32_t Funct3Mask = 0x00007000U;
        constexpr uint32_t Funct7Mask = 0xFE000000U;
        constexpr size_t NoPredicate = static_cast<size_t>(-1);
        std::vector<size_t> direct_predicates(
            instruction_patterns.size(), NoPredicate);
        std::map<std::pair<size_t,size_t>,std::string> shared_prefixes;
        std::map<std::string,std::vector<std::shared_ptr<Pin<>>>> gate_sinks;

        for (size_t index = 0; index < instruction_patterns.size(); ++index) {
            const auto& pattern = instruction_patterns[index];
            std::vector<size_t> terms;
            if (pattern.mask == 0xFFFFFFFFU) {
                terms.push_back(addPredicate(
                    pattern.mask,
                    pattern.value,
                    "PREDECODE_EXACT_" + pattern.name));
            } else {
                uint32_t factored_mask = OpcodeMask;
                terms.push_back(addPredicate(
                    OpcodeMask,
                    pattern.value,
                    "PREDECODE_OPCODE_"
                        + hexText(pattern.value & OpcodeMask, 2)));
                if ((pattern.mask & Funct3Mask) != 0) {
                    factored_mask |= Funct3Mask;
                    terms.push_back(addPredicate(
                        Funct3Mask,
                        pattern.value,
                        "PREDECODE_FUNCT3_"
                            + hexText(
                                (pattern.value & Funct3Mask) >> 12U, 1)));
                }
                if ((pattern.mask & Funct7Mask) != 0) {
                    factored_mask |= Funct7Mask;
                    terms.push_back(addPredicate(
                        Funct7Mask,
                        pattern.value,
                        "PREDECODE_FUNCT7_"
                            + hexText(
                                (pattern.value & Funct7Mask) >> 25U, 2)));
                }
                if (factored_mask != pattern.mask) {
                    throw std::logic_error(
                        "RV32I decode pattern cannot be factored into "
                        "opcode/funct3/funct7 predicates: " + pattern.name);
                }
            }

            if (terms.size() == 1) {
                direct_predicates[index] = terms.front();
                continue;
            }

            std::string previous_gate;
            size_t first_term=1;
            if (terms.size() == 3) {
                auto key=std::make_pair(terms[0],terms[1]);
                auto found=shared_prefixes.find(key);
                if (found != shared_prefixes.end()) {
                    previous_gate=found->second;
                    first_term=2;
                }
            }
            for (size_t term = first_term; term < terms.size(); ++term) {
                const auto gate =
                    "RECOGNIZE_" + pattern.name + "_AND_"
                    + std::to_string(term);
                builder.addNewComponent<ANDGate>(gate);
                if (term == 1) {
                    predicates[terms[0]].sinks.push_back(
                        builder.getInputPin<ANDGate>(gate, "A"));
                    if (terms.size() == 3)
                        shared_prefixes.emplace(std::make_pair(terms[0],terms[1]),gate);
                } else {
                    gate_sinks[previous_gate].push_back(builder.getInputPin<ANDGate>(gate,"A"));
                }
                predicates[terms[term]].sinks.push_back(
                    builder.getInputPin<ANDGate>(gate, "B"));
                previous_gate = gate;
            }
            gate_sinks[previous_gate].insert(gate_sinks[previous_gate].end(),
                match_sinks[index].begin(),match_sinks[index].end());
        }

        w.connect(getInputPin<32>("INSTRUCTION"), instruction_sinks);
        for (size_t i = 0; i < instruction_patterns.size(); ++i) {
            if (direct_predicates[i] != NoPredicate) {
                auto& sinks = predicates[direct_predicates[i]].sinks;
                sinks.insert(sinks.end(), match_sinks[i].begin(), match_sinks[i].end());
            }
        }
        for (const auto& [gate,sinks]:gate_sinks)
            w.connect(builder.getOutputPin<ANDGate>(gate,"OUT"),sinks);
        for (const auto& predicate : predicates)
            w.connect(builder.getOutputPin<RV32IBitPatternMatcher>(predicate.name, "MATCH"), predicate.sinks);
        w.connect(pack->getOutputPin<32>("MATCH_LO"), {getOutputPin<32>("MATCH_LO")});
        w.connect(pack->getOutputPin<8>("MATCH_HI"), {getOutputPin<8>("MATCH_HI")});
        w.finish();
    }
};

// One control output owns its bit equations. Parameterized type keys prevent
// the layout of a five-bit ALU opcode from leaking into a one-bit write enable.
class RV32IDecodeControlField : public IOComponent {
    std::string type_;
    size_t width_;
    uint8_t default_;
    std::vector<uint8_t> values_;
public:
    RV32IDecodeControlField(std::string name, size_t width, uint8_t fallback,
                            std::vector<uint8_t> values)
        : IOComponent(name, [width](IOComponent* s) {
            s->addPin<32>("MATCH_LO", PinType::INPUT);
            s->addPin<8>("MATCH_HI", PinType::INPUT);
            s->addPinDynamic("OUT", PinType::OUTPUT, width);
        }), type_("RV32IDecodeControlField<" + name + ">"), width_(width),
           default_(fallback), values_(std::move(values)) {}
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder& b) override {
        circuit::detail::StructuralWiring w(b);
        std::vector<Rewire::WireSpec> outputs;
        std::vector<Rewire::BitMap> mappings;
        for (size_t i = 0; i < patterns().size(); ++i) {
            outputs.push_back({patterns()[i].name, 1});
            mappings.emplace_back(i < 32 ? "MATCH_LO" : "MATCH_HI", i % 32, patterns()[i].name, 0);
        }
        auto matches = b.addNewComponent<Rewire>("MATCH_BITS",
            std::vector<Rewire::WireSpec>{{"MATCH_LO",32},{"MATCH_HI",8}}, outputs, mappings);
        for (auto pin : {"MATCH_LO", "MATCH_HI"})
            w.connect(getInputPinDynamic(pin), matches->getInputPinDynamic(pin));
        std::vector<Rewire::WireSpec> bit_inputs;
        std::vector<Rewire::BitMap> bit_maps;
        for (size_t bit = 0; bit < width_; ++bit) {
            bit_inputs.push_back({"BIT_" + std::to_string(bit), 1});
            bit_maps.emplace_back("BIT_" + std::to_string(bit), 0, "OUT", bit);
        }
        auto join = b.addNewComponent<Rewire>("OUTPUT_BITS", bit_inputs,
            std::vector<Rewire::WireSpec>{{"OUT", width_}}, bit_maps);
        // A few output bits use the same adjacent recognition pair. Share
        // those OR nodes within this field while retaining balanced depth.
        std::map<std::pair<std::string,std::string>,
                 circuit::detail::StructuralWiring::PinPtr> shared_or;
        auto reduceShared = [&](const std::string& name,
                                std::vector<circuit::detail::StructuralWiring::PinPtr> inputs) {
            for (size_t level = 0; inputs.size() > 1; ++level) {
                std::vector<circuit::detail::StructuralWiring::PinPtr> next;
                for (size_t i = 0; i < inputs.size(); i += 2) {
                    if (i + 1 == inputs.size()) {
                        next.push_back(inputs[i]);
                        continue;
                    }
                    const auto key = std::make_pair(inputs[i]->getID(), inputs[i+1]->getID());
                    auto [found, inserted] = shared_or.try_emplace(key);
                    if (inserted)
                        found->second = w.either(name + "_" + std::to_string(level) + "_" +
                                                  std::to_string(i/2), inputs[i], inputs[i+1]);
                    next.push_back(found->second);
                }
                inputs = std::move(next);
            }
            return inputs.front();
        };
        for (size_t bit = 0; bit < width_; ++bit) {
            const bool fallback = ((default_ >> bit) & 1U) != 0;
            std::vector<circuit::detail::StructuralWiring::PinPtr> terms;
            for (size_t i = 0; i < values_.size(); ++i)
                if ((((values_[i] >> bit) & 1U) != 0) != fallback)
                    terms.push_back(matches->getOutputPin(patterns()[i].name));
            const auto prefix = "BIT_" + std::to_string(bit);
            const bool has_terms = !terms.empty();
            auto value = terms.empty() ? w.constant(prefix + "_CONSTANT", fallback)
                                       : reduceShared(prefix + "_OR", std::move(terms));
            if (has_terms && fallback) value = w.invert(prefix + "_DEFAULT_NOT", value);
            w.connect(value, join->getInputPin(prefix));
        }
        w.connect(join->getOutputPinDynamic("OUT"), getOutputPinDynamic("OUT"));
        w.finish();
    }
};

class RV32IDecodeControlSignals : public IOComponent {
public:
    explicit RV32IDecodeControlSignals(std::string name)
        : IOComponent(std::move(name), [](IOComponent* s) {
            s->addPin<32>("MATCH_LO", PinType::INPUT);
            s->addPin<8>("MATCH_HI", PinType::INPUT);
            controlOutputs(s);
        }) {}
    const char* getTypeName() const override { return "RV32IDecodeControlSignals"; }
    void buildInternals(ComponentBuilder& b) override {
        circuit::detail::StructuralWiring w(b);
        auto field = [&](const char* name, size_t width, uint8_t fallback,
                         const std::function<uint8_t(const ControlWord&)>& value) {
            std::vector<uint8_t> values;
            for (const auto& pattern : patterns()) values.push_back(value(pattern.control));
            auto child = b.addNewComponent<RV32IDecodeControlField>(name, width, fallback, values);
            for (auto pin : {"MATCH_LO", "MATCH_HI"})
                w.connect(getInputPinDynamic(pin), child->getInputPinDynamic(pin));
            w.connect(child->getOutputPinDynamic("OUT"), getOutputPinDynamic(name));
        };
        field("LEGAL", 1, 0, [](const ControlWord&) { return 1; });
        field("REG_WRITE", 1, 0, [](const ControlWord& c) { return c.reg_write; });
        field("MEM_READ", 1, 0, [](const ControlWord& c) { return c.mem_read; });
        field("MEM_WRITE", 1, 0, [](const ControlWord& c) { return c.mem_write; });
        field("LOAD_SIGN_EXTEND", 1, 0, [](const ControlWord& c) { return c.load_sign_extend; });
        field("USES_RS1", 1, 0, [](const ControlWord& c) { return c.uses_rs1; });
        field("USES_RS2", 1, 0, [](const ControlWord& c) { return c.uses_rs2; });
        field("HALT_REQUEST", 1, 0, [](const ControlWord& c) { return c.halt; });
        field("TRAP_REQUEST", 1, 1, [](const ControlWord& c) { return c.trap; });
        field("ALU_OP", 5, ALU32Op::ZERO, [](const ControlWord& c) { return c.alu_op; });
        field("ALU_A_SEL", 2, ALU_A_ZERO, [](const ControlWord& c) { return c.alu_a; });
        field("ALU_B_SEL", 2, ALU_B_ZERO, [](const ControlWord& c) { return c.alu_b; });
        field("WRITEBACK_SEL", 2, WB_NONE, [](const ControlWord& c) { return c.writeback; });
        field("MEM_SIZE", 2, MEM_WORD, [](const ControlWord& c) { return c.mem_size; });
        field("BRANCH_TYPE", 3, 0, [](const ControlWord& c) { return c.branch; });
        field("JUMP_TYPE", 2, 0, [](const ControlWord& c) { return c.jump; });
        field("DECODE_TRAP_CAUSE", 4, TRAP_ILLEGAL, [](const ControlWord& c) { return c.trap_cause; });
        field("IMMEDIATE_SELECT", 3, 0, [](const ControlWord& c) { return c.immediate; });
        w.finish();
    }
};

void defineDecodeControlPins(IOComponent* self) {
    self->addPin<32>("INSTRUCTION", PinType::INPUT);
    self->addPin<5>("RS1_ADDR", PinType::OUTPUT);
    self->addPin<5>("RS2_ADDR", PinType::OUTPUT);
    self->addPin<5>("RD_ADDR", PinType::OUTPUT);
    self->addPin<32>("IMM", PinType::OUTPUT);
    self->addPin<5>("ALU_OP", PinType::OUTPUT);
    self->addPin<2>("ALU_A_SEL", PinType::OUTPUT);
    self->addPin<2>("ALU_B_SEL", PinType::OUTPUT);
    self->addPin("LEGAL", PinType::OUTPUT);
    self->addPin("REG_WRITE", PinType::OUTPUT);
    self->addPin("MEM_READ", PinType::OUTPUT);
    self->addPin("MEM_WRITE", PinType::OUTPUT);
    self->addPin<2>("WRITEBACK_SEL", PinType::OUTPUT);
    self->addPin<2>("MEM_SIZE", PinType::OUTPUT);
    self->addPin("LOAD_SIGN_EXTEND", PinType::OUTPUT);
    self->addPin("USES_RS1", PinType::OUTPUT);
    self->addPin("USES_RS2", PinType::OUTPUT);
    self->addPin<3>("BRANCH_TYPE", PinType::OUTPUT);
    self->addPin<2>("JUMP_TYPE", PinType::OUTPUT);
    self->addPin("HALT_REQUEST", PinType::OUTPUT);
    self->addPin("TRAP_REQUEST", PinType::OUTPUT);
    self->addPin<4>("DECODE_TRAP_CAUSE", PinType::OUTPUT);
}
}

namespace circuit::families {
const ComponentFamily RV32IDecodeControl{
    "rv32i.decode-control", "RV32IDecodeControlUnit", defineDecodeControlPins,
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::RV32IDecodeControlUnit>(context, name);
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::RV32IDecodeControlBehavioral>(context, name);
    }};
}

RV32IDecodeControlUnit::RV32IDecodeControlUnit(std::string name)
    : IOComponent(std::move(name), circuit::families::RV32IDecodeControl.pinInitializer()) {}

void RV32IDecodeControlUnit::buildInternals(ComponentBuilder& b) {
    circuit::detail::StructuralWiring w(b);
    auto fields = b.addNewComponent<Rewire>("FIELDS_AND_IMMEDIATES",
        std::vector<Rewire::WireSpec>{{"INSTRUCTION", 32}},
        std::vector<Rewire::WireSpec>{{"RS1",5},{"RS2",5},{"RD",5},
            {"IMM_I",32},{"IMM_S",32},{"IMM_B",32},{"IMM_U",32},{"IMM_J",32}},
        decodeMappings(), Rewire::UnmappedBitValue::LOW);
    auto recognition = b.addNewComponent<RV32IInstructionRecognition>("RECOGNITION");
    auto controls = b.addNewComponent<RV32IDecodeControlSignals>("CONTROL_SIGNALS");
    auto immediate = b.addNewComponent<Mux8to1_32bit>("IMMEDIATE_MUX");
    auto zero = b.addNewComponent<ConstantValue<32>>("CONST_ZERO32", 0);
    w.connect(getInputPin<32>("INSTRUCTION"), fields->getInputPin<32>("INSTRUCTION"));
    w.connect(getInputPin<32>("INSTRUCTION"), recognition->getInputPin<32>("INSTRUCTION"));
    for (auto pin : {"MATCH_LO", "MATCH_HI"})
        w.connect(recognition->getOutputPinDynamic(pin), controls->getInputPinDynamic(pin));
    for (const auto& [name, pin] : controls->getAllOutputPins())
        w.connect(pin, name == "IMMEDIATE_SELECT" ? immediate->getInputPinDynamic("SEL") : getOutputPinDynamic(name));
    for (const auto* name : {"RS1", "RS2", "RD"})
        w.connect(fields->getOutputPin<5>(name), getOutputPin<5>(std::string(name) + "_ADDR"));
    for (const auto& [name, slot] : std::vector<std::pair<std::string,size_t>>{
            {"IMM_I",IMM_I},{"IMM_S",IMM_S},{"IMM_B",IMM_B},{"IMM_U",IMM_U},{"IMM_J",IMM_J}})
        w.connect(fields->getOutputPin<32>(name), immediate->getInputPin<32>("IN" + std::to_string(slot)));
    for (const auto* pin : {"IN0","IN6","IN7"}) w.connect(zero->getOutputPin<32>("OUT"), immediate->getInputPin<32>(pin));
    w.connect(immediate->getOutputPin<32>("OUT"), getOutputPin<32>("IMM"));
    w.finish();
}
