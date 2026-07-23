#include "tests/ComponentSelectionTests.hpp"

#include "components/BasicComponent.hpp"
#include "components/Component.hpp"
#include "components/IOComponent.hpp"
#include "components/selection/BuildProfile.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/ComponentCatalog.hpp"
#include "components/selection/ProfileGenerator.hpp"
#include "modules/memory/Register32BitCellArray.hpp"
#include "modules/memory/MemoryBit.hpp"
#include "modules/memory/Register32.hpp"
#include "basic/Wire.hpp"
#include "simulator/Event.hpp"
#include "simulator/Simulator.hpp"
#include "tests/TestRegistry.hpp"
#include <algorithm>
#include <array>
#include <set>
#include <stdexcept>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

circuit::ContractDescriptor memoryBitContract(std::string id = "memory.write-enabled-bit") {
    return {
        std::move(id),
        1,
        "Write-enabled memory bit",
        {
            {"D", 1, PinType::INPUT},
            {"WE", 1, PinType::INPUT},
            {"CLK", 1, PinType::INPUT},
            {"RST", 1, PinType::INPUT},
            {"Q", 1, PinType::OUTPUT},
        },
        {},
        "four-state",
        "clock-boundary",
    };
}

circuit::VerificationEvidence structuralEvidence() {
    return {
        circuit::VerificationStatus::Verified,
        {"gate-level implementation"},
        {"MemoryBitStructuralContractTest"},
        {},
        {},
        "four-state",
        "clock-boundary",
    };
}

circuit::VerificationEvidence behavioralEvidence() {
    return {
        circuit::VerificationStatus::Verified,
        {"MemoryBit mux and DFF structure"},
        {"MemoryBitBehavioralContractTest"},
        {"MemoryBitEquivalenceTest"},
        {},
        "four-state",
        "clock-boundary",
    };
}

circuit::ImplementationDescriptor structuralMemoryBit() {
    circuit::ImplementationDescriptor descriptor;
    descriptor.id = "memory.write-enabled-bit.structural.mux-dff";
    descriptor.contract_id = "memory.write-enabled-bit";
    descriptor.fidelity = circuit::Fidelity::Structural;
    descriptor.default_priority = 100;
    descriptor.concrete_type_name = MemoryBit::TypeName;
    descriptor.evidence = structuralEvidence();
    descriptor.factory = [](
        const std::string& name,
        const circuit::ParameterMap&,
        const std::shared_ptr<circuit::BuildContext>& context) {
        return Component::createWithContext<MemoryBit>(context, name);
    };
    return descriptor;
}

circuit::ImplementationDescriptor behavioralMemoryBit() {
    circuit::ImplementationDescriptor descriptor;
    descriptor.id = "memory.write-enabled-bit.behavioral.direct";
    descriptor.contract_id = "memory.write-enabled-bit";
    descriptor.fidelity = circuit::Fidelity::Behavioral;
    descriptor.default_priority = 10;
    descriptor.concrete_type_name =
        std::string(circuit::families::MemoryBit.typeName());
    descriptor.evidence = behavioralEvidence();
    descriptor.factory = [](
        const std::string& name,
        const circuit::ParameterMap&,
        const std::shared_ptr<circuit::BuildContext>& context) {
        return circuit::families::MemoryBit.create(
            circuit::Fidelity::Behavioral, name, context);
    };
    return descriptor;
}

circuit::ComponentCatalog memoryBitCatalog() {
    circuit::ComponentCatalog catalog;
    catalog.registerContract(memoryBitContract());
    catalog.registerImplementation(structuralMemoryBit());
    catalog.registerImplementation(behavioralMemoryBit());
    catalog.freeze();
    return catalog;
}

template<typename Exception, typename Fn>
void requireThrows(Fn&& fn, const std::string& message) {
    try {
        fn();
    } catch (const Exception&) {
        return;
    }
    throw std::runtime_error(message);
}

void scheduleBit(Simulator& simulator, size_t time,
                 const std::shared_ptr<Wire<>>& wire, LogicValue value) {
    simulator.scheduleEvent(std::make_shared<WireUpdateEvent<>>(time, wire, value));
}

void scheduleBit(Simulator& simulator, size_t time,
                 const std::shared_ptr<Wire<>>& wire, bool value) {
    scheduleBit(simulator, time, wire,
                value ? LogicValue::HIGH : LogicValue::LOW);
}

std::vector<LogicValue> bits32(uint32_t value) {
    std::vector<LogicValue> bits(32, LogicValue::LOW);
    for (size_t bit = 0; bit < bits.size(); ++bit) {
        bits[bit] = ((value >> bit) & 1U) != 0
            ? LogicValue::HIGH
            : LogicValue::LOW;
    }
    return bits;
}

std::shared_ptr<IOComponent> createBuiltinFamilyDut(
    const circuit::ComponentFamily& family,
    circuit::Fidelity fidelity) {
    auto profile = circuit::BuildProfileBuilder(
        "equivalence-" + circuit::toString(fidelity))
        .addRule(circuit::preferFidelity(
            fidelity,
            circuit::ProfileSelector::exactPath("DUT"),
            "equivalence test selects one family fidelity"))
        .build();
    auto root = circuit::builtinComponentCatalog()
        .createRoot(family.request("DUT"), std::move(profile))
        .root;
    require(root->getSelectedFidelity() == circuit::toString(fidelity),
            "Equivalence test did not instantiate requested fidelity");
    auto io = std::dynamic_pointer_cast<IOComponent>(root);
    require(io != nullptr, "Equivalence family root is not an IOComponent");
    return io;
}

std::vector<LogicValue> runMemoryBitTrace(circuit::Fidelity fidelity) {
    Simulator simulator;
    auto component = createBuiltinFamilyDut(
        circuit::families::MemoryBit, fidelity);
    ComponentBuilder builder(component);
    auto d = builder.addNewWire("D_IN", nullptr, {component->getInputPin("D")});
    auto we = builder.addNewWire("WE_IN", nullptr, {component->getInputPin("WE")});
    auto clk = builder.addNewWire("CLK_IN", nullptr, {component->getInputPin("CLK")});
    auto rst = builder.addNewWire("RST_IN", nullptr, {component->getInputPin("RST")});
    auto q = builder.addNewWire("Q_OUT", component->getOutputPin("Q"), {});

    scheduleBit(simulator, 0, d, false);
    scheduleBit(simulator, 0, we, false);
    scheduleBit(simulator, 0, clk, false);
    scheduleBit(simulator, 0, rst, true);
    scheduleBit(simulator, 30, rst, false);
    scheduleBit(simulator, 40, d, true);
    scheduleBit(simulator, 60, clk, true);
    scheduleBit(simulator, 100, clk, false);
    scheduleBit(simulator, 110, we, true);
    scheduleBit(simulator, 130, clk, true);
    scheduleBit(simulator, 180, d, false);
    scheduleBit(simulator, 220, clk, false);
    scheduleBit(simulator, 260, clk, true);
    scheduleBit(simulator, 310, clk, false);
    scheduleBit(simulator, 320, we, false);
    scheduleBit(simulator, 330, d, true);
    scheduleBit(simulator, 360, clk, true);
    scheduleBit(simulator, 410, clk, false);
    scheduleBit(simulator, 420, we, true);
    scheduleBit(simulator, 430, d, LogicValue::UNKNOWN);
    scheduleBit(simulator, 460, clk, true);
    scheduleBit(simulator, 510, clk, false);
    scheduleBit(simulator, 520, rst, true);
    scheduleBit(simulator, 560, rst, false);
    scheduleBit(simulator, 570, d, true);
    scheduleBit(simulator, 580, we, LogicValue::UNKNOWN);
    scheduleBit(simulator, 610, clk, true);

    SimulationTest::scheduleInitialEventsForTree(component, simulator);
    simulator.runAndRecord(660);

    const std::array<size_t, 9> observations{20, 95, 170, 210, 300, 400, 500, 550, 650};
    std::vector<LogicValue> trace;
    trace.reserve(observations.size());
    for (const auto time : observations) {
        simulator.setCircuitStateAtTime(time);
        trace.push_back(q->getSingleValue());
    }
    return trace;
}

std::vector<std::vector<LogicValue>> runRegisterTrace(
    circuit::Fidelity fidelity) {
    Simulator simulator;
    auto component = createBuiltinFamilyDut(
        circuit::families::Register32, fidelity);
    ComponentBuilder builder(component);
    auto d = builder.addNewWire<32>(
        "D_IN", nullptr, {component->getInputPin<32>("D")});
    auto we = builder.addNewWire("WE_IN", nullptr, {component->getInputPin("WE")});
    auto clk = builder.addNewWire("CLK_IN", nullptr, {component->getInputPin("CLK")});
    auto rst = builder.addNewWire("RST_IN", nullptr, {component->getInputPin("RST")});
    auto q = builder.addNewWire<32>(
        "Q_OUT", component->getOutputPin<32>("Q"), {});

    simulator.scheduleEvent(std::make_shared<WireUpdateEvent<32>>(0, d, uint64_t{0}));
    scheduleBit(simulator, 0, we, false);
    scheduleBit(simulator, 0, clk, false);
    scheduleBit(simulator, 0, rst, true);
    scheduleBit(simulator, 60, rst, false);
    simulator.scheduleEvent(std::make_shared<WireUpdateEvent<32>>(100, d, uint64_t{0xffffffffU}));
    scheduleBit(simulator, 130, clk, true);
    scheduleBit(simulator, 180, clk, false);
    scheduleBit(simulator, 200, we, true);
    scheduleBit(simulator, 230, clk, true);
    scheduleBit(simulator, 300, clk, false);
    simulator.scheduleEvent(std::make_shared<WireUpdateEvent<32>>(320, d, uint64_t{0xaaaaaaaaU}));
    scheduleBit(simulator, 360, clk, true);
    scheduleBit(simulator, 420, clk, false);
    auto unknown = bits32(0x12345678U);
    unknown[13] = LogicValue::UNKNOWN;
    simulator.scheduleEvent(std::make_shared<WireUpdateEvent<32>>(430, d, unknown));
    scheduleBit(simulator, 460, clk, true);
    scheduleBit(simulator, 530, clk, false);
    scheduleBit(simulator, 550, rst, true);

    SimulationTest::scheduleInitialEventsForTree(component, simulator);
    simulator.runAndRecord(620);

    const std::array<size_t, 6> observations{90, 170, 280, 410, 510, 600};
    std::vector<std::vector<LogicValue>> trace;
    trace.reserve(observations.size());
    for (const auto time : observations) {
        simulator.setCircuitStateAtTime(time);
        trace.push_back(q->getValueVector());
    }
    return trace;
}

} // namespace

std::string BuildProfileTest::getTestName() const {
    return "BuildProfileTest";
}

void BuildProfileTest::verifyResults() {
    auto catalog = memoryBitCatalog();
    const circuit::ComponentBuildRequest root{
        "memory.write-enabled-bit", "memory", {}, {}, "four-state", "clock-boundary"};

    auto strict_a = circuit::strictAllStructural()->generate(catalog, root);
    auto strict_b = circuit::strictAllStructural()->generate(catalog, root);
    require(strict_a.serialize() == strict_b.serialize(),
            "Strict structural generator must be deterministic");
    require(strict_a.fingerprint() == strict_b.fingerprint(),
            "Equivalent generated profiles must have equal fingerprints");

    auto depth = circuit::structuralThroughDepth(2)->generate(catalog, root);
    require(depth.decide("system", 0, root.contract_id, {}).fidelity
                == circuit::Fidelity::Structural,
            "Depth profile root must be structural");
    require(depth.decide("system.core.alu", 2, root.contract_id, {}).fidelity
                == circuit::Fidelity::Structural,
            "Depth profile boundary must remain structural");
    require(depth.decide("system.core.alu.cell", 3, root.contract_id, {}).fidelity
                == circuit::Fidelity::Behavioral,
            "Depth profile must become behavioral below boundary");

    auto override_rule = circuit::preferFidelity(
        circuit::Fidelity::Behavioral,
        circuit::ProfileSelector::exactPath("system.core.alu"),
        "focused behavioral experiment");
    auto overridden = circuit::withOverrides(
        circuit::strictAllStructural(), {override_rule}, "manual-override")
        ->generate(catalog, root);
    require(overridden.decide("system.core.alu", 2, root.contract_id, {}).fidelity
                == circuit::Fidelity::Behavioral,
            "Exact override must beat general generator rule");

    auto handwritten = circuit::BuildProfileBuilder("handwritten")
        .addRule(circuit::preferFidelity(
            circuit::Fidelity::Structural, circuit::ProfileSelector::any(),
            "handwritten default"))
        .addRule(circuit::preferFidelity(
            circuit::Fidelity::Behavioral,
            circuit::ProfileSelector::exactPath("system.data_memory"),
            "handwritten memory override"))
        .unavailablePolicy(circuit::UnavailableFidelityPolicy::Error)
        .build();
    require(handwritten.decide("system.core", 1, root.contract_id, {}).fidelity
                == circuit::Fidelity::Structural,
            "Handwritten profile default failed");
    require(handwritten.decide("system.data_memory", 1, root.contract_id, {}).fidelity
                == circuit::Fidelity::Behavioral,
            "Handwritten exact override failed");
    require(handwritten.generatorDescriptor() == "handwritten",
            "Handwritten profile must preserve its origin descriptor");
}

std::string ComponentCatalogSelectionTest::getTestName() const {
    return "ComponentCatalogSelectionTest";
}

void ComponentCatalogSelectionTest::verifyResults() {
    auto catalog = memoryBitCatalog();
    const circuit::ComponentBuildRequest root{
        "memory.write-enabled-bit", "memory", {}, {}, "four-state", "clock-boundary"};

    auto structural_profile = circuit::strictAllStructural()->generate(catalog, root);
    auto structural = catalog.createRoot(root, structural_profile);
    require(std::dynamic_pointer_cast<MemoryBit>(structural.root) != nullptr,
            "Structural profile did not select MemoryBit");
    require(structural.root->getSelectedFidelity() == "structural",
            "Structural instance metadata is incorrect");
    require(structural.root->getContractId() == root.contract_id,
            "Structural instance contract metadata is incorrect");
    require(structural.manifest->entries().size() > 1,
            "Structural manifest must include recursively constructed children");
    const auto structural_manifest = structural.manifest->serialize();
    require(structural_manifest.find("memory.WRITE_MUX") != std::string::npos,
            "Structural manifest omitted explicit child topology");

    auto behavioral_profile = circuit::strictAllBehavioral()->generate(catalog, root);
    auto behavioral = catalog.createRoot(root, behavioral_profile);
    require(behavioral.root->getTypeName()
                == std::string(circuit::families::MemoryBit.typeName()),
            "Behavioral profile changed the public MemoryBit type");
    require(std::dynamic_pointer_cast<BasicComponent>(behavioral.root) != nullptr,
            "Behavioral profile did not select a direct evaluator");
    require(behavioral.root->getSelectedFidelity() == "behavioral",
            "Behavioral instance metadata is incorrect");
    require(behavioral.root->getChildren().empty(),
            "Behavioral implementation must be a black box at its contract boundary");
    require(behavioral.manifest->entries().size() == 1,
            "Behavioral manifest should contain one direct implementation node");

    circuit::ComponentCatalog behavioral_only;
    behavioral_only.registerContract(memoryBitContract("memory.behavioral-only"));
    auto only = behavioralMemoryBit();
    only.id = "memory.behavioral-only.direct";
    only.contract_id = "memory.behavioral-only";
    behavioral_only.registerImplementation(std::move(only));
    behavioral_only.freeze();
    const circuit::ComponentBuildRequest only_request{
        "memory.behavioral-only", "only", {}, {}, "four-state", "clock-boundary"};
    requireThrows<std::runtime_error>([&] {
        auto profile = circuit::strictAllStructural()->generate(
            behavioral_only, only_request);
        (void)behavioral_only.createRoot(only_request, profile);
    }, "Strict structural profile must fail when structural fidelity is unavailable");

    auto maximum = circuit::maximallyStructural()->generate(
        behavioral_only, only_request);
    auto fallback = behavioral_only.createRoot(only_request, maximum);
    require(fallback.root->getSelectedFidelity() == "behavioral",
            "Maximally structural profile did not use the only available implementation");
    require(fallback.root->getInstanceMetadata()->used_unavailable_exception,
            "Unavailable-fidelity fallback was not recorded");

    circuit::ComponentCatalog invalid;
    invalid.registerContract({
        "register.invalid-behavioral", 1, "Invalid behavioral register", {}, {},
        "four-state", "clock-boundary"});
    circuit::ImplementationDescriptor invalid_descriptor;
    invalid_descriptor.id = "register.invalid-behavioral.composed";
    invalid_descriptor.contract_id = "register.invalid-behavioral";
    invalid_descriptor.fidelity = circuit::Fidelity::Behavioral;
    invalid_descriptor.evidence = behavioralEvidence();
    invalid_descriptor.factory = [](
        const std::string& name,
        const circuit::ParameterMap&,
        const std::shared_ptr<circuit::BuildContext>& context) {
        return Component::createWithContext<Register32BitCellArray>(context, name);
    };
    invalid.registerImplementation(std::move(invalid_descriptor));
    invalid.freeze();
    const circuit::ComponentBuildRequest invalid_request{
        "register.invalid-behavioral", "invalid", {}, {}, {}, {}};
    requireThrows<std::runtime_error>([&] {
        auto profile = circuit::strictAllBehavioral()->generate(invalid, invalid_request);
        (void)invalid.createRoot(invalid_request, profile);
    }, "Catalog must reject a composed class registered as behavioral fidelity");
}

std::string BuiltinComponentCatalogInventoryTest::getTestName() const {
    return "BuiltinComponentCatalogInventoryTest";
}

void BuiltinComponentCatalogInventoryTest::verifyResults() {
    const auto catalog = circuit::createBuiltinComponentCatalog();
    require(catalog->frozen(), "Built-in catalog must be immutable before use");

    std::set<std::string> registered_types;
    const auto registered_test_names = getRegisteredTestNames();
    const std::set<std::string> registered_tests(
        registered_test_names.begin(), registered_test_names.end());
    size_t implementation_count = 0;
    for (const auto& contract : catalog->contracts()) {
        const auto implementations = catalog->implementationsFor(contract.id);
        require(!implementations.empty(),
                "Built-in contract lacks an implementation: " + contract.id);
        implementation_count += implementations.size();
        for (const auto& implementation : implementations) {
            require(!implementation.concrete_type_name.empty(),
                    "Built-in implementation lacks its concrete type name: "
                        + implementation.id);
            registered_types.insert(implementation.concrete_type_name);

            const auto requireRegisteredEvidence = [&](const auto& evidence_ids,
                                                       const char* evidence_kind) {
                for (const auto& test_id : evidence_ids) {
                    require(registered_tests.count(test_id) != 0,
                            "Built-in implementation '" + implementation.id
                                + "' names unknown " + evidence_kind + " '"
                                + test_id + "'");
                }
            };
            requireRegisteredEvidence(implementation.evidence.contract_tests,
                                      "contract test");
            requireRegisteredEvidence(implementation.evidence.equivalence_tests,
                                      "equivalence test");
            requireRegisteredEvidence(implementation.evidence.representative_tests,
                                      "representative test");

            if (implementation.fidelity == circuit::Fidelity::Behavioral
                && !implementation.reference_only) {
                require(!implementation.evidence.equivalence_tests.empty()
                            || !implementation.evidence.representative_tests.empty(),
                        "Behavioral implementation lacks equivalence or representative evidence: "
                            + implementation.id);
            }
        }
    }

    const std::set<std::string> expected_types{
        "ALU32", "ALU8", "AND8", "ANDGate", "AddSub32", "Adder32",
        "Adder8", "Memory64Kx32", "RV32IReferenceCore",
        "Register32", "RegisterFile32x32",
        "BitJoiner", "BitSplitter",
        "ClockGenerator", "Comparator32", "Comparator8", "ConstantValue",
        "DFlipFlop", "Decoder2to4", "Decoder5to32", "Decrementer8",
        "EqualityChecker8", "FullAdder",
        "GatedDLatch", "HalfAdder", "Incrementer8", "Logic32",
        "Memory32x32", "Memory4x32", "MemoryBit", "Mux16to1",
        "Mux16to1_8bit", "Mux2to1", "Mux2to1_32bit", "Mux2to1_4bit",
        "Mux2to1_8bit", "Mux32to1", "Mux32to1_32bit", "Mux4to1",
        "Mux4to1_32bit", "Mux4to1_8bit", "Mux8to1", "Mux8to1_32bit",
        "Mux8to1_8bit", "NAND8", "NANDGate", "NOR8", "NORGate", "NOT8",
        "NOTGate", "OR8", "ORGate", "RV32IBitPatternMatcher",
        "RV32IControlFlowUnit", "RV32IDecodeControlUnit",
        "RV32IExecutionControlStatusUnit", "RV32ISingleCycleCore",
        "RV32ISingleCycleSystem", "RV32IReferenceSystem",
        "RegisterFile4x32", "Rewire", "SRLatch",
        "ShiftLeftLogical8", "ShiftRightArithmetic8", "ShiftRightLogical8",
        "Shifter32", "SignedComparator8", "Subtractor8",
        "SubtractorWithBorrow8", "TwosComplement8", "XOR8", "XORGate",
        "ZeroDetect32", "ZeroDetect8"
    };

    require(registered_types == expected_types,
            "Built-in catalog does not cover every production module TypeName");
    require(implementation_count >= expected_types.size(),
            "Built-in catalog unexpectedly lost implementation variants");
}

std::string RecursiveBuildProfileTest::getTestName() const {
    return "RecursiveBuildProfileTest";
}

void RecursiveBuildProfileTest::verifyResults() {
    const auto catalog = circuit::createBuiltinComponentCatalog();
    const circuit::ComponentBuildRequest request{
        "rv32i.core.educational-single-cycle", "core", {}, {}, {}, {}};
    auto profile = circuit::structuralThroughDepth(
        0, circuit::UnavailableFidelityPolicy::UseOnlyAvailableAndRecordException)
        ->generate(*catalog, request);
    const auto result = catalog->createRoot(request, std::move(profile));

    require(result.root->getSelectedFidelity() == "structural",
            "Depth-zero profile must keep the RV32I core structural");
    const std::map<std::string, std::string> expected_children{
        {"CONTROL_FLOW", "RV32IControlFlowUnit"},
        {"DECODE_CONTROL", "RV32IDecodeControlUnit"},
        {"REGISTER_FILE", "RegisterFile32x32"},
        {"ALU", "ALU32"},
        {"EXECUTION_STATUS", "RV32IExecutionControlStatusUnit"},
    };
    for (const auto& [name, type] : expected_children) {
        const auto found = std::find_if(
            result.root->getChildren().begin(), result.root->getChildren().end(),
            [&](const auto& child) { return child->getName() == name; });
        require(found != result.root->getChildren().end(),
                "Profile-built RV32I core omitted child " + name);
        require((*found)->getTypeName() == type,
                "Profile selected wrong implementation for " + name);
        require((*found)->getSelectedFidelity() == "behavioral",
                "Selected RV32I child lacks behavioral instance metadata");
        require((*found)->getChildren().empty(),
                "Behavioral RV32I child must remain a black box");
    }

    const auto entries = result.manifest->entries();
    const auto root_entry = std::find_if(entries.begin(), entries.end(),
        [](const auto& entry) { return entry.path == "core"; });
    require(root_entry != entries.end(), "Build manifest omitted RV32I root");
    require(root_entry->effective_fidelity == circuit::EffectiveFidelity::Mixed,
            "RV32I root manifest must report its mixed effective fidelity");

    auto strict_profile = circuit::strictAllStructural()->generate(*catalog, request);
    const auto strict = catalog->createRoot(request, std::move(strict_profile));
    std::vector<std::shared_ptr<Component>> pending{strict.root};
    size_t visited = 0;
    while (!pending.empty()) {
        auto component = std::move(pending.back());
        pending.pop_back();
        ++visited;
        require(component->getSelectedFidelity() != "behavioral",
                "Strict structural RV32I core contains a behaviorally selected "
                "descendant at " + component->getID());
        pending.insert(pending.end(), component->getChildren().begin(),
                       component->getChildren().end());
    }
    require(visited > result.root->getChildren().size(),
            "Strict structural RV32I profile did not expand its lower-level topology");
    const auto strict_entries = strict.manifest->entries();
    const auto strict_root = std::find_if(strict_entries.begin(), strict_entries.end(),
        [](const auto& entry) { return entry.path == "core"; });
    require(strict_root != strict_entries.end()
                && strict_root->effective_fidelity
                    == circuit::EffectiveFidelity::Structural,
            "Strict structural RV32I manifest must report a structural root subtree");
}

std::string MemoryBitEquivalenceTest::getTestName() const {
    return "MemoryBitEquivalenceTest";
}

void MemoryBitEquivalenceTest::verifyResults() {
    const auto structural =
        runMemoryBitTrace(circuit::Fidelity::Structural);
    const auto behavioral =
        runMemoryBitTrace(circuit::Fidelity::Behavioral);
    const std::vector<LogicValue> expected{
        LogicValue::LOW,
        LogicValue::LOW,
        LogicValue::HIGH,
        LogicValue::HIGH,
        LogicValue::LOW,
        LogicValue::LOW,
        LogicValue::UNKNOWN,
        LogicValue::LOW,
        LogicValue::UNKNOWN,
    };
    require(structural == expected,
            "Structural MemoryBit trace does not satisfy the independent contract");
    require(behavioral == expected,
            "Behavioral MemoryBit trace does not satisfy the independent contract");
    require(structural == behavioral,
            "MemoryBit implementations differ at stable contract observations");
}

std::string Register32EquivalenceTest::getTestName() const {
    return "Register32EquivalenceTest";
}

void Register32EquivalenceTest::verifyResults() {
    const auto structural =
        runRegisterTrace(circuit::Fidelity::Structural);
    const auto behavioral =
        runRegisterTrace(circuit::Fidelity::Behavioral);
    auto unknown = bits32(0x12345678U);
    unknown[13] = LogicValue::UNKNOWN;
    const std::vector<std::vector<LogicValue>> expected{
        bits32(0x00000000U),
        bits32(0x00000000U),
        bits32(0xffffffffU),
        bits32(0xaaaaaaaaU),
        unknown,
        bits32(0x00000000U),
    };
    require(structural == expected,
            "Structural Register32 trace does not satisfy the independent contract");
    require(behavioral == expected,
            "Behavioral Register32 trace does not satisfy the independent contract");
    require(structural == behavioral,
            "Register32 implementations differ at stable contract observations");
}
