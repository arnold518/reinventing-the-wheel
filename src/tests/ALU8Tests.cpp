#include "tests/ALU8Tests.hpp"
#include "tests/ArithmeticLogicTests.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/composite/ALU8.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <iostream>
#include <random>
#include <stdexcept>

namespace {
struct Expected { unsigned out = 0; bool carry = false, overflow = false, invalid = false; };
int signed8(unsigned x) { return x < 128 ? int(x) : int(x) - 256; }
bool overflow8(int x) { return x < -128 || x > 127; }

// Independent integer oracle, not a simulator implementation or gate evaluator.
Expected addsubExpected(unsigned a, unsigned b, bool sub, bool cin) {
    const int unsigned_result = sub ? int(a)-int(b)-!cin : int(a+b)+cin;
    const int signed_result = sub ? signed8(a)-signed8(b)-!cin : signed8(a)+signed8(b)+cin;
    return {unsigned(unsigned_result) & 255u,
            sub ? unsigned_result >= 0 : unsigned_result > 255, overflow8(signed_result)};
}
Expected aluExpected(unsigned a, unsigned b, unsigned op, bool cin) {
    using namespace ALU8Op;
    const unsigned n = b & 7;
    Expected r;
    switch (op) {
    case ADD: return addsubExpected(a,b,false,false);
    case ADC: return addsubExpected(a,b,false,cin);
    case SUB: return addsubExpected(a,b,true,true);
    case SBC: return addsubExpected(a,b,true,cin);
    case INC: return addsubExpected(a,0,false,true);
    case DEC: return addsubExpected(a,1,true,true);
    case NEG: return addsubExpected(0,a,true,true);
    case AND: r.out = a & b; break;
    case OR: r.out = a | b; break;
    case XOR: r.out = a ^ b; break;
    case NOT: r.out = (~a) & 255; break;
    case NAND: r.out = (~(a & b)) & 255; break;
    case NOR: r.out = (~(a | b)) & 255; break;
    case XNOR: r.out = (~(a ^ b)) & 255; break;
    case SLL: case SRL: case SRA: case ROL: case ROR: {
        r.out = a;
        r.carry = cin;
        // Repeated one-bit operations form a different algorithm from the barrel network.
        for (unsigned i = 0; i < n; ++i) {
            if (op == SLL || op == ROL) {
                r.carry = (r.out & 128) != 0;
                r.out = ((r.out << 1) | (op == ROL && r.carry)) & 255;
            } else {
                r.carry = (r.out & 1) != 0;
                const unsigned fill = op == SRA ? r.out & 128 : (op == ROR && r.carry ? 128 : 0);
                r.out = (r.out >> 1) | fill;
            }
        }
        break;
    }
    case EQ: r.out = a == b; break;
    case SLTU: r.out = a < b; break;
    case SLT: r.out = signed8(a) < signed8(b); break;
    case MINU: r.out = std::min(a,b); break;
    case MAXU: r.out = std::max(a,b); break;
    case MINS: r.out = signed8(a) < signed8(b) ? a : b; break;
    case MAXS: r.out = signed8(a) < signed8(b) ? b : a; break;
    case PASS_A: r.out = a; break;
    case PASS_B: r.out = b; break;
    case CLEAR: break;
    default: r.invalid = true; break;
    }
    return r;
}
TestRow aluRowNew(unsigned a, unsigned b, unsigned op, bool cin) {
    auto r = aluExpected(a,b,op,cin);
    return {{{"A", bits(a)}, {"B", bits(b)}, {"OP", bits(op)}, {"C_IN", bit(cin)}},
        {{"OUT", bits(r.out)}, {"CARRY", bit(r.carry)}, {"OVERFLOW", bit(r.overflow)},
         {"ZERO", bit(r.out == 0)}, {"NEGATIVE", bit((r.out & 128) != 0)}, {"INVALID_OP", bit(r.invalid)}}};
}
TestRow addsubRow(unsigned a, unsigned b, bool sub, bool cin) {
    auto r = addsubExpected(a,b,sub,cin);
    return {{{"A", bits(a)}, {"B", bits(b)}, {"SUB", bit(sub)}, {"C_IN", bit(cin)}},
        {{"OUT", bits(r.out)}, {"CARRY_OUT", bit(r.carry)}, {"OVERFLOW", bit(r.overflow)}}};
}
std::vector<TestRow> arithmeticRows() {
    std::vector<TestRow> rows;
    for (unsigned op=0; op<8; ++op) for (auto a : {0u, 1u, 127u, 128u, 255u}) for (bool cin : {false,true}) {
        auto row = aluRowNew(a,1,op,cin);
        row.outputs.erase("ZERO"); row.outputs.erase("NEGATIVE");
        rows.push_back(std::move(row));
    }
    return rows;
}
std::vector<TestRow> logicRows() {
    std::vector<TestRow> rows;
    for (unsigned op=0; op<8; ++op) for (auto a : {0u, 0x55u, 0x80u, 255u}) {
        auto row = aluRowNew(a,0x0F,op+8,false);
        rows.push_back({{{"A",bits(a)},{"B",bits(0x0F)},{"OP",bits(op)}}, {{"OUT",row.outputs.at("OUT")}}});
    }
    return rows;
}
std::vector<TestRow> shiftRows() {
    std::vector<TestRow> rows;
    for (unsigned op=0; op<8; ++op) for (unsigned b : {0u,1u,3u,7u,8u,255u}) for (bool cin : {false,true}) {
        auto r = op < 5 ? aluExpected(0x81,b,op+16,cin) : Expected{};
        rows.push_back({{{"A",bits(0x81)},{"B",bits(b)},{"OP",bits(op)},{"C_IN",bit(cin)}},
            {{"OUT",bits(r.out)},{"CARRY",bit(r.carry)}}});
    }
    return rows;
}
std::vector<TestRow> comparisonRows() {
    std::vector<TestRow> rows;
    for (unsigned op=0; op<8; ++op) for (auto a : {0u,1u,127u,128u,255u}) {
        auto sub = addsubExpected(a,1,true,true);
        auto r = op < 7 ? aluExpected(a,1,21+op,false) : Expected{};
        rows.push_back({{{"A",bits(a)},{"B",bits(1)},{"DIFF",bits(sub.out)},
            {"CARRY_IN",bit(sub.carry)},{"OVERFLOW_IN",bit(sub.overflow)},{"OP",bits(op)}}, {{"OUT",bits(r.out)}}});
    }
    return rows;
}
std::vector<TestRow> flagsRows() {
    std::vector<TestRow> rows;
    for (unsigned controls=0; controls<32; ++controls) for (unsigned value : {0u,1u,128u,255u}) {
        bool arithmetic=controls&1, shift=controls&2, c=controls&4, v=controls&8, sc=controls&16;
        rows.push_back({{{"RESULT",bits(value)},{"ARITHMETIC",bit(arithmetic)},{"SHIFT",bit(shift)},
            {"ARITH_CARRY",bit(c)},{"ARITH_OVERFLOW",bit(v)},{"SHIFT_CARRY",bit(sc)}},
            {{"ZERO",bit(value==0)},{"NEGATIVE",bit(value&128)},
             {"CARRY",bit((arithmetic&&c)||(shift&&sc))},{"OVERFLOW",bit(arithmetic&&v)}}});
    }
    return rows;
}

// Reuse actual circuits and drain between inputs. Every oracle assertion is
// applied to both fidelities; history is disabled only for this large sweep.
class Harness {
public:
    explicit Harness(std::shared_ptr<IOComponent> component, const circuit::ComponentFamily* family = nullptr)
        : root(std::move(component)), builder(root) {
        if (family) {
            auto result = circuit::builtinComponentCatalog().createRoot(family->request("DIRECT"),
                circuit::withExactFidelity(circuit::canonicalDefaultProfile(), "DIRECT", circuit::Fidelity::Behavioral));
            peer = std::make_unique<Harness>(std::dynamic_pointer_cast<IOComponent>(result.root));
        }
        sim.setHistoryRecordingEnabled(false);
        for (const auto& [name, pin] : root->getAllInputPins())
            inputs[pin->getName()] = builder.addNewWireDynamic("INPUT_"+pin->getName(), pin->getWidth(), nullptr, {pin});
        for (const auto& [name, pin] : root->getAllOutputPins())
            builder.addNewWireDynamic("OUTPUT_"+pin->getName(), pin->getWidth(), pin, {});
        SimulationTest::scheduleInitialEventsForTree(root, sim, 0);
    }
    void check(const TestRow& row) {
        if (peer) peer->check(row);
        auto start = sim.getCurrentTime()+1;
        for (const auto& [name,value] : row.inputs) {
            auto event = makeTestWireUpdate(start, inputs.at(name), value);
            if (!event) throw std::runtime_error("Unsupported input width");
            sim.scheduleEvent(event);
        }
        auto result = sim.drainUntilIdle(start+10000, 1000000);
        if (!result.idle()) throw std::runtime_error("ALU8 circuit did not settle");
        max_delay = std::max(max_delay, result.final_time-start);
        for (const auto& [name, expected] : row.outputs) {
            auto pin = root->getOutputPinDynamic(name);
            auto values = expected.hasLogicVector() ? expected.values : circuit::test::logicBits(pin->getWidth(),
                expected.multi ? expected.numeric : expected.logic == LogicValue::HIGH);
            if (!expected.multi && expected.logic != LogicValue::LOW && expected.logic != LogicValue::HIGH)
                values = {expected.logic};
            if (pin->getValueAsVector() != values)
                throw std::runtime_error(root->getTypeName() + std::string(" mismatch: ") + name + " at " + testRowCheckpointDetail(row));
        }
        ++cases;
    }
    void report() const {
        std::cout << root->getTypeName() << ": " << cases << " cases, max observed settling delay " << max_delay
                  << ", processed events " << sim.getPerformanceCounters().processed_events << '\n';
        if (peer) { std::cout << "  behavioral: "; peer->report(); }
    }
    std::shared_ptr<IOComponent> root;
    Simulator sim;
    ComponentBuilder builder;
    std::map<std::string,std::shared_ptr<WireBase>> inputs;
    size_t cases=0, max_delay=0;
    std::unique_ptr<Harness> peer;
};
}

AddSub8Test::AddSub8Test() : TruthTableComponentTest("AddSub8Test","ADD_SUB8_ROOT", [] {
    std::vector<TestRow> rows;
    for (unsigned a : {0u,1u,127u,128u,255u}) for (unsigned b : {0u,1u,128u,255u})
        for (bool sub : {false,true}) for (bool cin : {false,true}) rows.push_back(addsubRow(a,b,sub,cin));
    return rows;
}()) {}
ArithmeticUnit8Test::ArithmeticUnit8Test() : TruthTableComponentTest("ArithmeticUnit8Test","ARITHMETIC8_ROOT",arithmeticRows()) {}
LogicUnit8Test::LogicUnit8Test() : TruthTableComponentTest("LogicUnit8Test","LOGIC8_ROOT",logicRows()) {}
BarrelShifter8Test::BarrelShifter8Test() : TruthTableComponentTest("BarrelShifter8Test","BARREL8_ROOT",shiftRows()) {}
CompareSelect8Test::CompareSelect8Test() : TruthTableComponentTest("CompareSelect8Test","COMPARE_SELECT8_ROOT",comparisonRows()) {}
ALU8FlagUnitTest::ALU8FlagUnitTest() : TruthTableComponentTest("ALU8FlagUnitTest","FLAGS8_ROOT",flagsRows()) {}

ALU8Test::ALU8Test() : TruthTableComponentTest("ALU8Test","ALU8_ROOT", [] {
    std::vector<TestRow> rows;
    for (unsigned op=0; op<32; ++op) {
        rows.push_back(aluRowNew(0x81,3,op,false));
        rows.push_back(aluRowNew(0x7F,1,op,true));
    }
    for (auto [a,b,op,cin] : std::vector<std::array<unsigned,4>>{
        {255,1,0,0},{0,0,6,0},{128,0,6,0},{255,255,3,0},{0x81,0,16,1},{0x81,8,20,1}})
        rows.push_back(aluRowNew(a,b,op,cin));
    return rows;
}()) {}

void ALU8ContractTest::verifyResults() {
    const auto started = std::chrono::steady_clock::now();
    Harness add(Component::create<AddSub8>("ADD_SUB"), &circuit::families::AddSub8);
    for (unsigned a=0; a<256; ++a) for (unsigned b=0; b<256; ++b)
        for (bool sub : {false,true}) for (bool cin : {false,true}) add.check(addsubRow(a,b,sub,cin));
    add.report();

    Harness shifter(Component::create<BarrelShifter8>("SHIFT"), &circuit::families::BarrelShifter8);
    for (unsigned op=0; op<5; ++op) for (unsigned a=0; a<256; ++a)
        for (unsigned distance=0; distance<8; ++distance) for (bool cin : {false,true}) {
            auto r = aluExpected(a,distance,op+16,cin);
            shifter.check({{{"A",bits(a)},{"B",bits(distance | 0xF8)},{"OP",bits(op)},{"C_IN",bit(cin)}},
                {{"OUT",bits(r.out)},{"CARRY",bit(r.carry)}}});
        }
    shifter.report();

    Harness alu(Component::create<ALU8>("ALU"), &circuit::families::ALU8);
    if (alu.root->getInputPinDynamic("OP")->getWidth()!=5) throw std::runtime_error("ALU8 opcode width");
    std::map<std::string,size_t> type_counts;
    std::function<void(std::shared_ptr<Component>)> visit = [&](auto c) {
        ++type_counts[c->getTypeName()];
        for (const auto& child : c->getChildren()) visit(child);
    };
    visit(alu.root);
    if (type_counts["AddSub8"] != 1 || type_counts["Adder8"] != 1 || type_counts["FullAdder"] != 8)
        throw std::runtime_error("ALU8 must share one structural eight-bit adder");
    constexpr std::array<unsigned,9> edges{0,1,2,7,15,127,128,254,255};
    for (unsigned op=0; op<32; ++op) for (auto a : edges) for (auto b : edges)
        for (bool cin : {false,true}) alu.check(aluRowNew(a,b,op,cin));
    std::mt19937 random(0xA108);
    for (size_t i=0; i<2048; ++i) {
        const auto a=random()&255, b=random()&255, op=random()&31, cin=random()&1;
        alu.check(aluRowNew(a,b,op,cin));
    }
    // All ignored operand/carry inputs must remain isolated, including X/Z.
    for (auto unknown : {LogicValue::UNKNOWN,LogicValue::HIGH_Z}) {
        for (unsigned op : {ALU8Op::INC,ALU8Op::DEC,ALU8Op::NEG,ALU8Op::NOT,ALU8Op::PASS_A}) {
            auto row=aluRowNew(0x81,0,op,false);
            row.inputs["B"] = TestValue(std::vector<LogicValue>(8,unknown));
            row.inputs["C_IN"] = TestValue(unknown);
            alu.check(row);
        }
        for (unsigned op : {7u,15u,31u,unsigned(ALU8Op::CLEAR)}) {
            auto row=aluRowNew(0,0,op,false);
            row.inputs["A"] = row.inputs["B"] = TestValue(std::vector<LogicValue>(8,unknown));
            row.inputs["C_IN"] = TestValue(unknown);
            alu.check(row);
        }
        auto masked = aluRowNew(0,0,ALU8Op::AND,false);
        masked.inputs["A"] = TestValue(std::vector<LogicValue>(8,unknown));
        alu.check(masked);
        auto input = circuit::test::logicBits(8,0); input[0]=unknown;
        auto output = circuit::test::logicBits(8,0); output[0]=LogicValue::UNKNOWN;
        auto row=aluRowNew(0,0,ALU8Op::XOR,false);
        row.inputs["A"]=TestValue(input); row.outputs["OUT"]=TestValue(output);
        row.outputs["ZERO"]=TestValue(LogicValue::UNKNOWN); alu.check(row);
        auto shifted=aluRowNew(0x81,1,ALU8Op::SLL,false);
        auto shift_amount=circuit::test::logicBits(8,1);
        for (size_t i=3;i<8;++i) shift_amount[i]=unknown;
        shifted.inputs["B"]=TestValue(shift_amount); shifted.inputs["C_IN"]=TestValue(unknown);
        alu.check(shifted);
        auto zero_shift=aluRowNew(0x81,0,ALU8Op::ROR,false);
        zero_shift.inputs["C_IN"]=TestValue(unknown);
        zero_shift.outputs["CARRY"]=TestValue(LogicValue::UNKNOWN); alu.check(zero_shift);
        auto unknown_op=aluRowNew(0,0,0,false);
        unknown_op.inputs["OP"]=TestValue(std::vector<LogicValue>(5,unknown));
        unknown_op.outputs.clear(); unknown_op.outputs["INVALID_OP"]=TestValue(LogicValue::UNKNOWN);
        alu.check(unknown_op);
    }
    alu.report();
    std::cout << "ALU8 contract checks took " << std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count() << " seconds\n";
}
