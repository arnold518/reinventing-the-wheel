#include "components/selection/BuiltinComponentCatalog.hpp"

#include "components/BasicComponent.hpp"
#include "components/Component.hpp"
#include "components/IOComponent.hpp"
#include "modules/basic/ClockGenerator.hpp"
#include "modules/basic/DFlipFlop.hpp"
#include "modules/basic/Decoder.hpp"
#include "modules/basic/Gate.hpp"
#include "modules/basic/Latch.hpp"
#include "modules/basic/Mux.hpp"
#include "modules/composite/ALU32.hpp"
#include "modules/composite/ALU8.hpp"
#include "modules/composite/ALU8ControlDecoder.hpp"
#include "modules/composite/ALU8ResultSelector.hpp"
#include "modules/composite/ALU32ControlDecoder.hpp"
#include "modules/composite/ALU32CompareUnit.hpp"
#include "modules/composite/ALU32ResultSelector.hpp"
#include "modules/composite/ALU32FlagUnit.hpp"
#include "modules/composite/AddSub8.hpp"
#include "modules/composite/ArithmeticUnit8.hpp"
#include "modules/composite/LogicUnit8.hpp"
#include "modules/composite/BarrelShifter8.hpp"
#include "modules/composite/CompareSelect8.hpp"
#include "modules/composite/ALU8FlagUnit.hpp"
#include "modules/composite/AddSub32.hpp"
#include "modules/composite/Adder32.hpp"
#include "modules/composite/Adder8.hpp"
#include "modules/composite/FullAdder.hpp"
#include "modules/composite/HalfAdder.hpp"
#include "modules/composite/Logic32.hpp"
#include "modules/composite/Shifter32.hpp"
#include "modules/composite/ZeroDetect32.hpp"
#include "modules/composite/ZeroDetect8.hpp"
#include "modules/memory/FFMemoryCatalog.hpp"
#include "modules/memory/SRAM.hpp"
#include "modules/memory/DRAM.hpp"
#include "modules/memory/ROM.hpp"
#include "modules/memory/NORFlash.hpp"
#include "modules/memory/MemoryProtocol.hpp"
#include "modules/memory/FlipFlopCell.hpp"
#include "modules/memory/Register.hpp"
#include "modules/rv32i/RegisterFile32x32.hpp"
#include "modules/rv32i/RV32IBitPatternMatcher.hpp"
#include "modules/rv32i/RV32IMemoryRequest.hpp"
#include "modules/rv32i/RV32IDecodeControlUnit.hpp"
#include "modules/rv32i/RV32IExecutionControlStatusUnit.hpp"
#include "modules/rv32i/RV32IPipelineControl.hpp"
#include "modules/rv32i/RV32IPipelineRegisters.hpp"
#include "modules/rv32i/RV32IPipelineStages.hpp"
#include "modules/rv32i/RV32IFiveStageCore.hpp"
#include "modules/rv32i/RV32IPrediction.hpp"
#include "modules/rv32i/RV32ISingleCycleCore.hpp"
#include "modules/rv32i/RV32ISingleCycleStages.hpp"
#include "modules/rv32i/RV32IBaselinePipeline.hpp"
#include "modules/rv32i/RV32ISingleCycleSystem.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Constant.hpp"
#include "modules/utility/Rewire.hpp"
#include "rv32i/RV32IValidationConfig.hpp"
#include <algorithm>
#include <charconv>
#include <stdexcept>
#include <tuple>
#include <type_traits>

namespace circuit {
namespace {

std::vector<PinDescriptor> pinSchema(const std::shared_ptr<IOComponent>& component) {
    std::vector<PinDescriptor> pins;
    for (const auto& [name, pin] : component->getAllInputPins()) {
        pins.push_back({name, pin->getWidth(), PinType::INPUT});
    }
    for (const auto& [name, pin] : component->getAllOutputPins()) {
        pins.push_back({name, pin->getWidth(), PinType::OUTPUT});
    }
    return pins;
}

template<typename T, typename... Args>
std::vector<PinDescriptor> schemaFor(Args&&... args) {
    auto component = std::make_shared<T>("SCHEMA", std::forward<Args>(args)...);
    component->initPins(component);
    return pinSchema(component);
}

std::vector<PinDescriptor> schemaFor(const ComponentFamily& family) {
    auto component = std::make_shared<IOComponent>(
        "SCHEMA", family.pinInitializer());
    component->initPins(component);
    return pinSchema(component);
}

std::vector<std::string> testIds(std::string test) {
    if (test.empty()) return {};
    return {std::move(test)};
}

VerificationEvidence verifiedStructural(std::vector<std::string> tests) {
    return {
        VerificationStatus::Verified,
        {"lower-level structural implementation"},
        std::move(tests),
        {},
        {},
        "four-state",
        "stable-after-settle",
    };
}

VerificationEvidence verifiedStructural(const std::string& test) {
    return verifiedStructural(testIds(test));
}

VerificationEvidence verifiedPrimitive(const std::string& tests) {
    return {
        VerificationStatus::Verified,
        {"terminal primitive truth function or routing rule"},
        testIds(tests),
        {},
        {},
        "four-state",
        "stable-after-settle",
    };
}

VerificationEvidence verifiedBehavioral(const std::string& lower_level,
                                        const std::string& tests,
                                        const std::string& equivalence,
                                        const std::string& domain = "four-state",
                                        const std::string& observation = "stable-after-settle") {
    return {
        VerificationStatus::Verified,
        {lower_level},
        testIds(tests),
        testIds(equivalence),
        {},
        domain,
        observation,
    };
}

VerificationEvidence verifiedBehavioralFromSlices(
    std::string lower_level,
    std::string contract_test,
    std::vector<std::string> representative_tests,
    std::string domain,
    std::string observation) {
    return {
        VerificationStatus::Verified,
        {std::move(lower_level)},
        testIds(std::move(contract_test)),
        {},
        std::move(representative_tests),
        std::move(domain),
        std::move(observation),
    };
}

VerificationEvidence verifiedBehavioralAgainstStructure(
    std::string lower_level,
    std::vector<std::string> tests) {
    return {
        VerificationStatus::Verified,
        {std::move(lower_level)},
        tests,
        std::move(tests),
        {},
        "known-binary",
        "clock-boundary",
    };
}

template<typename T, typename... Args>
ComponentFactory fixedFactory(Args... args) {
    auto stored = std::make_tuple(std::move(args)...);
    return [stored = std::move(stored)](
        const std::string& name,
        const ParameterMap&,
        const std::shared_ptr<BuildContext>& context) mutable {
        return std::apply([&](const auto&... unpacked) -> std::shared_ptr<Component> {
            return Component::createWithContext<T>(context, name, unpacked...);
        }, stored);
    };
}

template<typename T, typename... Args>
void addSingle(ComponentCatalog& catalog,
               std::string contract_id,
               std::string implementation_id,
               std::string display_name,
               Fidelity fidelity,
               VerificationEvidence evidence,
               bool terminal_primitive,
               Args... args) {
    catalog.registerContract({
        contract_id,
        std::is_same_v<T, ALU8> ? 2u : 1u,
        std::move(display_name),
        schemaFor<T>(args...),
        {},
        evidence.semantic_domain,
        evidence.observation,
    });
    ImplementationDescriptor implementation;
    implementation.id = std::move(implementation_id);
    implementation.contract_id = std::move(contract_id);
    implementation.fidelity = fidelity;
    implementation.terminal_primitive = terminal_primitive;
    implementation.concrete_type_name = T::TypeName;
    implementation.evidence = std::move(evidence);
    implementation.supports = [](const ParameterMap& parameters) {
        return parameters.empty();
    };
    implementation.factory = fixedFactory<T>(std::move(args)...);
    catalog.registerImplementation(std::move(implementation));
}

template<typename T>
void addStructural(ComponentCatalog& catalog,
                   const std::string& contract_id,
                   const std::string& implementation_id,
                   const std::string& display_name,
                   const std::string& tests) {
    addSingle<T>(catalog, contract_id, implementation_id, display_name,
                 Fidelity::Structural, verifiedStructural(tests), false);
}

template<typename T>
void addPrimitive(ComponentCatalog& catalog,
                  const std::string& contract_id,
                  const std::string& implementation_id,
                  const std::string& display_name,
                  const std::string& tests) {
    addSingle<T>(catalog, contract_id, implementation_id, display_name,
                 Fidelity::Structural, verifiedPrimitive(tests), true);
}

template<typename T>
ImplementationDescriptor implementationFor(
    std::string id,
    std::string contract_id,
    Fidelity fidelity,
    VerificationEvidence evidence) {
    ImplementationDescriptor implementation;
    implementation.id = std::move(id);
    implementation.contract_id = std::move(contract_id);
    implementation.fidelity = fidelity;
    implementation.concrete_type_name = T::TypeName;
    implementation.evidence = std::move(evidence);
    implementation.supports = [](const ParameterMap& parameters) {
        return parameters.empty();
    };
    implementation.factory = fixedFactory<T>();
    return implementation;
}

ImplementationDescriptor implementationFor(
    const ComponentFamily& family,
    std::string id,
    Fidelity fidelity,
    VerificationEvidence evidence) {
    if (!family.supports(fidelity)) {
        throw std::invalid_argument(
            "Family does not provide the registered implementation fidelity");
    }
    ImplementationDescriptor implementation;
    implementation.id = std::move(id);
    implementation.contract_id = std::string(family.id());
    implementation.fidelity = fidelity;
    implementation.concrete_type_name = std::string(family.typeName());
    implementation.evidence = std::move(evidence);
    implementation.supports = [](const ParameterMap& parameters) {
        return parameters.empty();
    };
    implementation.factory = [&family, fidelity](
        const std::string& name,
        const ParameterMap&,
        const std::shared_ptr<BuildContext>& context) {
        return family.create(fidelity, name, context);
    };
    return implementation;
}

void addSelectableFamily(
    ComponentCatalog& catalog,
    const ComponentFamily& family,
    const std::string& implementation_prefix,
    const std::string& display_name,
    const std::string& contract_test,
    const std::string& lower_level_description,
    uint32_t contract_version = 1,
    const std::string& extra_equivalence_test = {}) {
    const auto contract_id = std::string(family.id());
    catalog.registerContract({
        contract_id,
        contract_version,
        display_name,
        schemaFor(family),
        {},
        "four-state",
        "stable-after-settle",
    });
    catalog.registerImplementation(implementationFor(
        family,
        implementation_prefix + ".structural",
        Fidelity::Structural,
        verifiedStructural(contract_test)));
    auto evidence = verifiedBehavioral(
            lower_level_description,
            contract_test,
            contract_test,
            "four-state",
            "stable-after-settle");
    if (!extra_equivalence_test.empty()) evidence.equivalence_tests.push_back(extra_equivalence_test);
    catalog.registerImplementation(implementationFor(
        family, implementation_prefix + ".behavioral.direct", Fidelity::Behavioral, std::move(evidence)));
}

uint64_t parseUnsigned(const std::string& text) {
    size_t consumed = 0;
    const auto value = std::stoull(text, &consumed, 0);
    if (consumed != text.size()) {
        throw std::invalid_argument("Invalid unsigned parameter '" + text + "'");
    }
    return value;
}

std::vector<std::string> split(const std::string& text, char delimiter) {
    std::vector<std::string> result;
    size_t start = 0;
    while (start <= text.size()) {
        const auto end = text.find(delimiter, start);
        result.push_back(text.substr(start, end == std::string::npos
            ? std::string::npos : end - start));
        if (end == std::string::npos) break;
        start = end + 1;
    }
    if (result.size() == 1 && result.front().empty()) result.clear();
    return result;
}

std::vector<Rewire::WireSpec> parseWireSpecs(const std::string& text) {
    std::vector<Rewire::WireSpec> result;
    for (const auto& item : split(text, ',')) {
        const auto colon = item.find(':');
        if (colon == std::string::npos) {
            throw std::invalid_argument("Invalid Rewire wire spec '" + item + "'");
        }
        result.push_back({item.substr(0, colon),
                          static_cast<size_t>(parseUnsigned(item.substr(colon + 1)))});
    }
    return result;
}

std::vector<Rewire::BitMap> parseBitMaps(const std::string& text) {
    std::vector<Rewire::BitMap> result;
    for (const auto& item : split(text, ',')) {
        const auto arrow = item.find('>');
        const auto left_colon = item.find(':');
        const auto right_colon = item.find(':', arrow == std::string::npos ? 0 : arrow + 1);
        if (arrow == std::string::npos || left_colon == std::string::npos
            || right_colon == std::string::npos || left_colon > arrow) {
            throw std::invalid_argument("Invalid Rewire bit mapping '" + item + "'");
        }
        result.push_back({
            item.substr(0, left_colon),
            static_cast<size_t>(parseUnsigned(item.substr(left_colon + 1,
                                                          arrow - left_colon - 1))),
            item.substr(arrow + 1, right_colon - arrow - 1),
            static_cast<size_t>(parseUnsigned(item.substr(right_colon + 1))),
        });
    }
    return result;
}

Rewire::UnmappedBitValue parseUnmapped(const std::string& value) {
    if (value == "low") return Rewire::UnmappedBitValue::LOW;
    if (value == "high") return Rewire::UnmappedBitValue::HIGH;
    if (value == "unknown") return Rewire::UnmappedBitValue::UNKNOWN;
    throw std::invalid_argument("Invalid Rewire unmapped value '" + value + "'");
}

template<size_t WIDTH>
void addSplitter(ComponentCatalog& catalog) {
    addPrimitive<BitSplitter<WIDTH>>(
        catalog,
        "utility.bit-splitter.width" + std::to_string(WIDTH),
        "utility.bit-splitter.width" + std::to_string(WIDTH) + ".primitive",
        "Bit splitter " + std::to_string(WIDTH),
        "BitSplitter" + std::to_string(WIDTH) + "Test");
}

template<size_t WIDTH>
void addJoiner(ComponentCatalog& catalog) {
    addPrimitive<BitJoiner<WIDTH>>(
        catalog,
        "utility.bit-joiner.width" + std::to_string(WIDTH),
        "utility.bit-joiner.width" + std::to_string(WIDTH) + ".primitive",
        "Bit joiner " + std::to_string(WIDTH),
        "BitJoiner" + std::to_string(WIDTH) + "Test");
}

template<size_t WIDTH>
void addConstant(ComponentCatalog& catalog) {
    const auto contract_id = "utility.constant.width" + std::to_string(WIDTH);
    catalog.registerContract({
        contract_id, 1, "Constant " + std::to_string(WIDTH),
        schemaFor<ConstantValue<WIDTH>>(0), {}, "four-state", "initial-event"});
    ImplementationDescriptor implementation;
    implementation.id = contract_id + ".primitive";
    implementation.contract_id = contract_id;
    implementation.fidelity = Fidelity::Structural;
    implementation.terminal_primitive = true;
    implementation.concrete_type_name = ConstantValue<WIDTH>::TypeName;
    implementation.evidence = verifiedPrimitive(
        "ConstantValue" + std::to_string(WIDTH) + "Test");
    implementation.supports = [](const ParameterMap& parameters) {
        return parameters.size() == 1 && parameters.count("value") == 1;
    };
    implementation.factory = [](
        const std::string& name,
        const ParameterMap& parameters,
        const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<ConstantValue<WIDTH>>(
            context, name, parseUnsigned(parameters.at("value")));
    };
    catalog.registerImplementation(std::move(implementation));
}

void registerPrimitives(ComponentCatalog& catalog) {
    addPrimitive<NOTGate>(catalog, "logic.not", "logic.not.primitive", "NOT gate", "NOTGateTest");
    addPrimitive<ANDGate>(catalog, "logic.and", "logic.and.primitive", "AND gate", "ANDGateTest");
    addPrimitive<NANDGate>(catalog, "logic.nand", "logic.nand.primitive", "NAND gate", "NANDGateTest");
    addPrimitive<ORGate>(catalog, "logic.or", "logic.or.primitive", "OR gate", "ORGateTest");
    addPrimitive<NORGate>(catalog, "logic.nor", "logic.nor.primitive", "NOR gate", "NORGateTest");
    addPrimitive<XORGate>(catalog, "logic.xor", "logic.xor.primitive", "XOR gate", "XORGateTest");

    addSplitter<1>(catalog); addJoiner<1>(catalog);
    addSplitter<2>(catalog); addJoiner<2>(catalog);
    addSplitter<3>(catalog); addJoiner<3>(catalog);
    addSplitter<4>(catalog); addJoiner<4>(catalog);
    addSplitter<5>(catalog); addJoiner<5>(catalog);
    addSplitter<8>(catalog); addJoiner<8>(catalog);
    addSplitter<16>(catalog); addJoiner<16>(catalog);
    addSplitter<32>(catalog); addJoiner<32>(catalog);

    addConstant<1>(catalog);
    addConstant<2>(catalog);
    addConstant<4>(catalog);
    addConstant<8>(catalog);
    addConstant<32>(catalog);

    catalog.registerContract({
        "utility.rewire", 1, "Generic bit rewire", {}, {}, "four-state",
        "stable-after-settle"});
    ImplementationDescriptor rewire;
    rewire.id = "utility.rewire.primitive";
    rewire.contract_id = "utility.rewire";
    rewire.fidelity = Fidelity::Structural;
    rewire.terminal_primitive = true;
    rewire.concrete_type_name = Rewire::TypeName;
    rewire.evidence = verifiedPrimitive("RewireTest");
    rewire.supports = [](const ParameterMap& parameters) {
        return parameters.count("inputs") && parameters.count("outputs")
            && parameters.count("mappings") && parameters.count("unmapped");
    };
    rewire.factory = [](
        const std::string& name,
        const ParameterMap& parameters,
        const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<Rewire>(
            context,
            name,
            parseWireSpecs(parameters.at("inputs")),
            parseWireSpecs(parameters.at("outputs")),
            parseBitMaps(parameters.at("mappings")),
            parseUnmapped(parameters.at("unmapped")));
    };
    catalog.registerImplementation(std::move(rewire));

    catalog.registerContract({
        "timing.clock", 1, "Clock generator", schemaFor<ClockGenerator>(5), {},
        "four-state", "scheduled-source"});
    ImplementationDescriptor clock;
    clock.id = "timing.clock.primitive";
    clock.contract_id = "timing.clock";
    clock.fidelity = Fidelity::Structural;
    clock.terminal_primitive = true;
    clock.concrete_type_name = ClockGenerator::TypeName;
    clock.evidence = verifiedPrimitive("ClockGeneratorTest");
    clock.supports = [](const ParameterMap& parameters) {
        return parameters.size() == 1 && parameters.count("half_period");
    };
    clock.factory = [](
        const std::string& name,
        const ParameterMap& parameters,
        const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<ClockGenerator>(
            context, name, static_cast<size_t>(parseUnsigned(parameters.at("half_period"))));
    };
    catalog.registerImplementation(std::move(clock));
}

void registerStructuralFamilies(ComponentCatalog& catalog) {
    addStructural<SRLatch>(catalog, "sequential.sr-latch", "sequential.sr-latch.structural", "SR latch", "SRLatchTest");
    addStructural<GatedDLatch>(catalog, "sequential.gated-d-latch", "sequential.gated-d-latch.structural", "Gated D latch", "GatedDLatchTest");
    addStructural<DFlipFlop>(catalog, "sequential.d-flip-flop", "sequential.d-flip-flop.structural", "D flip-flop", "DFlipFlopTest");
    addStructural<Decoder5to32>(catalog, "decode.5-to-32", "decode.5-to-32.structural", "5-to-32 decoder", "Decoder5to32Test");


    addStructural<Mux2to1>(catalog, "mux.2x1.width1", "mux.2x1.width1.structural", "2:1 bit mux", "Mux2to1Test");
    addStructural<Mux4to1>(catalog, "mux.4x1.width1", "mux.4x1.width1.structural", "4:1 bit mux", "Mux4to1Test");
    addStructural<Mux8to1>(catalog, "mux.8x1.width1", "mux.8x1.width1.structural", "8:1 bit mux", "Mux8to1Test");
    addStructural<Mux32to1>(catalog, "mux.32x1.width1", "mux.32x1.width1.structural", "32:1 bit mux", "Mux32to1Test");
    addStructural<Mux2to1_4bit>(catalog, "mux.2x1.width4", "mux.2x1.width4.structural", "2:1 4-bit mux", "Mux2to1_4bitTest");
    addStructural<Mux2to1_32bit>(catalog, "mux.2x1.width32", "mux.2x1.width32.structural", "2:1 32-bit mux", "Mux2to1_32bitTest");
    addStructural<Mux4to1_32bit>(catalog, "mux.4x1.width32", "mux.4x1.width32.structural", "4:1 32-bit mux", "Mux4to1_32bitTest");
    addStructural<Mux8to1_8bit>(catalog, "mux.8x1.width8", "mux.8x1.width8.structural", "8:1 8-bit mux", "Mux8to1_8bitTest");
    addStructural<Mux8to1_32bit>(catalog, "mux.8x1.width32", "mux.8x1.width32.structural", "8:1 32-bit mux", "Mux8to1_32bitTest");
    addStructural<Mux32to1_32bit>(catalog, "mux.32x1.width32", "mux.32x1.width32.structural", "32:1 32-bit mux", "Mux32to1_32bitTest");

    addStructural<HalfAdder>(catalog, "arithmetic.half-adder", "arithmetic.half-adder.structural", "Half adder", "HalfAdderTest");
    addStructural<FullAdder>(catalog, "arithmetic.full-adder", "arithmetic.full-adder.structural", "Full adder", "FullAdderTest");
    addSelectableFamily(catalog, families::Adder8, "arithmetic.adder.width8", "8-bit adder", "Adder8Test", "Adder8 structural circuit", 1, "ALU8FidelityContractTest");
    addStructural<Adder32>(catalog, "arithmetic.adder.width32", "arithmetic.adder.width32.structural", "32-bit adder", "Adder32Test");
    addSelectableFamily(
        catalog,
        families::AddSub32,
        "arithmetic.add-sub.width32",
        "32-bit add/sub",
        "AddSub32Test",
        "AddSub32 structural full-adder chain");
    addSelectableFamily(catalog, families::ZeroDetect8, "compare.zero.width8", "8-bit zero detect", "ZeroDetect8Test", "ZeroDetect8 structural circuit", 1, "ALU8FidelityContractTest");
    addSelectableFamily(
        catalog,
        families::ZeroDetect32,
        "compare.zero.width32",
        "32-bit zero detect",
        "ZeroDetect32Test",
        "ZeroDetect32 structural OR-reduction tree");
    addSelectableFamily(
        catalog,
        families::Logic32,
        "logic.combined.width32",
        "32-bit logic",
        "Logic32Test",
        "Logic32 structural per-bit gate array");
    addSelectableFamily(
        catalog,
        families::Shifter32,
        "shift.barrel.width32",
        "32-bit shifter",
        "Shifter32Test",
        "Shifter32 structural mux network");
    addSelectableFamily(catalog, families::ALU8, "alu.width8", "8-bit ALU", "ALU8Test", "ALU8 structural circuit", 2, "ALU8FidelityContractTest");
    addSelectableFamily(catalog, families::AddSub8, "arithmetic.add-sub.width8", "8-bit add/sub with carry", "AddSub8Test", "AddSub8 structural circuit", 1, "ALU8FidelityContractTest");
    addSelectableFamily(catalog, families::ArithmeticUnit8, "arithmetic.unit.width8", "8-bit arithmetic unit", "ArithmeticUnit8Test", "ArithmeticUnit8 structural circuit", 1, "ALU8FidelityContractTest");
    addSelectableFamily(catalog, families::LogicUnit8, "logic.unit.width8", "8-bit logic unit", "LogicUnit8Test", "LogicUnit8 structural circuit", 1, "ALU8FidelityContractTest");
    addSelectableFamily(catalog, families::BarrelShifter8, "shift.barrel.width8", "8-bit barrel shifter", "BarrelShifter8Test", "BarrelShifter8 structural circuit", 1, "ALU8FidelityContractTest");
    addSelectableFamily(catalog, families::CompareSelect8, "compare.select.width8", "8-bit comparison result selector", "CompareSelect8Test", "CompareSelect8 structural circuit", 1, "ALU8FidelityContractTest");
    addSelectableFamily(catalog, families::ALU8FlagUnit, "alu.flags.width8", "8-bit ALU flags", "ALU8FlagUnitTest", "ALU8FlagUnit structural circuit", 1, "ALU8FidelityContractTest");
    addSelectableFamily(catalog, families::ALU8ControlDecoder, "alu.control.width8", "ALU8ControlDecoder", "ALU8ControlDecoderTest", "ALU8ControlDecoder structural circuit", 1, "ALU8FidelityContractTest");
    addSelectableFamily(catalog, families::ALU8ResultSelector, "alu.result-selector.width8", "ALU8ResultSelector", "ALU8ResultSelectorTest", "ALU8ResultSelector structural circuit", 1, "ALU8FidelityContractTest");
    addStructural<ALU32ControlDecoder>(catalog, "alu.control.width32", "alu.control.width32.structural", "ALU32ControlDecoder", "ALU32ControlDecoderTest");
    addStructural<ALU32CompareUnit>(catalog, "alu.compare.width32", "alu.compare.width32.structural", "ALU32CompareUnit", "ALU32CompareUnitTest");
    addStructural<ALU32ResultSelector>(catalog, "alu.result-selector.width32", "alu.result-selector.width32.structural", "ALU32ResultSelector", "ALU32ResultSelectorTest");
    addStructural<ALU32FlagUnit>(catalog, "alu.flags.width32", "alu.flags.width32.structural", "ALU32FlagUnit", "ALU32FlagUnitTest");
    addStructural<RV32IMemoryLanes>(catalog, "rv32i.memory-lanes", "rv32i.memory-lanes.structural", "RV32IMemoryLanes", "RV32IMemoryLanesTest");
    addStructural<RV32IMemoryRequest>(catalog, "rv32i.memory-request", "rv32i.memory-request.structural", "RV32IMemoryRequest", "RV32IMemoryRequestTest");
}

void registerStorage(ComponentCatalog& catalog) {
    const auto flip_flop_cell = std::string(families::FlipFlopCell.id());
    catalog.registerContract({
        flip_flop_cell, 1, "Write-enabled flip-flop cell",
        schemaFor(families::FlipFlopCell), {}, "four-state", "clock-boundary"});
    catalog.registerImplementation(implementationFor(
        families::FlipFlopCell,
        "memory.ff.cell.structural.mux-dff",
        Fidelity::Structural, verifiedStructural("FlipFlopCellTest")));
    catalog.registerImplementation(implementationFor(
        families::FlipFlopCell,
        "memory.ff.cell.behavioral.direct",
        Fidelity::Behavioral,
        verifiedBehavioral("FlipFlopCell structural mux/DFF", "FlipFlopCellTest",
                           "FlipFlopCellTest", "four-state", "clock-boundary")));


    const auto register_file =
        std::string(families::RegisterFile32x32.id());
    catalog.registerContract({
        register_file, 1, "RV32I 32x32 register file",
        schemaFor(families::RegisterFile32x32), {"register-state-view"},
        "four-state", "clock-boundary"});
    auto structural = implementationFor(
        families::RegisterFile32x32,
        "rv32i.register-file.structural.decoder-mux",
        Fidelity::Structural, verifiedStructural("RegisterFile32x32Test"));
    structural.capabilities = {"register-state-view"};
    catalog.registerImplementation(std::move(structural));
    auto behavioral = implementationFor(
        families::RegisterFile32x32,
        "rv32i.register-file.behavioral.direct",
        Fidelity::Behavioral,
        verifiedBehavioral("RegisterFile32x32 structural organization",
                           "RegisterFile32x32Test",
                           "RegisterFile32x32Test", "four-state", "clock-boundary"));
    behavioral.capabilities = {"register-state-view"};
    catalog.registerImplementation(std::move(behavioral));


}

// One known-binary contract with exactly one structural and one behavioral
// implementation, both proven by the same component test.
struct FamilyPair {
    const ComponentFamily& family;
    std::string display_name;
    std::string test;
    std::string lower_level;
    std::string structural_id;
    std::string behavioral_id;
    std::string observation = "stable-after-settle";
    uint32_t version = 1;
    std::vector<std::string> capabilities = {};
    ParameterPredicate supports = {};
};

void addFamilyPair(ComponentCatalog& catalog, const FamilyPair& entry) {
    catalog.registerContract({
        std::string(entry.family.id()), entry.version, entry.display_name,
        schemaFor(entry.family), entry.capabilities, "known-binary", entry.observation});
    auto structural = implementationFor(
        entry.family, entry.structural_id, Fidelity::Structural,
        verifiedStructural(entry.test));
    auto behavioral = implementationFor(
        entry.family, entry.behavioral_id, Fidelity::Behavioral,
        verifiedBehavioral(entry.lower_level, entry.test, entry.test,
                           "known-binary", entry.observation));
    for (auto* implementation : {&structural, &behavioral}) {
        implementation->capabilities = entry.capabilities;
        if (entry.supports) implementation->supports = entry.supports;
        catalog.registerImplementation(std::move(*implementation));
    }
}

void registerRV32I(ComponentCatalog& catalog) {
    addFamilyPair(catalog, {families::ALU32, "RV32I ALU32", "ALU32Test",
        "ALU32 structural implementation",
        "rv32i.alu32.structural", "rv32i.alu32.behavioral.direct"});
    addFamilyPair(catalog, {families::ALU32Core, "RV32I core ALU datapath", "ALU32CoreTest",
        "ALU32 shared arithmetic and compare network",
        "rv32i.alu32-core.structural", "rv32i.alu32-core.behavioral.direct"});
    addFamilyPair(catalog, {families::RV32IDecodeControl, "RV32I decode and control",
        "RV32IDecodeControlUnitTest", "RV32IDecodeControlUnit structure",
        "rv32i.decode-control.structural", "rv32i.decode-control.behavioral.direct"});
    addFamilyPair(catalog, {families::RV32IExecutionStatus, "RV32I execution status",
        "RV32IExecutionControlStatusUnitTest", "RV32IExecutionControlStatusUnit structure",
        "rv32i.execution-status.structural", "rv32i.execution-status.behavioral.direct",
        "clock-boundary"});

    catalog.registerContract({
        "rv32i.pattern-matcher", 1, "RV32I pattern matcher",
        schemaFor<RV32IBitPatternMatcher>(0U, 0U), {}, "four-state",
        "stable-after-settle"});
    ImplementationDescriptor matcher;
    matcher.id = "rv32i.pattern-matcher.structural";
    matcher.contract_id = "rv32i.pattern-matcher";
    matcher.fidelity = Fidelity::Structural;
    matcher.concrete_type_name = RV32IBitPatternMatcher::TypeName;
    matcher.evidence = verifiedStructural("RV32IBitPatternMatcherTest");
    matcher.supports = [](const ParameterMap& parameters) {
        return parameters.size() == 2 && parameters.count("mask")
            && parameters.count("value");
    };
    matcher.factory = [](
        const std::string& name,
        const ParameterMap& parameters,
        const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<RV32IBitPatternMatcher>(
            context, name,
            static_cast<uint32_t>(parseUnsigned(parameters.at("mask"))),
            static_cast<uint32_t>(parseUnsigned(parameters.at("value"))));
    };
    catalog.registerImplementation(std::move(matcher));

    const auto add_pipeline_register = [&](
        const ComponentFamily& family,
        const std::string& display_name,
        const std::string& test_name) {
        const auto id = std::string(family.id());
        addFamilyPair(catalog, {family, display_name, test_name,
            display_name + " structural Register<32> bank",
            id + ".structural.register32-bank", id + ".behavioral.direct",
            "clock-boundary", 1, {},
            [](const ParameterMap& parameters) {
                return parameters.empty() ||
                    (parameters.size() == 1 && parameters.contains("compact_packet") &&
                     parameters.at("compact_packet") == "1");
            }});
    };

    add_pipeline_register(
        families::RV32IIFIDPipelineRegister,
        "RV32I IF/ID pipeline register",
        "RV32IIFIDPipelineRegisterTest");
    add_pipeline_register(
        families::RV32IIDEXPipelineRegister,
        "RV32I ID/EX pipeline register",
        "RV32IIDEXPipelineRegisterTest");
    add_pipeline_register(
        families::RV32IEXMEMPipelineRegister,
        "RV32I EX/MEM pipeline register",
        "RV32IEXMEMPipelineRegisterTest");
    add_pipeline_register(
        families::RV32IMEMWBPipelineRegister,
        "RV32I MEM/WB pipeline register",
        "RV32IMEMWBPipelineRegisterTest");

    const auto add_pipeline_control = [&](
        const ComponentFamily& family,
        const std::string& display_name,
        const std::string& test_name,
        bool configurable_fetch = false) {
        const auto id = std::string(family.id());
        ParameterPredicate supports;
        if (configurable_fetch) {
            supports = [](const ParameterMap& parameters) {
                return parameters.empty() ||
                    (parameters.size() == 1 && parameters.contains("external_next_pc") &&
                     (parameters.at("external_next_pc") == "0" ||
                      parameters.at("external_next_pc") == "1"));
            };
        }
        addFamilyPair(catalog, {family, display_name, test_name,
            display_name + " structural gate network",
            id + ".structural.gates", id + ".behavioral.direct",
            "stable-after-settle", 1, {}, supports});
    };

    add_pipeline_control(
        families::RV32IForwardingUnit,
        "RV32I pipeline forwarding unit",
        "RV32IForwardingUnitTest");
    for (const auto* family : {&families::RV32IPredictionCounter, &families::RV32IBranchPredecode,
                              &families::RV32IBranchRecovery, &families::RV32IBranchPredictor,
                              &families::RV32IPredictionPath}) {
        const std::string id(family->id()), name(family->typeName());
        const bool configured = family==&families::RV32IBranchPredictor || family==&families::RV32IPredictionPath;
        addFamilyPair(catalog, {*family, name, name + "Test", name + " structural circuit",
            id + ".structural", id + ".behavioral", "stable-after-settle", 1, {},
            configured ? ParameterPredicate(rv32i::supportsPredictionParameters) : ParameterPredicate()});
    }
    add_pipeline_control(
        families::RV32IHazardDetectionUnit,
        "RV32I pipeline load-use hazard detector",
        "RV32IHazardDetectionUnitTest");
    add_pipeline_control(
        families::RV32IMemoryAlignmentUnit,
        "RV32I memory-alignment checker",
        "RV32IMemoryAlignmentUnitTest");
    add_pipeline_control(
        families::RV32IPipelineRetirementUnit,
        "RV32I pipeline retirement unit",
        "RV32IPipelineRetirementUnitTest");
    add_pipeline_control(
        families::RV32IPipelineCoordinator,
        "RV32I pipeline coordinator",
        "RV32IPipelineCoordinatorTest");
    add_pipeline_control(
        families::RV32IFetchStage,
        "RV32I pipeline fetch stage",
        "RV32IFetchStageTest", true);
    add_pipeline_control(
        families::RV32IDecodeStage,
        "RV32I pipeline decode stage",
        "RV32IDecodeStageTest");
    add_pipeline_control(
        families::RV32IExecuteStage,
        "RV32I pipeline execute stage",
        "RV32IExecuteStageTest");
    add_pipeline_control(
        families::RV32IMemoryStage,
        "RV32I pipeline memory stage",
        "RV32IMemoryStageTest");
    add_pipeline_control(
        families::RV32IPipelineExecutionStatus,
        "RV32I sticky pipeline execution status",
        "RV32IPipelineExecutionStatusTest");
    add_pipeline_control(
        families::RV32IWritebackStage,
        "RV32I pipeline writeback stage",
        "RV32IWritebackStageTest");

    const auto five_stage_core =
        std::string(families::RV32IFiveStageCore.id());
    catalog.registerContract({
        five_stage_core,
        2,
        "Educational RV32I five-stage core",
        schemaFor(families::RV32IFiveStageCore),
        {"rv32i-pipeline-state-view", "rv32i-retirement-view"},
        "known-binary",
        "clock-boundary",
    });
    auto five_stage_structural = implementationFor(
        families::RV32IFiveStageCore,
        "rv32i.core.educational-five-stage.structural",
        Fidelity::Structural,
        verifiedStructural("RV32IFiveStageCoreTest"));
    five_stage_structural.capabilities = {
        "rv32i-pipeline-state-view", "rv32i-retirement-view"};
    five_stage_structural.supports = rv32i::supportsPredictionParameters;
    catalog.registerImplementation(
        std::move(five_stage_structural));
    auto five_stage_behavioral = implementationFor(
        families::RV32IFiveStageCore,
        "rv32i.core.educational-five-stage.behavioral",
        Fidelity::Behavioral,
        verifiedBehavioralFromSlices(
            "Structural pipeline registers, forwarding, hazard detection, "
            "and control-flow slices",
            "RV32IFiveStageCoreTest",
            {
                "RV32IIFIDPipelineRegisterTest",
                "RV32IIDEXPipelineRegisterTest",
                "RV32IEXMEMPipelineRegisterTest",
                "RV32IMEMWBPipelineRegisterTest",
                "RV32IForwardingUnitTest",
                "RV32IHazardDetectionUnitTest",
                "RV32IMemoryAlignmentUnitTest",
                "RV32IPipelineRetirementUnitTest",
                "RV32IPipelineCoordinatorTest",
            },
            "known-binary",
            "clock-boundary"));
    five_stage_behavioral.capabilities = {
        "rv32i-pipeline-state-view", "rv32i-retirement-view"};
    five_stage_behavioral.supports = rv32i::supportsPredictionParameters;
    catalog.registerImplementation(
        std::move(five_stage_behavioral));

    for(const auto* stage : {&families::RV32ISingleCycleFetchStage,
            &families::RV32ISingleCycleDecodeStage, &families::RV32ISingleCycleExecuteStage,
            &families::RV32ISingleCycleMemoryStage, &families::RV32ISingleCycleWritebackStage,
            &families::RV32ISingleCycleNextPCStage}) {
        const std::string id(stage->id()), name(stage->typeName());
        addFamilyPair(catalog, {*stage, name, name + "Test", name + " structural datapath",
            id + ".structural", id + ".behavioral", "stable-after-settle", 1,
            stage==&families::RV32ISingleCycleDecodeStage
                ? std::vector<std::string>{"register-state-view"} : std::vector<std::string>{}});
    }

    for(const auto* block : {&families::RV32IBaselineFetchStage, &families::RV32IBaselineDecodeStage,
            &families::RV32IBaselineExecuteStage, &families::RV32IBaselineMemoryStage,
            &families::RV32IBaselineDependencyUnit, &families::RV32IBaselineControl,
            &families::RV32IBaselinePipelineCore}) {
        const std::string id(block->id()), name(block->typeName());
        const bool core=block==&families::RV32IBaselinePipelineCore;
        addFamilyPair(catalog, {*block, name, name + "Test", name + " structural circuitry",
            id + ".structural", id + ".behavioral",
            core ? "clock-boundary" : "stable-after-settle", core ? 2u : 1u,
            core ? std::vector<std::string>{"rv32i-pipeline-state-view","rv32i-retirement-view"}
                 : block==&families::RV32IBaselineDecodeStage
                     ? std::vector<std::string>{"register-state-view"} : std::vector<std::string>{}});
    }

    const auto core =
        std::string(families::RV32ISingleCycleCore.id());
    catalog.registerContract({
        core, 2, "Educational RV32I single-cycle core",
        schemaFor(families::RV32ISingleCycleCore),
        {"rv32i-architectural-state-view"},
        "known-binary", "clock-boundary"});
    auto structural_core = implementationFor(
        families::RV32ISingleCycleCore,
        "rv32i.core.educational-single-cycle.structural",
        Fidelity::Structural,
        verifiedStructural("RV32ISingleCycleCoreTest"));
    structural_core.capabilities = {
        "rv32i-architectural-state-view"};
    catalog.registerImplementation(std::move(structural_core));
    auto behavioral_core = implementationFor(
        families::RV32ISingleCycleCore,
        "rv32i.core.educational-single-cycle.behavioral",
        Fidelity::Behavioral,
        verifiedBehavioralAgainstStructure(
            "RV32ISingleCycleCore structural data path",
            {"RV32ISingleCycleCoreTest"}));
    behavioral_core.capabilities = {
        "rv32i-architectural-state-view"};
    catalog.registerImplementation(std::move(behavioral_core));
    std::vector<std::string> educational_system_tests;
    // All committed correctness and performance programs prove the same
    // system contract and are therefore visualizable through one registry.
    for (size_t program = 1;
         program <= rv32i::validation::ProgramCount;
         ++program) {
        educational_system_tests.push_back(
            "RV32ISingleCycleSystemTest/program-"
            + std::string(program < 10 ? "0" : "")
            + std::to_string(program));
    }
    const auto system =
        std::string(families::RV32ISingleCycleSystem.id());
    catalog.registerContract({
        system, 1, "Educational RV32I single-cycle system",
        schemaFor(families::RV32ISingleCycleSystem),
        {"rv32i-architectural-state-view"},
        "known-binary", "clock-boundary"});
    auto structural_system = implementationFor(
        families::RV32ISingleCycleSystem,
        "rv32i.system.educational-single-cycle.structural",
        Fidelity::Structural,
        verifiedStructural(educational_system_tests));
    structural_system.capabilities = {
        "rv32i-architectural-state-view"};
    catalog.registerImplementation(std::move(structural_system));
    auto behavioral_system = implementationFor(
        families::RV32ISingleCycleSystem,
        "rv32i.system.educational-single-cycle.behavioral",
        Fidelity::Behavioral,
        verifiedBehavioralAgainstStructure(
            "RV32ISingleCycleSystem structural core and memory system",
            std::move(educational_system_tests)));
    behavioral_system.capabilities = {
        "rv32i-architectural-state-view"};
    catalog.registerImplementation(std::move(behavioral_system));
}

} // namespace

std::shared_ptr<ComponentCatalog> createBuiltinComponentCatalog() {
    auto catalog = std::make_shared<ComponentCatalog>();
    registerPrimitives(*catalog);
    registerStructuralFamilies(*catalog);
    registerStorage(*catalog);
    registerRV32I(*catalog);
    registerFFMemories(*catalog);
    registerSRAMComponents(*catalog);
    memory_device::registerMatrixComponents(*catalog);
    registerDRAMComponents(*catalog);
    registerROMComponents(*catalog);
    registerNORFlashComponents(*catalog);
    memory::registerMemoryEndpoints(*catalog);
    catalog->freeze();
    return catalog;
}

const ComponentCatalog& builtinComponentCatalog() {
    static const auto catalog = createBuiltinComponentCatalog();
    return *catalog;
}

} // namespace circuit
