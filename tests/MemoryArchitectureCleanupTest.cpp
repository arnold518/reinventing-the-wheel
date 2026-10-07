#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "modules/memory/MemoryContainer.hpp"
#include "modules/memory/MemoryStorage.hpp"
#include "rv32i/RV32IInstructionOracle.hpp"
#include "rv32i/RV32IProgram.hpp"
#include "tests/TestRegistry.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
template<class F> void rejects(F f) {
    bool rejected = false;
    try { f(); } catch (const std::exception&) { rejected = true; }
    check(rejected, "Retired or invalid operation accepted");
}
}

int main() {
    try {
        const auto& catalog = circuit::builtinComponentCatalog();
        for (const auto* id : {"rv32i.memory.64k-x32", "memory.word-array.4x32",
                "memory.word-array.32x32", "memory.bus-adapter.width32"})
            rejects([&] { catalog.contract(id); });
        for (const auto* model : {"sram", "rom", "ideal-reference"})
            for (const auto* width : {"8", "16", "32"})
                rejects([&] { catalog.contract(std::string("memory.container.") + model + "." + width); });
        for (size_t width : {8, 16, 32}) {
            const auto id = "memory.container.ff." + std::to_string(width);
            // These IDs remain valid for saved fidelity profiles.
            check(catalog.implementation(id + ".behavioral").contract_id == id, "Lost FF behavioral ID");
            check(catalog.implementation(id + ".structural-slice").contract_id == id, "Lost FF structural ID");
        }
        for (const auto& contract : catalog.contracts())
        for (const auto& implementation : catalog.implementationsFor(contract.id)) {
            for (const auto& test : implementation.evidence.contract_tests)
                check(bool(createTestByName(test)), "Catalog evidence points to a removed scenario");
        }

        // Provision real current devices through the generic capability. The last
        // byte is legal; crossing the boundary is not. No legacy memory class.
        auto device = createMemoryContainer<32, circuit::memory::FFModel>("M",
            circuit::memory::EndpointConfig{.depth_words = 4});
        auto view = std::dynamic_pointer_cast<MemoryStateView>(device);
        view->clearMemoryImage();
        rv32i::RV32IProgram::fromBytes({0xaa, 0xbb}).loadInto(*view, 14);
        check(view->memoryStorage().readBytes(14, 2) == std::vector<uint8_t>({0xaa,0xbb}), "Image loading changed");
        rejects([&] { rv32i::RV32IProgram::fromBytes({1,2}).loadInto(*view, 15); });

        // Historical zero writes and unchanged-value writes must remain visible.
        MemoryStorage storage(4, 32, LogicValue::LOW);
        std::vector<LogicValue> zero(8, LogicValue::LOW), ones(8, LogicValue::HIGH);
        storage.writeLogic(10, 0, zero, true);
        storage.writeLogic(20, 0, zero, true);
        check(storage.getTouchedWordCountAtTime(9) == 0, "Write appeared before its timestamp");
        check(storage.getTouchedWordCountAtTime(10) == 1, "Zero write was lost");
        check(storage.getByteWritesInTimeRange(10,20).at(0) == 0, "Repeated zero write was lost");
        storage.writeLogic(30, 15, ones, true);
        storage.fill(40, LogicValue::LOW);
        check(storage.getTouchedWordCountAtTime(40) == 0, "Reset did not clear touched words");
        check(storage.getWordsAtTime(30,12,1)[0].second[31] == LogicValue::HIGH, "Reset erased earlier history");
        storage.setHistoryRecordingEnabled(false);
        rejects([&] { storage.getWordsAtTime(30,12,1); });

        // The answer-sheet adapter reads storage without mutating it; the owner
        // records the actual write at its simulated timestamp.
        MemoryStorage instructions(4,32,LogicValue::LOW), data(4,32,LogicValue::LOW);
        instructions.loadBytes(0, rv32i::RV32IProgram::fromWords({0x00102023u}).bytes()); // sw x1,0(x0)
        rv32i::RV32IState state;
        state.writeRegister(1,0x12345678u);
        auto trace = rv32i::RV32IInstructionOracle::stepWithoutDataMemoryWrite(state,instructions,data);
        check(trace.memory.kind == rv32i::RV32IMemoryAccessKind::Write && trace.memory.write_data == 0x12345678u,
              "Answer sheet lost write intent");
        check(data.readBytes(0,4) == std::vector<uint8_t>(4,0), "Oracle performed an unrecorded write");
        std::cout << "Memory architecture cleanup contracts passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
