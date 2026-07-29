#include "modules/rv32i/RV32IDecodeControlUnit.hpp"
#include "modules/rv32i/RV32IDecodeControlDirect.hpp"

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
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
    return {name, 0x0000707FU, (static_cast<uint32_t>(funct3) << 12U) | 0x23U, control};
}

InstructionPattern branch(const char* name, uint8_t funct3, uint8_t branch_type) {
    ControlWord control;
    control.alu_op = ALU32Op::SUB;
    control.alu_a = ALU_A_RS1;
    control.alu_b = ALU_B_RS2;
    control.branch = branch_type;
    control.immediate = IMM_B;
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
    self->addPin<3>("BRANCH_TYPE", PinType::OUTPUT);
    self->addPin<2>("JUMP_TYPE", PinType::OUTPUT);
    self->addPin("HALT_REQUEST", PinType::OUTPUT);
    self->addPin("TRAP_REQUEST", PinType::OUTPUT);
    self->addPin<4>("DECODE_TRAP_CAUSE", PinType::OUTPUT);
}
}

namespace circuit::families {
const ComponentFamily RV32IDecodeControl{
    "rv32i.decode-control",
    "RV32IDecodeControlUnit",
    defineDecodeControlPins,
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::RV32IDecodeControlUnit>(
            context, name);
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<
            ::RV32IDecodeControlDirect>(context, name);
    }};
}

RV32IDecodeControlUnit::RV32IDecodeControlUnit(std::string name)
    : IOComponent(std::move(name),
                  circuit::families::RV32IDecodeControl.pinInitializer()) {}

void RV32IDecodeControlUnit::buildInternals(ComponentBuilder& builder) {
    builder.addNewComponent<Rewire>(
        "FIELDS_AND_IMMEDIATES",
        std::vector<Rewire::WireSpec>{{"INSTRUCTION", 32}},
        std::vector<Rewire::WireSpec>{{"RS1", 5}, {"RS2", 5}, {"RD", 5},
                                      {"IMM_I", 32}, {"IMM_S", 32}, {"IMM_B", 32},
                                      {"IMM_U", 32}, {"IMM_J", 32}},
        decodeMappings(),
        Rewire::UnmappedBitValue::LOW);
    builder.addNewComponent<Mux8to1_32bit>("IMMEDIATE_MUX");
    builder.addNewComponent<BitJoiner<3>>("IMMEDIATE_SELECT_JOIN");
    builder.addNewComponent<ConstantValue<1>>("CONST_LOW", 0);
    builder.addNewComponent<ConstantValue<1>>("CONST_HIGH", 1);
    builder.addNewComponent<ConstantValue<32>>("CONST_ZERO32", 0);

    const auto& instruction_patterns = patterns();
    std::vector<std::vector<std::shared_ptr<Pin<>>>> match_sinks(instruction_patterns.size());
    std::vector<std::shared_ptr<Pin<32>>> instruction_sinks{
        builder.getInputPin<Rewire, 32>("FIELDS_AND_IMMEDIATES", "INSTRUCTION")};

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
    std::vector<std::shared_ptr<Pin<>>> recognition_outputs(
        instruction_patterns.size());

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
        for (size_t term = 1; term < terms.size(); ++term) {
            const auto gate =
                "RECOGNIZE_" + pattern.name + "_AND_"
                + std::to_string(term);
            builder.addNewComponent<ANDGate>(gate);
            if (term == 1) {
                predicates[terms[0]].sinks.push_back(
                    builder.getInputPin<ANDGate>(gate, "A"));
            } else {
                builder.addNewWire(
                    previous_gate + "_to_" + gate,
                    builder.getOutputPin<ANDGate>(
                        previous_gate, "OUT"),
                    {builder.getInputPin<ANDGate>(gate, "A")});
            }
            predicates[terms[term]].sinks.push_back(
                builder.getInputPin<ANDGate>(gate, "B"));
            previous_gate = gate;
        }
        recognition_outputs[index] =
            builder.getOutputPin<ANDGate>(previous_gate, "OUT");
    }
    builder.addNewWire<32>(
        "INSTRUCTION_fanout",
        getInputPin<32>("INSTRUCTION"),
        instruction_sinks);

    builder.addNewWire<5>("RS1_to_output", builder.getOutputPin<Rewire, 5>("FIELDS_AND_IMMEDIATES", "RS1"), {getOutputPin<5>("RS1_ADDR")});
    builder.addNewWire<5>("RS2_to_output", builder.getOutputPin<Rewire, 5>("FIELDS_AND_IMMEDIATES", "RS2"), {getOutputPin<5>("RS2_ADDR")});
    builder.addNewWire<5>("RD_to_output", builder.getOutputPin<Rewire, 5>("FIELDS_AND_IMMEDIATES", "RD"), {getOutputPin<5>("RD_ADDR")});

    for (const auto& [name, mux_input] : std::vector<std::pair<std::string, size_t>>{
             {"IMM_I", IMM_I}, {"IMM_S", IMM_S}, {"IMM_B", IMM_B}, {"IMM_U", IMM_U}, {"IMM_J", IMM_J}}) {
        builder.addNewWire<32>(
            name + "_to_mux",
            builder.getOutputPin<Rewire, 32>("FIELDS_AND_IMMEDIATES", name),
            {builder.getInputPin<Mux8to1_32bit, 32>("IMMEDIATE_MUX", "IN" + std::to_string(mux_input))});
    }
    builder.addNewWire<32>(
        "ZERO_to_immediate_mux",
        builder.getOutputPin<ConstantValue<32>, 32>("CONST_ZERO32", "OUT"),
        {builder.getInputPin<Mux8to1_32bit, 32>("IMMEDIATE_MUX", "IN0"),
         builder.getInputPin<Mux8to1_32bit, 32>("IMMEDIATE_MUX", "IN6"),
         builder.getInputPin<Mux8to1_32bit, 32>("IMMEDIATE_MUX", "IN7")});
    builder.addNewWire<3>(
        "immediate_select_to_mux",
        builder.getOutputPin<BitJoiner<3>, 3>("IMMEDIATE_SELECT_JOIN", "OUT"),
        {builder.getInputPin<Mux8to1_32bit, 3>("IMMEDIATE_MUX", "SEL")});
    builder.addNewWire<32>(
        "immediate_to_output",
        builder.getOutputPin<Mux8to1_32bit, 32>("IMMEDIATE_MUX", "OUT"),
        {getOutputPin<32>("IMM")});

    std::vector<std::shared_ptr<Pin<>>> low_sinks;
    std::vector<std::shared_ptr<Pin<>>> high_sinks;

    auto connectEncodedBit = [&](const std::string& signal_name,
                                 size_t bit_index,
                                 bool default_value,
                                 const std::function<uint8_t(const ControlWord&)>& value,
                                 const std::shared_ptr<Pin<>>& destination) {
        std::vector<size_t> differing_matches;
        for (size_t index = 0; index < instruction_patterns.size(); ++index) {
            const bool bit_value = ((value(instruction_patterns[index].control) >> bit_index) & 1U) != 0;
            if (bit_value != default_value) {
                differing_matches.push_back(index);
            }
        }

        if (differing_matches.empty()) {
            (default_value ? high_sinks : low_sinks).push_back(destination);
            return;
        }

        std::shared_ptr<Pin<>> difference_output;
        if (differing_matches.size() == 1) {
            if (!default_value) {
                match_sinks[differing_matches.front()].push_back(destination);
                return;
            }
            const auto not_name = signal_name + "_BIT_" + std::to_string(bit_index) + "_DEFAULT_NOT";
            builder.addNewComponent<NOTGate>(not_name);
            match_sinks[differing_matches.front()].push_back(builder.getInputPin<NOTGate>(not_name, "IN"));
            builder.addNewWire(not_name + "_to_destination", builder.getOutputPin<NOTGate>(not_name, "OUT"), {destination});
            return;
        }

        const auto base = signal_name + "_BIT_" + std::to_string(bit_index) + "_OR_";
        builder.addNewComponent<ORGate>(base + "1");
        match_sinks[differing_matches[0]].push_back(builder.getInputPin<ORGate>(base + "1", "A"));
        match_sinks[differing_matches[1]].push_back(builder.getInputPin<ORGate>(base + "1", "B"));
        std::string previous = base + "1";

        for (size_t term = 2; term < differing_matches.size(); ++term) {
            const auto gate = base + std::to_string(term);
            builder.addNewComponent<ORGate>(gate);
            builder.addNewWire(
                previous + "_to_" + gate,
                builder.getOutputPin<ORGate>(previous, "OUT"),
                {builder.getInputPin<ORGate>(gate, "A")});
            match_sinks[differing_matches[term]].push_back(builder.getInputPin<ORGate>(gate, "B"));
            previous = gate;
        }
        difference_output = builder.getOutputPin<ORGate>(previous, "OUT");

        if (!default_value) {
            builder.addNewWire(signal_name + "_BIT_" + std::to_string(bit_index) + "_to_destination", difference_output, {destination});
            return;
        }

        const auto not_name = signal_name + "_BIT_" + std::to_string(bit_index) + "_DEFAULT_NOT";
        builder.addNewComponent<NOTGate>(not_name);
        builder.addNewWire(not_name + "_input", difference_output, {builder.getInputPin<NOTGate>(not_name, "IN")});
        builder.addNewWire(not_name + "_to_destination", builder.getOutputPin<NOTGate>(not_name, "OUT"), {destination});
    };

    auto connectSingle = [&](const std::string& name,
                             bool default_value,
                             const std::function<bool(const ControlWord&)>& getter,
                             const std::shared_ptr<Pin<>>& destination) {
        connectEncodedBit(name, 0, default_value,
                          [getter](const ControlWord& control) { return static_cast<uint8_t>(getter(control)); },
                          destination);
    };

    auto connectBus = [&]<size_t Width>(const std::string& name,
                                       uint8_t default_value,
                                       const std::function<uint8_t(const ControlWord&)>& getter,
                                       const std::shared_ptr<Pin<Width>>& output) {
        const auto join_name = name + "_JOIN";
        builder.addNewComponent<BitJoiner<Width>>(join_name);
        for (size_t bit = 0; bit < Width; ++bit) {
            connectEncodedBit(name, bit, ((default_value >> bit) & 1U) != 0, getter,
                              builder.getInputPin<BitJoiner<Width>>(join_name, "IN_" + std::to_string(bit)));
        }
        builder.addNewWire<Width>(
            name + "_bus_to_output",
            builder.getOutputPin<BitJoiner<Width>, Width>(join_name, "OUT"),
            {output});
    };

    connectSingle("LEGAL", false, [](const ControlWord&) { return true; }, getOutputPin("LEGAL"));
    connectSingle("REG_WRITE", false, [](const ControlWord& c) { return c.reg_write; }, getOutputPin("REG_WRITE"));
    connectSingle("MEM_READ", false, [](const ControlWord& c) { return c.mem_read; }, getOutputPin("MEM_READ"));
    connectSingle("MEM_WRITE", false, [](const ControlWord& c) { return c.mem_write; }, getOutputPin("MEM_WRITE"));
    connectSingle("LOAD_SIGN_EXTEND", false, [](const ControlWord& c) { return c.load_sign_extend; }, getOutputPin("LOAD_SIGN_EXTEND"));
    connectSingle("HALT_REQUEST", false, [](const ControlWord& c) { return c.halt; }, getOutputPin("HALT_REQUEST"));
    connectSingle("TRAP_REQUEST", true, [](const ControlWord& c) { return c.trap; }, getOutputPin("TRAP_REQUEST"));

    connectBus.template operator()<5>("ALU_OP", ALU32Op::ZERO, [](const ControlWord& c) { return c.alu_op; }, getOutputPin<5>("ALU_OP"));
    connectBus.template operator()<2>("ALU_A_SEL", ALU_A_ZERO, [](const ControlWord& c) { return c.alu_a; }, getOutputPin<2>("ALU_A_SEL"));
    connectBus.template operator()<2>("ALU_B_SEL", ALU_B_ZERO, [](const ControlWord& c) { return c.alu_b; }, getOutputPin<2>("ALU_B_SEL"));
    connectBus.template operator()<2>("WRITEBACK_SEL", WB_NONE, [](const ControlWord& c) { return c.writeback; }, getOutputPin<2>("WRITEBACK_SEL"));
    connectBus.template operator()<2>("MEM_SIZE", MEM_WORD, [](const ControlWord& c) { return c.mem_size; }, getOutputPin<2>("MEM_SIZE"));
    connectBus.template operator()<3>("BRANCH_TYPE", 0, [](const ControlWord& c) { return c.branch; }, getOutputPin<3>("BRANCH_TYPE"));
    connectBus.template operator()<2>("JUMP_TYPE", 0, [](const ControlWord& c) { return c.jump; }, getOutputPin<2>("JUMP_TYPE"));
    connectBus.template operator()<4>("DECODE_TRAP_CAUSE", TRAP_ILLEGAL,
                                      [](const ControlWord& c) { return c.trap_cause; },
                                      getOutputPin<4>("DECODE_TRAP_CAUSE"));

    for (size_t bit = 0; bit < 3; ++bit) {
        connectEncodedBit("IMMEDIATE_SELECT", bit, false,
                          [](const ControlWord& c) { return c.immediate; },
                          builder.getInputPin<BitJoiner<3>>("IMMEDIATE_SELECT_JOIN", "IN_" + std::to_string(bit)));
    }

    for (size_t index = 0; index < instruction_patterns.size(); ++index) {
        if (direct_predicates[index] != NoPredicate) {
            auto& sinks = predicates[direct_predicates[index]].sinks;
            sinks.insert(
                sinks.end(),
                match_sinks[index].begin(),
                match_sinks[index].end());
            continue;
        }
        builder.addNewWire(
            "MATCH_" + instruction_patterns[index].name + "_fanout",
            recognition_outputs[index],
            match_sinks[index]);
    }
    for (const auto& predicate : predicates) {
        builder.addNewWire(
            predicate.name + "_fanout",
            builder.getOutputPin<RV32IBitPatternMatcher>(
                predicate.name, "MATCH"),
            predicate.sinks);
    }
    builder.addNewWire(
        "CONST_LOW_fanout",
        builder.getOutputPin<ConstantValue<1>>("CONST_LOW", "OUT"),
        low_sinks);
    builder.addNewWire(
        "CONST_HIGH_fanout",
        builder.getOutputPin<ConstantValue<1>>("CONST_HIGH", "OUT"),
        high_sinks);
}
