#include "tests/ComponentSelectionTests.hpp"
#include "tests/ComponentTestModel.hpp"

#include "components/BasicComponent.hpp"
#include "components/Component.hpp"
#include "components/IOComponent.hpp"
#include "components/selection/BuildProfile.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/ComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "modules/composite/ALU32.hpp"
#include "modules/composite/HalfAdder.hpp"
#include "modules/memory/Memory64Kx32.hpp"
#include "modules/memory/MemoryBit.hpp"
#include "modules/memory/Register32.hpp"
#include "modules/memory/RegisterFile32x32.hpp"
#include "modules/rv32i/RV32IBuildProfiles.hpp"
#include "modules/rv32i/RV32ISingleCycleSystem.hpp"
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
        {"MemoryBitTest"},
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
        {"MemoryBitTest"},
        {"MemoryBitTest"},
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

    auto strict_a = circuit::strictAllStructural();
    auto strict_b = circuit::strictAllStructural();
    require(strict_a.serialize() == strict_b.serialize(),
            "Strict structural generator must be deterministic");
    require(strict_a.fingerprint() == strict_b.fingerprint(),
            "Equivalent generated profiles must have equal fingerprints");

    auto depth = circuit::structuralThroughDepth(2);
    require(depth.decide("system", 0, root.contract_id, {}).fidelity
                == circuit::Fidelity::Structural,
            "Depth profile root must be structural");
    require(depth.decide("system.core.alu", 2, root.contract_id, {}).fidelity
                == circuit::Fidelity::Structural,
            "Depth profile boundary must remain structural");
    require(depth.decide("system.core.alu.cell", 3, root.contract_id, {}).fidelity
                == circuit::Fidelity::Behavioral,
            "Depth profile must become behavioral below boundary");

    auto subtree =
        circuit::BuildProfileBuilder("subtree")
            .addRule(circuit::preferFidelity(
                circuit::Fidelity::Behavioral,
                circuit::ProfileSelector::subtree("system.core.alu"),
                "behavioral ALU subtree"))
            .addRule(circuit::preferFidelity(
                circuit::Fidelity::Structural,
                circuit::ProfileSelector::exactPath("system.core.alu"),
                "keep ALU root structural"))
            .build();
    require(
        subtree.decide(
            "system.core.alu", 2, root.contract_id, {}).fidelity
                == circuit::Fidelity::Structural,
        "Later exact rule must keep the subtree root structural");
    require(
        subtree.decide(
            "system.core.alu.add", 3, root.contract_id, {}).fidelity
                == circuit::Fidelity::Behavioral,
        "Subtree rule did not match a direct child");
    require(
        subtree.decide(
            "system.core.alu.add.fa0", 4, root.contract_id, {}).fidelity
                == circuit::Fidelity::Behavioral,
        "Subtree rule did not match a deeper child");

    auto override_rule = circuit::preferFidelity(
        circuit::Fidelity::Behavioral,
        circuit::ProfileSelector::exactPath("system.core.alu"),
        "focused behavioral experiment");
    auto overridden = circuit::withProfileOverrides(
        circuit::strictAllStructural(), {override_rule}, "manual-override");
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
    require(handwritten.name() == "handwritten",
            "Handwritten profile must preserve its name");
}

std::string ComponentCatalogSelectionTest::getTestName() const {
    return "ComponentCatalogSelectionTest";
}

void ComponentCatalogSelectionTest::verifyResults() {
    auto catalog = memoryBitCatalog();
    const circuit::ComponentBuildRequest root{
        "memory.write-enabled-bit", "memory", {}, {}, "four-state", "clock-boundary"};

    auto structural_profile = circuit::strictAllStructural();
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

    auto behavioral_profile = circuit::strictAllBehavioral();
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
        auto profile = circuit::strictAllStructural();
        (void)behavioral_only.createRoot(only_request, profile);
    }, "Strict structural profile must fail when structural fidelity is unavailable");

    auto maximum = circuit::maximallyStructural();
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
        return Component::createWithContext<HalfAdder>(context, name);
    };
    invalid.registerImplementation(std::move(invalid_descriptor));
    invalid.freeze();
    const circuit::ComponentBuildRequest invalid_request{
        "register.invalid-behavioral", "invalid", {}, {}, {}, {}};
    requireThrows<std::runtime_error>([&] {
        auto profile = circuit::strictAllBehavioral();
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
    std::map<std::string, std::set<std::string>>
        logical_tests_by_contract;
    for (const auto& test : getTestRegistry()) {
        require(
            !test.logical_test_id.empty(),
            "Registered test lacks a logical test ID: "
                + test.name);
        require(
            !test.labels.empty(),
            "Registered test lacks labels: " + test.name);
        for (const auto& contract_id : test.contract_ids) {
            logical_tests_by_contract[contract_id].insert(
                test.logical_test_id);
        }
    }
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

            if (implementation.fidelity == circuit::Fidelity::Behavioral) {
                require(!implementation.evidence.equivalence_tests.empty()
                            || !implementation.evidence.representative_tests.empty(),
                        "Behavioral implementation lacks equivalence or representative evidence: "
                            + implementation.id);
            }
        }

        const auto owners =
            logical_tests_by_contract.find(contract.id);
        require(
            owners != logical_tests_by_contract.end(),
            "Built-in contract lacks a logical component test: "
                + contract.id);
        require(
            owners->second.size() == 1,
            "Built-in contract has multiple logical component tests: "
                + contract.id);
    }

    for (const auto& [contract_id, owners] :
         logical_tests_by_contract) {
        require(
            catalog->hasContract(contract_id),
            "Test registry names an unknown component contract: "
                + contract_id);
        require(
            owners.size() == 1,
            "Component contract has ambiguous logical ownership: "
                + contract_id);
    }

    const std::set<std::string> expected_types{
        "ALU32", "ALU8", "AND8", "ANDGate", "AddSub32", "Adder32",
        "Adder8", "Memory64Kx32",
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
        "RV32ISingleCycleSystem", "RV32IFiveStageCore",
        "RV32IIFIDPipelineRegister", "RV32IIDEXPipelineRegister",
        "RV32IEXMEMPipelineRegister", "RV32IMEMWBPipelineRegister",
        "RV32IForwardingUnit", "RV32IHazardDetectionUnit",
        "RV32IPipelineControlFlowUnit", "RV32IMemoryAlignmentUnit",
        "RV32IPipelineRetirementUnit", "RV32IPipelineCoordinator",
        "RV32IFetchStage", "RV32IDecodeStage", "RV32IExecuteStage",
        "RV32IMemoryStage", "RV32IWritebackStage",
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
    const auto architecture_profile =
        rv32i::architectureStructuralProfile(
            *catalog, "rv32i-architecture-profile-test");
    const std::set<std::string> behavioral_memory_contracts{
        std::string(circuit::families::Memory64Kx32.id()),
        std::string(circuit::families::RegisterFile32x32.id()),
        std::string(circuit::families::Register32.id()),
        std::string(circuit::families::MemoryBit.id()),
    };
    size_t rv32i_contracts = 0;
    size_t reusable_contracts = 0;
    for (const auto& contract : catalog->contracts()) {
        const auto decision = architecture_profile.decide(
            "probe", 0, contract.id, {});
        require(
            decision.fidelity.has_value(),
            "RV32I architecture profile left a contract undecided: "
                + contract.id);
        if (behavioral_memory_contracts.contains(contract.id)) {
            require(
                *decision.fidelity
                    == circuit::Fidelity::Behavioral,
                "RV32I architecture profile did not compact memory "
                "contract " + contract.id);
        } else if (contract.id.starts_with("rv32i.")) {
            ++rv32i_contracts;
            require(
                *decision.fidelity
                    == circuit::Fidelity::Structural,
                "RV32I architecture profile did not preserve "
                "structural contract " + contract.id);
        } else {
            ++reusable_contracts;
            require(
                *decision.fidelity
                    == circuit::Fidelity::Behavioral,
                "RV32I architecture profile did not compact reusable "
                "contract " + contract.id);
        }
    }
    require(
        rv32i_contracts > 0 && reusable_contracts > 0,
        "RV32I architecture profile audit lacked both contract groups");

    const circuit::ComponentBuildRequest request{
        "rv32i.core.educational-single-cycle", "core", {}, {}, {}, {}};
    auto profile = circuit::structuralThroughDepth(
        0, circuit::UnavailableFidelityPolicy::UseOnlyAvailableAndRecordException);
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

    auto strict_profile = circuit::strictAllStructural();
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
    require(strict_root != strict_entries.end(),
            "Strict structural RV32I manifest omitted the root");

    auto mixed_profile =
        circuit::BuildProfileBuilder("behavioral-below-alu")
            .addRule(circuit::preferFidelity(
                circuit::Fidelity::Behavioral,
                circuit::ProfileSelector::subtree("alu"),
                "make the ALU subtree behavioral"))
            .addRule(circuit::preferFidelity(
                circuit::Fidelity::Structural,
                circuit::ProfileSelector::exactPath("alu"),
                "keep the ALU itself structural"))
            .unavailablePolicy(
                circuit::UnavailableFidelityPolicy::
                    UseOnlyAvailableAndRecordException)
            .build();
    const auto mixed_alu = catalog->createRoot(
        circuit::families::ALU32.request("alu"),
        std::move(mixed_profile));
    require(
        mixed_alu.root->getSelectedFidelity() == "structural",
        "Exact ALU root selection was overridden by its descendant profile");
    const std::set<std::string> selectable_alu_children{
        "ARITHMETIC", "LOGIC", "SHIFT",
        "ARITHMETIC_ZERO", "RESULT_ZERO"};
    size_t selected_children = 0;
    for (const auto& child : mixed_alu.root->getChildren()) {
        if (selectable_alu_children.count(child->getName()) == 0) {
            continue;
        }
        ++selected_children;
        require(
            child->getSelectedFidelity() == "behavioral",
            "ALU descendant profile did not select behavioral "
                + child->getName());
        require(
            child->getChildren().empty(),
            "Behavioral ALU descendant must remain a black box: "
                + child->getName());
    }
    require(
        selected_children == selectable_alu_children.size(),
        "Structural ALU omitted a profile-selectable child block");

    auto mixed_system_profile =
        circuit::BuildProfileBuilder("behavioral-core")
            .addRule(circuit::preferFidelity(
                circuit::Fidelity::Behavioral,
                circuit::ProfileSelector::subtree("system"),
                "compact system subtree"))
            .addRule(circuit::preferFidelity(
                circuit::Fidelity::Structural,
                circuit::ProfileSelector::exactPath("system"),
                "visible system wrapper"))
            .unavailablePolicy(
                circuit::UnavailableFidelityPolicy::
                    UseOnlyAvailableAndRecordException)
            .build();
    const auto mixed_system = catalog->createRoot(
        circuit::families::RV32ISingleCycleSystem.request("system"),
        std::move(mixed_system_profile));
    const auto structural_system =
        std::dynamic_pointer_cast<RV32ISingleCycleSystem>(
            mixed_system.root);
    require(
        structural_system != nullptr,
        "Profile did not keep the system wrapper structural");
    require(
        structural_system->core() != nullptr
            && structural_system->core()->getSelectedFidelity()
                == "behavioral",
        "One recursive profile did not select the nested core");
    require(
        structural_system->snapshotArchitecturalState()
                .toKnownState().pc == 0,
        "Structural system could not observe its selected core through "
        "the fidelity-independent state capability");
}

std::string ComponentTestModelTest::getTestName() const {
    return "ComponentTestModelTest";
}

void ComponentTestModelTest::verifyResults() {
    using namespace circuit::test;

    ComponentTestSpec spec{
        "MemoryBitTest",
        std::string(circuit::families::MemoryBit.id()),
        "MEMORY_BIT_ROOT",
        {},
        {},
    };
    ActionScenario scenario{
        "contract",
        {},
        {
            {
                "reset",
                CheckpointKind::AfterEdge,
                {
                    {"D", logicBit(false)},
                    {"WE", logicBit(false)},
                    {"CLK", logicBit(false)},
                    {"RST", logicBit(true)},
                },
                {{"Q", logicBit(false)}},
            },
            {
                "armed",
                CheckpointKind::Settled,
                {
                    {"D", logicBit(true)},
                    {"WE", logicBit(true)},
                    {"RST", logicBit(false)},
                },
                {{"Q", logicBit(false)}},
            },
            {
                "write-one",
                CheckpointKind::AfterEdge,
                {{"CLK", logicBit(true)}},
                {{"Q", logicBit(true)}},
            },
            {
                "falling-edge",
                CheckpointKind::AfterEdge,
                {{"CLK", logicBit(false)}},
                {{"Q", logicBit(true)}},
            },
            {
                "prepare-unknown",
                CheckpointKind::Settled,
                {{"D", logicBit(LogicValue::UNKNOWN)}},
                {{"Q", logicBit(true)}},
            },
            {
                "write-unknown",
                CheckpointKind::AfterEdge,
                {{"CLK", logicBit(true)}},
                {{"Q", logicBit(LogicValue::UNKNOWN)}},
            },
        },
        {10'000, 100'000},
    };
    spec.scenarios.push_back(scenario);

    ComponentTestRunner runner;
    auto runs = runner.runAll(spec, spec.scenarios.front());
    require(runs.targets.size() == 2,
            "Unified component test must run both MemoryBit fidelities");
    require(runs.targets[0].root != runs.targets[1].root,
            "Fidelity runs must use independent component trees");
    require(runs.targets[0].simulator != runs.targets[1].simulator,
            "Fidelity runs must use independent simulators");
    for (const auto& target : runs.targets) {
        require(target.completed, "Run artifact must be immutable-ready");
        require(target.checkpoints.size() == scenario.actions.size(),
                "Run artifact omitted checkpoints");
        for (size_t index = 1; index < target.checkpoints.size(); ++index) {
            require(
                target.checkpoints[index].actual_time
                    > target.checkpoints[index - 1].actual_time,
                "Checkpoint times must be strictly increasing");
        }
        require(
            target.profile != nullptr
                && target.profile->name().starts_with(
                    "canonical-default-"),
            "Component test did not preserve its recursive base profile");
        require(target.manifest != nullptr
                    && !target.manifest->entries().empty(),
                "Run artifact omitted its build manifest");
    }

    auto corrupted = runs.targets.back();
    corrupted.checkpoints.back().pins.outputs.at("Q") =
        logicBit(LogicValue::LOW);
    requireThrows<std::runtime_error>(
        [&] { ComponentTestRunner::compare(runs.targets.front(), corrupted); },
        "Automatic public-pin comparison failed to detect corruption");

    auto named_profile =
        circuit::BuildProfileBuilder("explicit-profile").build();
    auto configured = runner.run(
        spec,
        spec.scenarios.front(),
        std::move(named_profile));
    require(
        configured.root_fidelity == circuit::Fidelity::Structural,
        "Explicit run configuration did not preserve the forced DUT fidelity");
    require(
        configured.profile
            && configured.profile->name()
                == "explicit-profile",
        "Explicit run did not preserve the recursive profile");
}
