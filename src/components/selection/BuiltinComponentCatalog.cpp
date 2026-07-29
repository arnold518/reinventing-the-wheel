#include "components/selection/BuiltinComponentCatalog.hpp"

#include "components/BasicComponent.hpp"
#include "components/Component.hpp"
#include "components/IOComponent.hpp"
#include "modules/basic/ClockGenerator.hpp"
#include "modules/basic/DFlipFlop.hpp"
#include "modules/basic/Decoder.hpp"
#include "modules/basic/Gate.hpp"
#include "modules/basic/Latch.hpp"
#include "modules/basic/Logic8.hpp"
#include "modules/basic/Mux.hpp"
#include "modules/composite/ALU32.hpp"
#include "modules/composite/ALU8.hpp"
#include "modules/composite/AddSub32.hpp"
#include "modules/composite/Adder32.hpp"
#include "modules/composite/Adder8.hpp"
#include "modules/composite/Arithmetic8.hpp"
#include "modules/composite/Comparator32.hpp"
#include "modules/composite/Comparator8.hpp"
#include "modules/composite/FullAdder.hpp"
#include "modules/composite/HalfAdder.hpp"
#include "modules/composite/Logic32.hpp"
#include "modules/composite/Shifter32.hpp"
#include "modules/composite/Shifter8.hpp"
#include "modules/composite/ZeroDetect32.hpp"
#include "modules/composite/ZeroDetect8.hpp"
#include "modules/memory/Memory64Kx32.hpp"
#include "modules/memory/Memory32x32.hpp"
#include "modules/memory/Memory4x32.hpp"
#include "modules/memory/MemoryBit.hpp"
#include "modules/memory/Register32.hpp"
#include "modules/memory/RegisterFile32x32.hpp"
#include "modules/memory/RegisterFile4x32.hpp"
#include "modules/rv32i/RV32IBitPatternMatcher.hpp"
#include "modules/rv32i/RV32IControlFlowUnit.hpp"
#include "modules/rv32i/RV32IDecodeControlUnit.hpp"
#include "modules/rv32i/RV32IExecutionControlStatusUnit.hpp"
#include "modules/rv32i/RV32ISingleCycleCore.hpp"
#include "modules/rv32i/RV32ISingleCycleSystem.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Constant.hpp"
#include "modules/utility/Rewire.hpp"
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

std::vector<std::string> numberedTestIds(
    const std::string& prefix,
    const std::string& suffix,
    size_t first,
    size_t last) {
    std::vector<std::string> result;
    result.reserve(last - first + 1);
    for (size_t number = first; number <= last; ++number) {
        result.push_back(prefix + std::to_string(number) + suffix);
    }
    return result;
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
        1,
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
    const std::string& lower_level_description) {
    const auto contract_id = std::string(family.id());
    catalog.registerContract({
        contract_id,
        1,
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
    catalog.registerImplementation(implementationFor(
        family,
        implementation_prefix + ".behavioral.direct",
        Fidelity::Behavioral,
        verifiedBehavioral(
            lower_level_description,
            contract_test,
            contract_test,
            "four-state",
            "stable-after-settle")));
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
    addStructural<Decoder2to4>(catalog, "decode.2-to-4", "decode.2-to-4.structural", "2-to-4 decoder", "Decoder2to4Test");
    addStructural<Decoder5to32>(catalog, "decode.5-to-32", "decode.5-to-32.structural", "5-to-32 decoder", "Decoder5to32Test");

    addStructural<AND8>(catalog, "logic.and.width8", "logic.and.width8.structural", "8-bit AND", "AND8Test");
    addStructural<OR8>(catalog, "logic.or.width8", "logic.or.width8.structural", "8-bit OR", "OR8Test");
    addStructural<XOR8>(catalog, "logic.xor.width8", "logic.xor.width8.structural", "8-bit XOR", "XOR8Test");
    addStructural<NOT8>(catalog, "logic.not.width8", "logic.not.width8.structural", "8-bit NOT", "NOT8Test");
    addStructural<NAND8>(catalog, "logic.nand.width8", "logic.nand.width8.structural", "8-bit NAND", "NAND8Test");
    addStructural<NOR8>(catalog, "logic.nor.width8", "logic.nor.width8.structural", "8-bit NOR", "NOR8Test");

    addStructural<Mux2to1>(catalog, "mux.2x1.width1", "mux.2x1.width1.structural", "2:1 bit mux", "Mux2to1Test");
    addStructural<Mux4to1>(catalog, "mux.4x1.width1", "mux.4x1.width1.structural", "4:1 bit mux", "Mux4to1Test");
    addStructural<Mux8to1>(catalog, "mux.8x1.width1", "mux.8x1.width1.structural", "8:1 bit mux", "Mux8to1Test");
    addStructural<Mux16to1>(catalog, "mux.16x1.width1", "mux.16x1.width1.structural", "16:1 bit mux", "Mux16to1Test");
    addStructural<Mux32to1>(catalog, "mux.32x1.width1", "mux.32x1.width1.structural", "32:1 bit mux", "Mux32to1Test");
    addStructural<Mux2to1_4bit>(catalog, "mux.2x1.width4", "mux.2x1.width4.structural", "2:1 4-bit mux", "Mux2to1_4bitTest");
    addStructural<Mux2to1_8bit>(catalog, "mux.2x1.width8", "mux.2x1.width8.structural", "2:1 8-bit mux", "Mux2to1_8bitTest");
    addStructural<Mux2to1_32bit>(catalog, "mux.2x1.width32", "mux.2x1.width32.structural", "2:1 32-bit mux", "Mux2to1_32bitTest");
    addStructural<Mux4to1_8bit>(catalog, "mux.4x1.width8", "mux.4x1.width8.structural", "4:1 8-bit mux", "Mux4to1_8bitTest");
    addStructural<Mux4to1_32bit>(catalog, "mux.4x1.width32", "mux.4x1.width32.structural", "4:1 32-bit mux", "Mux4to1_32bitTest");
    addStructural<Mux8to1_8bit>(catalog, "mux.8x1.width8", "mux.8x1.width8.structural", "8:1 8-bit mux", "Mux8to1_8bitTest");
    addStructural<Mux8to1_32bit>(catalog, "mux.8x1.width32", "mux.8x1.width32.structural", "8:1 32-bit mux", "Mux8to1_32bitTest");
    addStructural<Mux16to1_8bit>(catalog, "mux.16x1.width8", "mux.16x1.width8.structural", "16:1 8-bit mux", "Mux16to1_8bitTest");
    addStructural<Mux32to1_32bit>(catalog, "mux.32x1.width32", "mux.32x1.width32.structural", "32:1 32-bit mux", "Mux32to1_32bitTest");

    addStructural<HalfAdder>(catalog, "arithmetic.half-adder", "arithmetic.half-adder.structural", "Half adder", "HalfAdderTest");
    addStructural<FullAdder>(catalog, "arithmetic.full-adder", "arithmetic.full-adder.structural", "Full adder", "FullAdderTest");
    addStructural<Adder8>(catalog, "arithmetic.adder.width8", "arithmetic.adder.width8.structural", "8-bit adder", "Adder8Test");
    addStructural<Adder32>(catalog, "arithmetic.adder.width32", "arithmetic.adder.width32.structural", "32-bit adder", "Adder32Test");
    addSelectableFamily(
        catalog,
        families::AddSub32,
        "arithmetic.add-sub.width32",
        "32-bit add/sub",
        "AddSub32Test",
        "AddSub32 structural full-adder chain");
    addStructural<TwosComplement8>(catalog, "arithmetic.twos-complement.width8", "arithmetic.twos-complement.width8.structural", "8-bit two's complement", "TwosComplement8Test");
    addStructural<Subtractor8>(catalog, "arithmetic.subtractor.width8", "arithmetic.subtractor.width8.structural", "8-bit subtractor", "Subtractor8Test");
    addStructural<SubtractorWithBorrow8>(catalog, "arithmetic.subtractor-borrow.width8", "arithmetic.subtractor-borrow.width8.structural", "8-bit subtractor with borrow", "SubtractorWithBorrow8Test");
    addStructural<Incrementer8>(catalog, "arithmetic.incrementer.width8", "arithmetic.incrementer.width8.structural", "8-bit incrementer", "Incrementer8Test");
    addStructural<Decrementer8>(catalog, "arithmetic.decrementer.width8", "arithmetic.decrementer.width8.structural", "8-bit decrementer", "Decrementer8Test");
    addStructural<EqualityChecker8>(catalog, "compare.equal.width8", "compare.equal.width8.structural", "8-bit equality", "EqualityChecker8Test");
    addStructural<Comparator8>(catalog, "compare.unsigned.width8", "compare.unsigned.width8.structural", "8-bit comparator", "Comparator8Test");
    addStructural<SignedComparator8>(catalog, "compare.signed.width8", "compare.signed.width8.structural", "8-bit signed comparator", "SignedComparator8Test");
    addSelectableFamily(
        catalog,
        families::Comparator32,
        "compare.width32",
        "32-bit comparator",
        "Comparator32Test",
        "Comparator32 structural subtract-and-detect network");
    addStructural<ZeroDetect8>(catalog, "compare.zero.width8", "compare.zero.width8.structural", "8-bit zero detect", "ZeroDetect8Test");
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
    addStructural<ShiftLeftLogical8>(catalog, "shift.left-logical.width8", "shift.left-logical.width8.structural", "8-bit shift left", "ShiftLeftLogical8Test");
    addStructural<ShiftRightLogical8>(catalog, "shift.right-logical.width8", "shift.right-logical.width8.structural", "8-bit shift right", "ShiftRightLogical8Test");
    addStructural<ShiftRightArithmetic8>(catalog, "shift.right-arithmetic.width8", "shift.right-arithmetic.width8.structural", "8-bit arithmetic shift", "ShiftRightArithmetic8Test");
    addSelectableFamily(
        catalog,
        families::Shifter32,
        "shift.barrel.width32",
        "32-bit shifter",
        "Shifter32Test",
        "Shifter32 structural mux network");
    addStructural<ALU8>(catalog, "alu.width8", "alu.width8.structural", "8-bit ALU", "ALU8Test");
}

void registerStorage(ComponentCatalog& catalog) {
    const auto memory_bit = std::string(families::MemoryBit.id());
    catalog.registerContract({
        memory_bit, 1, "Write-enabled memory bit",
        schemaFor(families::MemoryBit), {}, "four-state", "clock-boundary"});
    catalog.registerImplementation(implementationFor(
        families::MemoryBit,
        "memory.write-enabled-bit.structural.mux-dff",
        Fidelity::Structural, verifiedStructural("MemoryBitTest")));
    catalog.registerImplementation(implementationFor(
        families::MemoryBit,
        "memory.write-enabled-bit.behavioral.direct",
        Fidelity::Behavioral,
        verifiedBehavioral("MemoryBit structural mux/DFF", "MemoryBitTest",
                           "MemoryBitTest", "four-state", "clock-boundary")));

    const auto register32 = std::string(families::Register32.id());
    catalog.registerContract({
        register32, 1, "32-bit write-enabled register",
        schemaFor(families::Register32), {}, "four-state", "clock-boundary"});
    catalog.registerImplementation(implementationFor(
        families::Register32,
        "memory.register.width32.structural.bits",
        Fidelity::Structural, verifiedStructural("Register32Test")));
    catalog.registerImplementation(implementationFor(
        families::Register32,
        "memory.register.width32.behavioral.direct",
        Fidelity::Behavioral,
        verifiedBehavioral("Register32 structural bit cells", "Register32Test",
                           "Register32Test", "four-state", "clock-boundary")));

    addStructural<RegisterFile4x32>(catalog, "memory.register-file.4x32", "memory.register-file.4x32.structural", "4x32 register file", "RegisterFile4x32Test");

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

    addStructural<Memory4x32>(catalog, "memory.word-array.4x32", "memory.word-array.4x32.structural", "4x32 memory slice", "Memory4x32Test");
    addStructural<Memory32x32>(catalog, "memory.word-array.32x32", "memory.word-array.32x32.structural", "32x32 memory slice", "Memory32x32Test");

    const auto memory64 = std::string(families::Memory64Kx32.id());
    const auto memory64_evidence = verifiedBehavioralFromSlices(
            "Memory4x32 and Memory32x32 representative slices",
            "Memory64Kx32Test",
            {"Memory4x32Test", "Memory32x32Test"},
            "four-state", "clock-boundary");
    catalog.registerContract({
        memory64, 1, "RV32I 64Kx32 memory",
        schemaFor(families::Memory64Kx32), {},
        memory64_evidence.semantic_domain, memory64_evidence.observation});
    catalog.registerImplementation(implementationFor(
        families::Memory64Kx32,
        "rv32i.memory.64k-x32.behavioral.direct",
        Fidelity::Behavioral, memory64_evidence));
}

void registerRV32I(ComponentCatalog& catalog) {
    const auto alu32 = std::string(families::ALU32.id());
    catalog.registerContract({
        alu32, 1, "RV32I ALU32", schemaFor(families::ALU32), {},
        "known-binary", "stable-after-settle"});
    catalog.registerImplementation(implementationFor(
        families::ALU32,
        "rv32i.alu32.structural", Fidelity::Structural,
        verifiedStructural("ALU32Test")));
    catalog.registerImplementation(implementationFor(
        families::ALU32,
        "rv32i.alu32.behavioral.direct", Fidelity::Behavioral,
        verifiedBehavioral("ALU32 structural implementation", "ALU32Test",
                           "ALU32Test", "known-binary",
                           "stable-after-settle")));

    const auto control_flow = std::string(families::RV32IControlFlow.id());
    catalog.registerContract({
        control_flow, 1, "RV32I control flow",
        schemaFor(families::RV32IControlFlow), {}, "known-binary", "clock-boundary"});
    catalog.registerImplementation(implementationFor(
        families::RV32IControlFlow,
        "rv32i.control-flow.structural", Fidelity::Structural,
        verifiedStructural("RV32IControlFlowUnitTest")));
    catalog.registerImplementation(implementationFor(
        families::RV32IControlFlow,
        "rv32i.control-flow.behavioral.direct",
        Fidelity::Behavioral,
        verifiedBehavioral("RV32IControlFlowUnit structure",
                           "RV32IControlFlowUnitTest",
                           "RV32IControlFlowUnitTest", "known-binary",
                           "clock-boundary")));

    const auto decode_control =
        std::string(families::RV32IDecodeControl.id());
    catalog.registerContract({
        decode_control, 1, "RV32I decode and control",
        schemaFor(families::RV32IDecodeControl), {}, "known-binary",
        "stable-after-settle"});
    catalog.registerImplementation(implementationFor(
        families::RV32IDecodeControl,
        "rv32i.decode-control.structural",
        Fidelity::Structural, verifiedStructural("RV32IDecodeControlUnitTest")));
    catalog.registerImplementation(implementationFor(
        families::RV32IDecodeControl,
        "rv32i.decode-control.behavioral.direct",
        Fidelity::Behavioral,
        verifiedBehavioral("RV32IDecodeControlUnit structure",
                           "RV32IDecodeControlUnitTest",
                           "RV32IDecodeControlUnitTest", "known-binary",
                           "stable-after-settle")));

    const auto execution_status =
        std::string(families::RV32IExecutionStatus.id());
    catalog.registerContract({
        execution_status, 1, "RV32I execution status",
        schemaFor(families::RV32IExecutionStatus), {}, "known-binary",
        "clock-boundary"});
    catalog.registerImplementation(implementationFor(
        families::RV32IExecutionStatus,
        "rv32i.execution-status.structural",
        Fidelity::Structural,
        verifiedStructural("RV32IExecutionControlStatusUnitTest")));
    catalog.registerImplementation(implementationFor(
        families::RV32IExecutionStatus,
        "rv32i.execution-status.behavioral.direct",
        Fidelity::Behavioral,
        verifiedBehavioral("RV32IExecutionControlStatusUnit structure",
                           "RV32IExecutionControlStatusUnitTest",
                           "RV32IExecutionControlStatusUnitTest", "known-binary",
                           "clock-boundary")));

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

    const auto core =
        std::string(families::RV32ISingleCycleCore.id());
    catalog.registerContract({
        core, 1, "Educational RV32I single-cycle core",
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
    for (size_t program = 1; program <= 16; ++program) {
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
    catalog->freeze();
    return catalog;
}

const ComponentCatalog& builtinComponentCatalog() {
    static const auto catalog = createBuiltinComponentCatalog();
    return *catalog;
}

} // namespace circuit
