#include "tests/RV32IBlockStandaloneTests.hpp"

#include "basic/Wire.hpp"
#include "components/Component.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/IOComponent.hpp"
#include "modules/composite/ALU32.hpp"
#include "modules/rv32i/BehavioralRV32IControlFlowUnit.hpp"
#include "modules/rv32i/BehavioralRV32IDecodeControlUnit.hpp"
#include "modules/rv32i/BehavioralRV32IExecutionControlStatusUnit.hpp"
#include "modules/rv32i/RV32IComponentEncoding.hpp"
#include "modules/rv32i/RV32IControlFlowUnit.hpp"
#include "modules/rv32i/RV32IDecodeControlUnit.hpp"
#include "modules/rv32i/RV32IExecutionControlStatusUnit.hpp"
#include "rv32i/RV32IControl.hpp"
#include "rv32i/RV32IDecoder.hpp"
#include "rv32i/RV32IState.hpp"
#include "simulator/Event.hpp"
#include <array>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
template<size_t Width>
void drive(Simulator& simulator,
           size_t time,
           const std::shared_ptr<Wire<Width>>& wire,
           uint64_t value) {
    simulator.scheduleEvent(std::make_shared<WireUpdateEvent<Width>>(time, wire, value));
}

void driveBit(Simulator& simulator,
              size_t time,
              const std::shared_ptr<Wire<>>& wire,
              bool value) {
    simulator.scheduleEvent(std::make_shared<WireUpdateEvent<>>(
        time, wire, value ? LogicValue::HIGH : LogicValue::LOW));
}

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::shared_ptr<IOComponent> ioRoot(const std::shared_ptr<Component>& root,
                                    const std::string& test_name) {
    auto io = std::dynamic_pointer_cast<IOComponent>(root);
    require(static_cast<bool>(io), test_name + " requires an IOComponent root");
    return io;
}

LogicValue logic(bool value) {
    return value ? LogicValue::HIGH : LogicValue::LOW;
}

struct ControlFlowExpected {
    size_t time;
    uint32_t pc;
    uint32_t plus4;
    uint32_t next;
    bool branch;
    bool pc_misaligned;
    bool target_misaligned;
    const char* label;
};

const std::vector<ControlFlowExpected>& controlFlowExpected() {
    static const std::vector<ControlFlowExpected> values{
        {900, 0, 4, 4, false, false, false, "reset"},
        {1900, 0, 4, 4, false, false, false, "sequential candidate"},
        {2900, 4, 8, 8, false, false, false, "PC+4 commit"},
        {3900, 12, 16, 20, true, false, false, "taken BEQ commit"},
        {4900, 12, 16, 0x102, false, false, true, "misaligned JALR candidate held"},
        {5900, 0, 4, 4, false, false, false, "reset recovery"},
    };
    return values;
}

uint32_t encodeR(uint8_t funct7, uint8_t rs2, uint8_t rs1,
                 uint8_t funct3, uint8_t rd) {
    return (static_cast<uint32_t>(funct7) << 25U)
         | (static_cast<uint32_t>(rs2) << 20U)
         | (static_cast<uint32_t>(rs1) << 15U)
         | (static_cast<uint32_t>(funct3) << 12U)
         | (static_cast<uint32_t>(rd) << 7U)
         | 0x33U;
}

uint32_t encodeI(int32_t immediate, uint8_t rs1, uint8_t funct3,
                 uint8_t rd, uint8_t opcode) {
    return ((static_cast<uint32_t>(immediate) & 0xFFFU) << 20U)
         | (static_cast<uint32_t>(rs1) << 15U)
         | (static_cast<uint32_t>(funct3) << 12U)
         | (static_cast<uint32_t>(rd) << 7U)
         | opcode;
}

uint32_t encodeS(int32_t immediate, uint8_t rs2, uint8_t rs1, uint8_t funct3) {
    const uint32_t bits = static_cast<uint32_t>(immediate) & 0xFFFU;
    return ((bits >> 5U) << 25U)
         | (static_cast<uint32_t>(rs2) << 20U)
         | (static_cast<uint32_t>(rs1) << 15U)
         | (static_cast<uint32_t>(funct3) << 12U)
         | ((bits & 0x1FU) << 7U)
         | 0x23U;
}

uint32_t encodeB(int32_t immediate, uint8_t rs2, uint8_t rs1, uint8_t funct3) {
    const uint32_t bits = static_cast<uint32_t>(immediate) & 0x1FFFU;
    return (((bits >> 12U) & 1U) << 31U)
         | (((bits >> 5U) & 0x3FU) << 25U)
         | (static_cast<uint32_t>(rs2) << 20U)
         | (static_cast<uint32_t>(rs1) << 15U)
         | (static_cast<uint32_t>(funct3) << 12U)
         | (((bits >> 1U) & 0xFU) << 8U)
         | (((bits >> 11U) & 1U) << 7U)
         | 0x63U;
}

uint32_t encodeJ(int32_t immediate, uint8_t rd) {
    const uint32_t bits = static_cast<uint32_t>(immediate) & 0x1FFFFFU;
    return (((bits >> 20U) & 1U) << 31U)
         | (((bits >> 1U) & 0x3FFU) << 21U)
         | (((bits >> 11U) & 1U) << 20U)
         | (((bits >> 12U) & 0xFFU) << 12U)
         | (static_cast<uint32_t>(rd) << 7U)
         | 0x6FU;
}

struct DecodeCase {
    uint32_t raw;
    const char* label;
};

const std::vector<DecodeCase>& standaloneDecodeCases() {
    static const std::vector<DecodeCase> values{
        {encodeR(0x00, 7, 6, 0, 5), "ADD x5,x6,x7"},
        {encodeI(-12, 6, 2, 5, 0x03), "LW x5,-12(x6)"},
        {encodeS(-20, 7, 6, 2), "SW x7,-20(x6)"},
        {encodeB(-16, 7, 6, 0), "BEQ x6,x7,-16"},
        {encodeJ(-2048, 5), "JAL x5,-2048"},
        {0x00000073U, "ECALL"},
        {0x00100073U, "EBREAK"},
        {0x00000000U, "illegal encoding"},
    };
    return values;
}

void checkDecodeBit(const std::shared_ptr<IOComponent>& component,
                    const char* pin,
                    bool expected,
                    const std::string& prefix) {
    require(component->getOutputPin(pin)->getValue() == logic(expected),
            prefix + pin);
}

void checkDecodeBus(const std::shared_ptr<IOComponent>& component,
                    const char* pin,
                    uint64_t expected,
                    const std::string& prefix) {
    require(component->getOutputPinDynamic(pin)->getValueAsUInt64() == expected,
            prefix + pin);
}

void checkDecodedContract(const std::shared_ptr<IOComponent>& component,
                          const DecodeCase& test_case) {
    const auto decoded = rv32i::RV32IDecoder::decode(test_case.raw);
    const auto control = rv32i::RV32IControl::fromDecoded(decoded);
    const std::string prefix = std::string(test_case.label) + ": ";
    checkDecodeBus(component, "RS1_ADDR", decoded.rs1, prefix);
    checkDecodeBus(component, "RS2_ADDR", decoded.rs2, prefix);
    checkDecodeBus(component, "RD_ADDR", decoded.rd, prefix);
    checkDecodeBus(component, "IMM", static_cast<uint32_t>(decoded.immediate), prefix);
    checkDecodeBus(component, "ALU_OP", control.alu_op, prefix);
    checkDecodeBus(component, "ALU_A_SEL", rv32i::component_encoding::aluSourceA(control.alu_a), prefix);
    checkDecodeBus(component, "ALU_B_SEL", rv32i::component_encoding::aluSourceB(control.alu_b), prefix);
    checkDecodeBit(component, "LEGAL", control.legal, prefix);
    checkDecodeBit(component, "REG_WRITE", control.reg_write, prefix);
    checkDecodeBit(component, "MEM_READ", control.mem_read, prefix);
    checkDecodeBit(component, "MEM_WRITE", control.mem_write, prefix);
    checkDecodeBus(component, "WRITEBACK_SEL", rv32i::component_encoding::writeback(control.writeback), prefix);
    checkDecodeBus(component, "MEM_SIZE", rv32i::component_encoding::memorySize(control.mem_size), prefix);
    checkDecodeBit(component, "LOAD_SIGN_EXTEND", control.load_sign_extend, prefix);
    checkDecodeBus(component, "BRANCH_TYPE", rv32i::component_encoding::branch(control.branch), prefix);
    checkDecodeBus(component, "JUMP_TYPE", rv32i::component_encoding::jump(control.jump), prefix);
    checkDecodeBit(component, "HALT_REQUEST", control.halt, prefix);
    checkDecodeBit(component, "TRAP_REQUEST", control.trap, prefix);
    checkDecodeBus(component, "DECODE_TRAP_CAUSE",
                   rv32i::component_encoding::decodeTrapCause(control.trap_cause), prefix);
}

struct StatusExpected {
    size_t time;
    bool pc_write;
    bool register_write;
    bool request;
    bool halted;
    bool trapped;
    uint8_t cause;
    bool misaligned;
    bool attempt;
    const char* label;
};

const std::vector<StatusExpected>& standaloneStatusExpected() {
    static const std::vector<StatusExpected> values{
        {900, false, false, false, false, false, 0, false, false, "reset"},
        {1900, true, true, false, false, false, 0, false, true, "normal completion"},
        {2900, false, false, true, false, false, 0, false, false, "data-memory wait"},
        {3900, false, false, true, false, false, 0, true, true, "misaligned load attempt"},
        {4900, false, false, false, false, true,
         static_cast<uint8_t>(rv32i::RV32IExecutionTrapCause::LoadAddressMisaligned),
         true, false, "load trap latched"},
        {5900, false, false, false, false, false, 0, false, false, "reset recovery"},
        {6900, false, false, false, false, false, 0, false, true, "ECALL attempt"},
        {7900, false, false, false, false, true,
         static_cast<uint8_t>(rv32i::RV32IExecutionTrapCause::EnvironmentCall),
         false, false, "ECALL trap latched"},
    };
    return values;
}
}

RV32IControlFlowUnitStandaloneTestBase::RV32IControlFlowUnitStandaloneTestBase(
    std::string test_name)
    : test_name_(std::move(test_name)) {}

std::string RV32IControlFlowUnitStandaloneTestBase::getTestName() const {
    return test_name_;
}

void RV32IControlFlowUnitStandaloneTestBase::setupCircuit() {
    root = createRootComponent();
    builder = std::make_unique<ComponentBuilder>(root);
    setInitialState();
}

void RV32IControlFlowUnitStandaloneTestBase::setInitialState() {
    auto component = ioRoot(root, getTestName());
    auto clk = builder->addNewWire("CLK_IN", nullptr, {component->getInputPin("CLK")});
    auto rst = builder->addNewWire("RST_IN", nullptr, {component->getInputPin("RST")});
    auto pc_write = builder->addNewWire("PC_WRITE_IN", nullptr, {component->getInputPin("PC_WRITE")});
    auto rs1 = builder->addNewWire<32>("RS1_VALUE_IN", nullptr, {component->getInputPin<32>("RS1_VALUE")});
    auto imm = builder->addNewWire<32>("IMM_IN", nullptr, {component->getInputPin<32>("IMM")});
    auto branch = builder->addNewWire<3>("BRANCH_TYPE_IN", nullptr, {component->getInputPin<3>("BRANCH_TYPE")});
    auto jump = builder->addNewWire<2>("JUMP_TYPE_IN", nullptr, {component->getInputPin<2>("JUMP_TYPE")});
    auto eq = builder->addNewWire("EQ_IN", nullptr, {component->getInputPin("EQ")});
    auto lt_signed = builder->addNewWire("LT_SIGNED_IN", nullptr, {component->getInputPin("LT_SIGNED")});
    auto lt_unsigned = builder->addNewWire("LT_UNSIGNED_IN", nullptr, {component->getInputPin("LT_UNSIGNED")});

    for (const char* pin : {"PC", "PC_PLUS_4", "NEXT_PC_CANDIDATE"}) {
        builder->addNewWireDynamic(std::string(pin) + "_OUT", 32,
                                   component->getOutputPinDynamic(pin), {});
    }
    for (const char* pin : {"BRANCH_TAKEN", "PC_MISALIGNED", "TARGET_MISALIGNED"}) {
        builder->addNewWire(std::string(pin) + "_OUT", component->getOutputPin(pin), {});
    }

    driveBit(*sim, 0, clk, false);
    driveBit(*sim, 0, rst, true);
    driveBit(*sim, 0, pc_write, false);
    drive<32>(*sim, 0, rs1, 0);
    drive<32>(*sim, 0, imm, 0);
    drive<3>(*sim, 0, branch, 0);
    drive<2>(*sim, 0, jump, 0);
    driveBit(*sim, 0, eq, false);
    driveBit(*sim, 0, lt_signed, false);
    driveBit(*sim, 0, lt_unsigned, false);
    driveBit(*sim, 1000, rst, false);

    driveBit(*sim, 2000, pc_write, true);
    driveBit(*sim, 2200, clk, true);
    driveBit(*sim, 2500, clk, false);

    drive<32>(*sim, 3000, imm, 8);
    drive<3>(*sim, 3000, branch, 1);
    driveBit(*sim, 3000, eq, true);
    driveBit(*sim, 3200, clk, true);
    driveBit(*sim, 3500, clk, false);

    driveBit(*sim, 4000, pc_write, false);
    drive<3>(*sim, 4000, branch, 0);
    drive<2>(*sim, 4000, jump, 2);
    drive<32>(*sim, 4000, rs1, 0x101);
    drive<32>(*sim, 4000, imm, 2);

    driveBit(*sim, 5000, rst, true);
    drive<2>(*sim, 5000, jump, 0);
    drive<32>(*sim, 5000, imm, 0);
}

void RV32IControlFlowUnitStandaloneTestBase::verifyResults() {
    auto component = ioRoot(root, getTestName());
    for (const auto& expected : controlFlowExpected()) {
        sim->setCircuitStateAtTime(expected.time);
        const std::string prefix = std::string(expected.label) + ": ";
        require(component->getOutputPin<32>("PC")->getValueAsUInt64() == expected.pc,
                prefix + "PC");
        require(component->getOutputPin<32>("PC_PLUS_4")->getValueAsUInt64() == expected.plus4,
                prefix + "PC_PLUS_4");
        require(component->getOutputPin<32>("NEXT_PC_CANDIDATE")->getValueAsUInt64() == expected.next,
                prefix + "NEXT_PC_CANDIDATE");
        require(component->getOutputPin("BRANCH_TAKEN")->getValue() == logic(expected.branch),
                prefix + "BRANCH_TAKEN");
        require(component->getOutputPin("PC_MISALIGNED")->getValue() == logic(expected.pc_misaligned),
                prefix + "PC_MISALIGNED");
        require(component->getOutputPin("TARGET_MISALIGNED")->getValue() == logic(expected.target_misaligned),
                prefix + "TARGET_MISALIGNED");
    }
}

size_t RV32IControlFlowUnitStandaloneTestBase::getRunDuration() const {
    return 6000;
}

std::vector<SimulationTest::SimulationCheckpoint>
RV32IControlFlowUnitStandaloneTestBase::getCheckpoints() const {
    std::vector<SimulationCheckpoint> checkpoints;
    size_t row = 0;
    for (const auto& expected : controlFlowExpected()) {
        checkpoints.push_back({expected.time, expected.label,
                               "single control-flow implementation", row++});
    }
    return checkpoints;
}

RV32IControlFlowUnitTest::RV32IControlFlowUnitTest()
    : RV32IControlFlowUnitStandaloneTestBase("RV32IControlFlowUnitTest") {}

std::shared_ptr<Component> RV32IControlFlowUnitTest::createRootComponent() const {
    return Component::create<RV32IControlFlowUnit>("RV32I_CONTROL_FLOW_ROOT");
}

BehavioralRV32IControlFlowUnitTest::BehavioralRV32IControlFlowUnitTest()
    : RV32IControlFlowUnitStandaloneTestBase("BehavioralRV32IControlFlowUnitTest") {}

std::shared_ptr<Component> BehavioralRV32IControlFlowUnitTest::createRootComponent() const {
    return Component::create<BehavioralRV32IControlFlowUnit>("BEHAVIORAL_RV32I_CONTROL_FLOW_ROOT");
}

RV32IDecodeControlUnitStandaloneTestBase::RV32IDecodeControlUnitStandaloneTestBase(
    std::string test_name)
    : test_name_(std::move(test_name)) {}

std::string RV32IDecodeControlUnitStandaloneTestBase::getTestName() const {
    return test_name_;
}

void RV32IDecodeControlUnitStandaloneTestBase::setupCircuit() {
    root = createRootComponent();
    builder = std::make_unique<ComponentBuilder>(root);
    setInitialState();
}

void RV32IDecodeControlUnitStandaloneTestBase::setInitialState() {
    auto component = ioRoot(root, getTestName());
    auto instruction = builder->addNewWire<32>(
        "INSTRUCTION_IN", nullptr, {component->getInputPin<32>("INSTRUCTION")});
    for (const auto& [pin, width] : std::array<std::pair<const char*, size_t>, 12>{{
             {"RS1_ADDR", 5}, {"RS2_ADDR", 5}, {"RD_ADDR", 5}, {"IMM", 32},
             {"ALU_OP", 5}, {"ALU_A_SEL", 2}, {"ALU_B_SEL", 2},
             {"WRITEBACK_SEL", 2}, {"MEM_SIZE", 2}, {"BRANCH_TYPE", 3},
             {"JUMP_TYPE", 2}, {"DECODE_TRAP_CAUSE", 4}}}) {
        builder->addNewWireDynamic(std::string(pin) + "_OUT", width,
                                   component->getOutputPinDynamic(pin), {});
    }
    for (const char* pin : {"LEGAL", "REG_WRITE", "MEM_READ", "MEM_WRITE",
                            "LOAD_SIGN_EXTEND", "HALT_REQUEST", "TRAP_REQUEST"}) {
        builder->addNewWire(std::string(pin) + "_OUT", component->getOutputPin(pin), {});
    }

    size_t row = 0;
    for (const auto& test_case : standaloneDecodeCases()) {
        drive<32>(*sim, row * 1000, instruction, test_case.raw);
        ++row;
    }
}

void RV32IDecodeControlUnitStandaloneTestBase::verifyResults() {
    auto component = ioRoot(root, getTestName());
    size_t row = 0;
    for (const auto& test_case : standaloneDecodeCases()) {
        sim->setCircuitStateAtTime(row * 1000 + 900);
        checkDecodedContract(component, test_case);
        ++row;
    }
}

size_t RV32IDecodeControlUnitStandaloneTestBase::getRunDuration() const {
    return standaloneDecodeCases().size() * 1000;
}

std::vector<SimulationTest::SimulationCheckpoint>
RV32IDecodeControlUnitStandaloneTestBase::getCheckpoints() const {
    std::vector<SimulationCheckpoint> checkpoints;
    size_t row = 0;
    for (const auto& test_case : standaloneDecodeCases()) {
        checkpoints.push_back({row * 1000 + 900, test_case.label,
                               "single decode/control implementation", row});
        ++row;
    }
    return checkpoints;
}

RV32IDecodeControlUnitTest::RV32IDecodeControlUnitTest()
    : RV32IDecodeControlUnitStandaloneTestBase("RV32IDecodeControlUnitTest") {}

std::shared_ptr<Component> RV32IDecodeControlUnitTest::createRootComponent() const {
    return Component::create<RV32IDecodeControlUnit>("RV32I_DECODE_CONTROL_ROOT");
}

BehavioralRV32IDecodeControlUnitTest::BehavioralRV32IDecodeControlUnitTest()
    : RV32IDecodeControlUnitStandaloneTestBase("BehavioralRV32IDecodeControlUnitTest") {}

std::shared_ptr<Component> BehavioralRV32IDecodeControlUnitTest::createRootComponent() const {
    return Component::create<BehavioralRV32IDecodeControlUnit>(
        "BEHAVIORAL_RV32I_DECODE_CONTROL_ROOT");
}

RV32IExecutionControlStatusUnitStandaloneTestBase::
RV32IExecutionControlStatusUnitStandaloneTestBase(std::string test_name)
    : test_name_(std::move(test_name)) {}

std::string RV32IExecutionControlStatusUnitStandaloneTestBase::getTestName() const {
    return test_name_;
}

void RV32IExecutionControlStatusUnitStandaloneTestBase::setupCircuit() {
    root = createRootComponent();
    builder = std::make_unique<ComponentBuilder>(root);
    setInitialState();
}

void RV32IExecutionControlStatusUnitStandaloneTestBase::setInitialState() {
    auto component = ioRoot(root, getTestName());
    auto bitInput = [&](const char* pin) {
        return builder->addNewWire(std::string(pin) + "_IN", nullptr,
                                   {component->getInputPin(pin)});
    };
    auto clk = bitInput("CLK");
    auto rst = bitInput("RST");
    auto enable = bitInput("ENABLE");
    auto legal = bitInput("LEGAL");
    auto reg_write = bitInput("REG_WRITE");
    auto mem_read = bitInput("MEM_READ");
    auto mem_write = bitInput("MEM_WRITE");
    auto halt_request = bitInput("HALT_REQUEST");
    auto trap_request = bitInput("TRAP_REQUEST");
    auto pc_misaligned = bitInput("PC_MISALIGNED");
    auto target_misaligned = bitInput("TARGET_MISALIGNED");
    auto imem_ready = bitInput("IMEM_READY");
    auto imem_fault = bitInput("IMEM_FAULT");
    auto dmem_ready = bitInput("DMEM_READY");
    auto dmem_fault = bitInput("DMEM_FAULT");
    auto decode_cause = builder->addNewWire<4>(
        "DECODE_TRAP_CAUSE_IN", nullptr,
        {component->getInputPin<4>("DECODE_TRAP_CAUSE")});
    auto address = builder->addNewWire<32>(
        "ALU_ADDRESS_IN", nullptr, {component->getInputPin<32>("ALU_ADDRESS")});
    auto mem_size = builder->addNewWire<2>(
        "MEM_SIZE_IN", nullptr, {component->getInputPin<2>("MEM_SIZE")});

    for (const char* pin : {"PC_WRITE", "REGISTER_WRITE", "MEMORY_REQUEST_ACTIVE",
                            "HALTED", "TRAPPED", "DATA_ADDRESS_MISALIGNED",
                            "INSTRUCTION_ATTEMPT"}) {
        builder->addNewWire(std::string(pin) + "_OUT", component->getOutputPin(pin), {});
    }
    builder->addNewWire<4>("TRAP_CAUSE_OUT", component->getOutputPin<4>("TRAP_CAUSE"), {});

    auto normalInputs = [&](size_t time) {
        driveBit(*sim, time, enable, true);
        driveBit(*sim, time, legal, true);
        driveBit(*sim, time, reg_write, true);
        driveBit(*sim, time, mem_read, false);
        driveBit(*sim, time, mem_write, false);
        driveBit(*sim, time, halt_request, false);
        driveBit(*sim, time, trap_request, false);
        drive<4>(*sim, time, decode_cause, 0);
        drive<32>(*sim, time, address, 0);
        drive<2>(*sim, time, mem_size, 2);
        driveBit(*sim, time, pc_misaligned, false);
        driveBit(*sim, time, target_misaligned, false);
        driveBit(*sim, time, imem_ready, true);
        driveBit(*sim, time, imem_fault, false);
        driveBit(*sim, time, dmem_ready, true);
        driveBit(*sim, time, dmem_fault, false);
    };

    driveBit(*sim, 0, clk, false);
    driveBit(*sim, 0, rst, true);
    normalInputs(0);
    driveBit(*sim, 1000, rst, false);

    driveBit(*sim, 2000, mem_read, true);
    driveBit(*sim, 2000, dmem_ready, false);

    driveBit(*sim, 3000, dmem_ready, true);
    drive<32>(*sim, 3000, address, 1);
    drive<2>(*sim, 3000, mem_size, 1);
    driveBit(*sim, 4200, clk, true);
    driveBit(*sim, 4500, clk, false);

    driveBit(*sim, 5000, rst, true);
    normalInputs(5000);
    driveBit(*sim, 6000, rst, false);
    driveBit(*sim, 6000, trap_request, true);
    drive<4>(*sim, 6000, decode_cause, 2);
    driveBit(*sim, 7200, clk, true);
    driveBit(*sim, 7500, clk, false);
}

void RV32IExecutionControlStatusUnitStandaloneTestBase::verifyResults() {
    auto component = ioRoot(root, getTestName());
    for (const auto& expected : standaloneStatusExpected()) {
        sim->setCircuitStateAtTime(expected.time);
        const std::string prefix = std::string(expected.label) + ": ";
        for (const auto& [pin, value] : std::array<std::pair<const char*, bool>, 7>{{
                 {"PC_WRITE", expected.pc_write},
                 {"REGISTER_WRITE", expected.register_write},
                 {"MEMORY_REQUEST_ACTIVE", expected.request},
                 {"HALTED", expected.halted},
                 {"TRAPPED", expected.trapped},
                 {"DATA_ADDRESS_MISALIGNED", expected.misaligned},
                 {"INSTRUCTION_ATTEMPT", expected.attempt}}}) {
            require(component->getOutputPin(pin)->getValue() == logic(value), prefix + pin);
        }
        require(component->getOutputPin<4>("TRAP_CAUSE")->getValueAsUInt64()
                    == expected.cause,
                prefix + "TRAP_CAUSE");
    }
}

size_t RV32IExecutionControlStatusUnitStandaloneTestBase::getRunDuration() const {
    return 8200;
}

std::vector<SimulationTest::SimulationCheckpoint>
RV32IExecutionControlStatusUnitStandaloneTestBase::getCheckpoints() const {
    std::vector<SimulationCheckpoint> checkpoints;
    size_t row = 0;
    for (const auto& expected : standaloneStatusExpected()) {
        checkpoints.push_back({expected.time, expected.label,
                               "single execution/status implementation", row++});
    }
    return checkpoints;
}

RV32IExecutionControlStatusUnitTest::RV32IExecutionControlStatusUnitTest()
    : RV32IExecutionControlStatusUnitStandaloneTestBase(
          "RV32IExecutionControlStatusUnitTest") {}

std::shared_ptr<Component>
RV32IExecutionControlStatusUnitTest::createRootComponent() const {
    return Component::create<RV32IExecutionControlStatusUnit>(
        "RV32I_EXECUTION_CONTROL_STATUS_ROOT");
}

BehavioralRV32IExecutionControlStatusUnitTest::
BehavioralRV32IExecutionControlStatusUnitTest()
    : RV32IExecutionControlStatusUnitStandaloneTestBase(
          "BehavioralRV32IExecutionControlStatusUnitTest") {}

std::shared_ptr<Component>
BehavioralRV32IExecutionControlStatusUnitTest::createRootComponent() const {
    return Component::create<BehavioralRV32IExecutionControlStatusUnit>(
        "BEHAVIORAL_RV32I_EXECUTION_CONTROL_STATUS_ROOT");
}
