#include "tests/RV32IExternalValidationTests.hpp"

#include "basic/Wire.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/capabilities/RV32ISystemProgramAccess.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "modules/memory/Memory64Kx32.hpp"
#include "modules/rv32i/RV32IBuildProfiles.hpp"
#include "modules/rv32i/RV32ISingleCycleSystem.hpp"
#include "rv32i/RV32IElfImage.hpp"
#include "simulator/Event.hpp"
#include <filesystem>
#include <iomanip>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifndef CIRCUITSIM_SOURCE_DIR
#error "CIRCUITSIM_SOURCE_DIR must identify the repository root"
#endif

namespace {
constexpr const char* RootName =
    "RV32I_EXTERNAL_VALIDATION_ROOT";
constexpr uint32_t TohostAddress = 0x0003ffc0U;
constexpr size_t MaximumInstructions = 64;
constexpr size_t CycleTimeStep = 4000;

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void drive(
    Simulator& simulator,
    size_t time,
    const std::shared_ptr<Wire<>>& wire,
    bool value) {
    simulator.scheduleEvent(
        std::make_shared<WireUpdateEvent<>>(
            time,
            wire,
            value
                ? LogicValue::HIGH
                : LogicValue::LOW));
}

uint32_t observedWord(
    const std::map<uint32_t, uint8_t>& bytes,
    uint32_t address) {
    uint32_t value = 0;
    for (size_t byte = 0; byte < 4; ++byte) {
        const auto found = bytes.find(
            static_cast<uint32_t>(address + byte));
        if (found != bytes.end()) {
            value |= static_cast<uint32_t>(found->second)
                  << (byte * 8);
        }
    }
    return value;
}

std::string hex32(uint32_t value) {
    std::ostringstream out;
    out << "0x" << std::hex << std::setw(8)
        << std::setfill('0') << value;
    return out.str();
}

class ExternalFixtureRun final : public SimulationTest {
public:
    ExternalFixtureRun(
        std::filesystem::path elf_path,
        circuit::Fidelity fidelity,
        uint32_t tohost_address,
        size_t maximum_instructions,
        bool record_history)
        : elf_path_(std::move(elf_path)),
          fidelity_(fidelity),
          tohost_address_(tohost_address),
          maximum_instructions_(maximum_instructions),
          record_history_(record_history) {
        require(
            maximum_instructions_ > 0,
            "External RV32I instruction limit must be positive");
        require(
            (tohost_address_ & 0x3U) == 0,
            "External RV32I tohost address must be word aligned");
    }

    void setupCircuit() override {
        sim->setHistoryRecordingEnabled(record_history_);
        const auto profile = circuit::withExactFidelity(
            circuit::canonicalDefaultProfile(),
            RootName,
            fidelity_,
            "external-rv32i-self-check");
        auto build =
            circuit::builtinComponentCatalog().createRoot(
                rv32i::educationalSystemRequest(RootName),
                profile);
        root = std::move(build.root);
        builder =
            std::make_unique<ComponentBuilder>(root);
        buildCircuit();
        setInitialState();
    }

    std::string getTestName() const override {
        return "RV32IExternalFixtureRun/"
             + elf_path_.filename().string()
             + "/" + circuit::toString(fidelity_);
    }

    size_t getRunDuration() const override {
        return run_duration_;
    }

    std::vector<SimulationCheckpoint>
    getCheckpoints() const override {
        return checkpoints_;
    }

    rv32i::test::ExternalFixtureRunResult
    result() const {
        auto result = result_;
        result.hardware_cycles = result.instruction_count;
        result.simulator_counters =
            sim->getPerformanceCounters();
        return result;
    }

protected:
    void buildCircuit() override {
        system_ =
            std::dynamic_pointer_cast<IOComponent>(root);
        program_access_ =
            std::dynamic_pointer_cast<
                RV32ISystemProgramAccess>(root);
        require(
            system_ != nullptr,
            getTestName() + ": root lacks IO contract");
        require(
            program_access_ != nullptr,
            getTestName()
                + ": root lacks program-access contract");

        clk_wire_ = builder->addNewWire(
            "CLK_IN",
            nullptr,
            {system_->getInputPin("CLK")});
        rst_wire_ = builder->addNewWire(
            "RST_IN",
            nullptr,
            {system_->getInputPin("RST")});
        enable_wire_ = builder->addNewWire(
            "ENABLE_IN",
            nullptr,
            {system_->getInputPin("ENABLE")});
        builder->addNewWire<32>(
            "PC_OUT",
            system_->getOutputPin<32>("PC"),
            {});
        builder->addNewWire(
            "HALTED_OUT",
            system_->getOutputPin("HALTED"),
            {});
        builder->addNewWire(
            "TRAPPED_OUT",
            system_->getOutputPin("TRAPPED"),
            {});
    }

    void setInitialState() override {
        program_access_->setProgramMemoryHistoryRecordingEnabled(
            record_history_);
        const auto image =
            rv32i::RV32IElfImage::fromFile(elf_path_);
        image.requireFitsMemory(
            Memory64Kx32::capacityBytes());
        require(
            image.entryPoint() == 0,
            getTestName()
                + ": structural reset vector requires ELF entry 0");

        program_access_->clearInstructionMemory();
        program_access_->clearDataMemory();
        for (const auto& segment : image.segments()) {
            /*
             * The current system is Harvard. Mirroring every loadable
             * segment gives instruction fetches and data loads the same
             * initial bare-metal image without merging the memories.
             */
            program_access_->loadInstructionBytes(
                segment.address,
                segment.bytes);
            program_access_->loadDataBytes(
                segment.address,
                segment.bytes);
        }

        drive(*sim, 0, clk_wire_, false);
        drive(*sim, 0, rst_wire_, true);
        drive(*sim, 0, enable_wire_, true);
    }

    void runSimulation() override {
        sim->advanceAndRecord(1000);
        drive(*sim, 1000, rst_wire_, false);
        sim->advanceAndRecord(5000);

        size_t last_observation_time =
            sim->getCurrentTime();
        std::map<uint32_t, uint8_t> observed_writes;
        for (size_t instruction = 0;
             instruction < maximum_instructions_;
             ++instruction) {
            const size_t start_time =
                sim->getCurrentTime();
            drive(
                *sim,
                start_time + 1000,
                clk_wire_,
                true);
            drive(
                *sim,
                start_time + 1500,
                clk_wire_,
                false);
            sim->advanceAndRecord(
                start_time + CycleTimeStep);

            if (record_history_) {
                const auto writes =
                    program_access_
                        ->dataMemoryWritesInTimeRange(
                            last_observation_time,
                            sim->getCurrentTime());
                for (const auto& [address, value] : writes) {
                    observed_writes[address] = value;
                }
                result_.tohost = observedWord(
                    observed_writes,
                    tohost_address_);
            } else {
                const auto access =
                    program_access_->lastCommittedDataMemoryAccess();
                if (access.kind
                        == rv32i::RV32IMemoryAccessKind::Write
                    && access.address <= tohost_address_
                    && access.address + 4 > tohost_address_) {
                    const auto bytes = program_access_->readDataBytes(
                        tohost_address_, 4);
                    result_.tohost = static_cast<uint32_t>(bytes[0])
                        | (static_cast<uint32_t>(bytes[1]) << 8)
                        | (static_cast<uint32_t>(bytes[2]) << 16)
                        | (static_cast<uint32_t>(bytes[3]) << 24);
                }
            }
            last_observation_time =
                sim->getCurrentTime();

            result_.pc = static_cast<uint32_t>(
                system_->getOutputPin<32>("PC")
                    ->getValueAsUInt64());
            result_.instruction_count =
                instruction + 1;
            if (record_history_) {
                checkpoints_.push_back({
                    sim->getCurrentTime(),
                    "I" + std::to_string(instruction + 1),
                    "PC=" + hex32(result_.pc)
                        + ", tohost="
                        + hex32(result_.tohost),
                    instruction + 1,
                });
            }

            if (result_.tohost == 1
                || result_.tohost == 3) {
                run_duration_ =
                    sim->getCurrentTime();
                return;
            }
            require(
                system_->getOutputPin("HALTED")->getValue()
                    != LogicValue::HIGH,
                getTestName()
                    + ": CPU halted before writing tohost");
            require(
                system_->getOutputPin("TRAPPED")->getValue()
                    != LogicValue::HIGH,
                getTestName()
                    + ": CPU trapped before writing tohost");
        }
        run_duration_ = sim->getCurrentTime();
    }

    void verifyResults() override {
        require(
            result_.tohost != 3,
            getTestName()
                + ": external self-check reported failure");
        require(
            result_.tohost == 1,
            getTestName()
                + ": timed out waiting for tohost=1; last PC="
                + hex32(result_.pc));
    }

private:
    std::filesystem::path elf_path_;
    circuit::Fidelity fidelity_;
    uint32_t tohost_address_;
    size_t maximum_instructions_;
    bool record_history_ = true;
    std::shared_ptr<IOComponent> system_{};
    std::shared_ptr<RV32ISystemProgramAccess>
        program_access_{};
    std::shared_ptr<Wire<>> clk_wire_{};
    std::shared_ptr<Wire<>> rst_wire_{};
    std::shared_ptr<Wire<>> enable_wire_{};
    rv32i::test::ExternalFixtureRunResult result_{};
    size_t run_duration_ = 0;
    std::vector<SimulationCheckpoint> checkpoints_{};
};

std::filesystem::path smokeFixturePath() {
    return std::filesystem::path(CIRCUITSIM_SOURCE_DIR)
         / "tests"
         / "fixtures"
         / "rv32i"
         / "external-smoke"
         / "self-check.elf";
}
} // namespace

rv32i::test::ExternalFixtureValidationResult
rv32i::test::validateExternalFixture(
    const std::filesystem::path& elf_path,
    uint32_t tohost_address,
    size_t maximum_instructions) {
    const auto behavioral = runExternalFixture(
        elf_path,
        circuit::Fidelity::Behavioral,
        tohost_address,
        maximum_instructions);
    const auto structural = runExternalFixture(
        elf_path,
        circuit::Fidelity::Structural,
        tohost_address,
        maximum_instructions);

    require(
        behavioral.tohost == structural.tohost,
        "RV32I fidelities returned different external verdicts");
    require(
        behavioral.instruction_count
            == structural.instruction_count,
        "RV32I fidelities reached the external verdict after "
        "different instruction counts");

    return {behavioral, structural};
}

rv32i::test::ExternalFixtureRunResult
rv32i::test::runExternalFixture(
    const std::filesystem::path& elf_path,
    circuit::Fidelity fidelity,
    uint32_t tohost_address,
    size_t maximum_instructions,
    bool record_history) {
    require(
        std::filesystem::is_regular_file(elf_path),
        "External RV32I fixture is missing: "
            + elf_path.string());

    ExternalFixtureRun run(
        elf_path,
        fidelity,
        tohost_address,
        maximum_instructions,
        record_history);
    require(
        run.run(),
        circuit::toString(fidelity)
            + " RV32I root failed the external fixture");
    return run.result();
}

std::string
RV32IExternalValidationSmokeTest::getTestName() const {
    return "RV32IExternalValidationSmokeTest";
}

void RV32IExternalValidationSmokeTest::verifyResults() {
    const auto fixture = smokeFixturePath();
    const auto result =
        rv32i::test::validateExternalFixture(
        fixture,
        TohostAddress,
        MaximumInstructions);
    require(
        result.behavioral.tohost == 1
            && result.structural.tohost == 1,
        "External RV32I smoke fixture did not pass");
    const auto headless = rv32i::test::runExternalFixture(
        fixture,
        circuit::Fidelity::Behavioral,
        TohostAddress,
        MaximumInstructions,
        false);
    require(
        headless.tohost == 1
            && headless.instruction_count
                == result.behavioral.instruction_count,
        "Headless external execution changed the RV32I result");
}
