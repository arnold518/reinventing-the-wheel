#pragma once

#include "basic/PinBase.hpp"
#include "basic/Wire.hpp"
#include "components/Component.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/IOComponent.hpp"
#include "simulator/Event.hpp"
#include "simulator/SimulationTest.hpp"
#include "simulator/Simulator.hpp"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

struct TestValue {
    bool multi = false;
    LogicValue logic = LogicValue::UNKNOWN;
    uint64_t numeric = 0;

    TestValue() = default;
    TestValue(LogicValue value) : multi(false), logic(value), numeric(0) {}
    TestValue(uint64_t value) : multi(true), logic(LogicValue::UNKNOWN), numeric(value) {}
};

struct TestRow {
    std::map<std::string, TestValue> inputs;
    std::map<std::string, TestValue> outputs;
};

inline std::string testValueToString(const TestValue& value) {
    std::ostringstream out;
    if (value.multi) {
        out << "0x" << std::hex << std::uppercase << value.numeric;
    } else {
        out << value.logic;
    }
    return out.str();
}

inline std::string testValuesToString(const std::map<std::string, TestValue>& values) {
    std::ostringstream out;
    bool first = true;
    for (const auto& [pin_name, value] : values) {
        if (!first) {
            out << ", ";
        }
        first = false;
        out << pin_name << "=" << testValueToString(value);
    }
    return out.str();
}

inline std::string testRowCheckpointDetail(const TestRow& row) {
    return testValuesToString(row.inputs) + " -> " + testValuesToString(row.outputs);
}

struct RunRowsOptions {
    std::optional<size_t> time_step = std::nullopt;
    size_t probe_time_limit = 100000;
};

template<typename... Args>
struct FirstRunRowsArgIsOptions : std::false_type {};

template<typename First, typename... Rest>
struct FirstRunRowsArgIsOptions<First, Rest...>
    : std::is_same<std::decay_t<First>, RunRowsOptions> {};

class StandaloneVerificationTest : public SimulationTest {
public:
    void setupCircuit() override {
        root = Component::create<Component>(getTestName());
        builder = std::make_unique<ComponentBuilder>(root);
        buildCircuit();
        setInitialState();
    }

    size_t getRunDuration() const override {
        return 0;
    }

protected:
    void buildCircuit() override {}
    void setInitialState() override {}
};

inline std::shared_ptr<Event> makeTestWireUpdate(size_t time, const std::shared_ptr<WireBase>& wire, const TestValue& value) {
    uint64_t numeric = value.multi ? value.numeric : static_cast<uint64_t>(value.logic == LogicValue::HIGH);
    switch (wire->getWidth()) {
        case 1:
            if (value.multi) return std::make_shared<WireUpdateEvent<1>>(time, std::dynamic_pointer_cast<Wire<1>>(wire), numeric);
            return std::make_shared<WireUpdateEvent<1>>(time, std::dynamic_pointer_cast<Wire<1>>(wire), value.logic);
        case 2: return std::make_shared<WireUpdateEvent<2>>(time, std::dynamic_pointer_cast<Wire<2>>(wire), numeric);
        case 3: return std::make_shared<WireUpdateEvent<3>>(time, std::dynamic_pointer_cast<Wire<3>>(wire), numeric);
        case 4: return std::make_shared<WireUpdateEvent<4>>(time, std::dynamic_pointer_cast<Wire<4>>(wire), numeric);
        case 5: return std::make_shared<WireUpdateEvent<5>>(time, std::dynamic_pointer_cast<Wire<5>>(wire), numeric);
        case 8: return std::make_shared<WireUpdateEvent<8>>(time, std::dynamic_pointer_cast<Wire<8>>(wire), numeric);
        case 16: return std::make_shared<WireUpdateEvent<16>>(time, std::dynamic_pointer_cast<Wire<16>>(wire), numeric);
        case 32: return std::make_shared<WireUpdateEvent<32>>(time, std::dynamic_pointer_cast<Wire<32>>(wire), numeric);
        default: return nullptr;
    }
}

inline bool checkExpectedOutputs(const std::shared_ptr<IOComponent>& io,
                                 const TestRow& row,
                                 size_t row_index,
                                 const std::string& mode) {
    for (const auto& [pin_name, expected] : row.outputs) {
        auto pin = io->getOutputPinDynamic(pin_name);
        assert(pin && "Missing output pin");
        if (expected.multi || pin->getWidth() > 1) {
            auto actual = pin->getValueAsUInt64();
            if (actual != expected.numeric) {
                std::cerr << mode << " row " << row_index << ": "
                          << pin_name << " = " << actual
                          << ", expected " << expected.numeric << std::endl;
                return false;
            }
        } else {
            auto actual = pin->getValue();
            if (actual != expected.logic) {
                std::cerr << mode << " row " << row_index << ": "
                          << pin_name << " = " << actual
                          << ", expected " << expected.logic << std::endl;
                return false;
            }
        }
    }
    return true;
}

struct RunRowsProbeResult {
    bool passed = true;
    size_t max_delay = 0;
};

inline size_t secondSmallestPowerOfTenGreaterThan(size_t value) {
    size_t result = 1;
    while (result <= value) {
        assert(result <= static_cast<size_t>(-1) / 10 && "runRows time_step overflow");
        result *= 10;
    }
    assert(result <= static_cast<size_t>(-1) / 10 && "runRows time_step overflow");
    return result * 10;
}

template<typename ComponentT, typename... Args>
RunRowsProbeResult probeIsolatedRows(const std::vector<TestRow>& rows, const RunRowsOptions& options, Args&&... args) {
    assert(options.probe_time_limit > 0 && "runRows requires a positive probe_time_limit");
    RunRowsProbeResult result;
    for (size_t row_index = 0; row_index < rows.size(); ++row_index) {
        Simulator sim;
        auto root = Component::create<ComponentT>("ROOT", args...);
        auto io = std::dynamic_pointer_cast<IOComponent>(root);
        assert(io);
        ComponentBuilder builder(root);

        for (const auto& [pin_name, value] : rows[row_index].inputs) {
            auto pin = io->getInputPinDynamic(pin_name);
            assert(pin && "Missing input pin");
            auto wire = builder.addNewWireDynamic("IN_" + pin_name, pin->getWidth(), nullptr, {pin});
            auto event = makeTestWireUpdate(0, wire, value);
            assert(event && "Unsupported test wire width");
            sim.scheduleEvent(event);
        }

        for (const auto& [pin_name, _] : rows[row_index].outputs) {
            auto pin = io->getOutputPinDynamic(pin_name);
            assert(pin && "Missing output pin");
            builder.addNewWireDynamic("OUT_" + pin_name, pin->getWidth(), pin, {});
        }

        sim.runAndRecord(options.probe_time_limit);
        const auto timestamps = sim.getUniqueTimestamps();
        const size_t row_delay = timestamps.empty() ? 0 : timestamps.back();
        if (row_delay >= options.probe_time_limit) {
            std::cerr << "isolated row " << row_index
                      << ": measured delay reached probe_time_limit " << options.probe_time_limit
                      << "; increase RunRowsOptions::probe_time_limit" << std::endl;
            assert(false && "runRows probe_time_limit is too small");
            result.passed = false;
            return result;
        }
        result.max_delay = std::max(result.max_delay, row_delay);

        sim.setCircuitStateAtTime(row_delay);
        if (!checkExpectedOutputs(io, rows[row_index], row_index, "isolated")) {
            result.passed = false;
            return result;
        }
    }
    return result;
}

template<typename ComponentT, typename... Args>
bool runRowsWithOptions(const std::vector<TestRow>& rows, const RunRowsOptions& options, Args&&... args) {
    if (rows.empty()) {
        return true;
    }
    const auto probe = probeIsolatedRows<ComponentT>(rows, options, args...);
    if (!probe.passed) {
        return false;
    }
    const size_t time_step = options.time_step.value_or(secondSmallestPowerOfTenGreaterThan(probe.max_delay));
    if (time_step <= probe.max_delay) {
        std::cerr << "runRows time_step " << time_step
                  << " must be greater than measured max row delay "
                  << probe.max_delay << std::endl;
        assert(false && "runRows time_step must be greater than max row delay");
        return false;
    }
    assert(rows.size() <= static_cast<size_t>(-1) / time_step && "runRows duration overflow");

    Simulator sim;
    auto root = Component::create<ComponentT>("ROOT", args...);
    auto io = std::dynamic_pointer_cast<IOComponent>(root);
    assert(io);
    ComponentBuilder builder(root);

    std::map<std::string, std::shared_ptr<WireBase>> input_wires;
    std::map<std::string, std::shared_ptr<WireBase>> output_wires;
    for (const auto& row : rows) {
        for (const auto& [pin_name, _] : row.inputs) {
            if (input_wires.count(pin_name)) {
                continue;
            }
            auto pin = io->getInputPinDynamic(pin_name);
            assert(pin && "Missing input pin");
            input_wires[pin_name] = builder.addNewWireDynamic("IN_" + pin_name, pin->getWidth(), nullptr, {pin});
        }
        for (const auto& [pin_name, _] : row.outputs) {
            if (output_wires.count(pin_name)) {
                continue;
            }
            auto pin = io->getOutputPinDynamic(pin_name);
            assert(pin && "Missing output pin");
            output_wires[pin_name] = builder.addNewWireDynamic("OUT_" + pin_name, pin->getWidth(), pin, {});
        }
    }

    for (size_t row_index = 0; row_index < rows.size(); ++row_index) {
        const auto time = row_index * time_step;
        for (const auto& [pin_name, value] : rows[row_index].inputs) {
            auto wire_it = input_wires.find(pin_name);
            assert(wire_it != input_wires.end() && "Missing input wire");
            auto event = makeTestWireUpdate(time, wire_it->second, value);
            assert(event && "Unsupported test wire width");
            sim.scheduleEvent(event);
        }
    }

    sim.runAndRecord(rows.size() * time_step);

    for (size_t row_index = 0; row_index < rows.size(); ++row_index) {
        const auto settle_time = ((row_index + 1) * time_step) - 1;
        sim.setCircuitStateAtTime(settle_time);
        const auto& row = rows[row_index];
        if (!checkExpectedOutputs(io, row, row_index, "persistent")) {
            return false;
        }
    }
    return true;
}

template<typename ComponentT, typename... Args>
bool runRowsBatched(const std::vector<TestRow>& rows, size_t time_step, Args&&... args) {
    RunRowsOptions options;
    options.time_step = time_step;
    return runRowsWithOptions<ComponentT>(rows, options, std::forward<Args>(args)...);
}

template<typename ComponentT, typename... Args>
requires (!FirstRunRowsArgIsOptions<Args...>::value)
bool runRows(const std::vector<TestRow>& rows, Args&&... args) {
    return runRowsWithOptions<ComponentT>(rows, RunRowsOptions{}, args...);
}

template<typename ComponentT, typename... Args>
bool runRows(const std::vector<TestRow>& rows, const RunRowsOptions& options, Args&&... args) {
    return runRowsWithOptions<ComponentT>(rows, options, args...);
}

inline TestValue bits(uint64_t value) {
    return TestValue(value);
}

inline TestValue bit(bool value) {
    return TestValue(value ? LogicValue::HIGH : LogicValue::LOW);
}
