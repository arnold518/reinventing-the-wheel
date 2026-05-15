#pragma once

#include "components/Component.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/IOComponent.hpp"
#include "tests/TestHelpers.hpp"
#include <cassert>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

template<typename ComponentT, typename... Args>
class ComponentRowsTest : public SimulationTest {
public:
    ComponentRowsTest(std::string test_name,
                      std::string root_name,
                      std::vector<TestRow> rows,
                      Args... args)
        : test_name_(std::move(test_name)),
          root_name_(std::move(root_name)),
          rows_(std::move(rows)),
          args_(std::move(args)...) {}

    std::string getTestName() const override {
        return test_name_;
    }

    void setupCircuit() override {
        assert(!rows_.empty() && "ComponentRowsTest requires at least one row");

        const RunRowsOptions options;
        const auto probe = std::apply(
            [&](const auto&... args) {
                return probeIsolatedRows<ComponentT>(rows_, options, args...);
            },
            args_);
        assert(probe.passed && "ComponentRowsTest isolated row probe failed");

        time_step_ = options.time_step.value_or(secondSmallestPowerOfTenGreaterThan(probe.max_delay));
        if (time_step_ <= probe.max_delay) {
            std::cerr << test_name_ << " time_step " << time_step_
                      << " must be greater than measured max row delay "
                      << probe.max_delay << std::endl;
            assert(false && "ComponentRowsTest time_step must be greater than max row delay");
        }
        assert(rows_.size() <= std::numeric_limits<size_t>::max() / time_step_
               && "ComponentRowsTest duration overflow");
        run_duration_ = rows_.size() * time_step_;

        root = std::apply(
            [&](const auto&... args) {
                return Component::create<ComponentT>(root_name_, args...);
            },
            args_);
        builder = std::make_unique<ComponentBuilder>(root);
        buildCircuit();
        setInitialState();
    }

    size_t getRunDuration() const override {
        return run_duration_;
    }

    std::vector<SimulationTest::SimulationCheckpoint> getCheckpoints() const override {
        std::vector<SimulationTest::SimulationCheckpoint> checkpoints;
        if (time_step_ == 0) {
            return checkpoints;
        }
        checkpoints.reserve(rows_.size());
        for (size_t row_index = 0; row_index < rows_.size(); ++row_index) {
            checkpoints.push_back({
                ((row_index + 1) * time_step_) - 1,
                "Row " + std::to_string(row_index),
                testRowCheckpointDetail(rows_[row_index]),
                row_index,
            });
        }
        return checkpoints;
    }

protected:
    void buildCircuit() override {}

    void setInitialState() override {
        auto io_root = std::dynamic_pointer_cast<IOComponent>(root);
        assert(io_root && "ComponentRowsTest requires IOComponent root");

        std::map<std::string, std::shared_ptr<WireBase>> input_wires;
        for (const auto& row : rows_) {
            for (const auto& [pin_name, _] : row.inputs) {
                if (input_wires.count(pin_name)) {
                    continue;
                }
                auto pin = io_root->getInputPinDynamic(pin_name);
                assert(pin && "ComponentRowsTest references a missing input pin");
                input_wires[pin_name] = builder->addNewWireDynamic(
                    "INPUT_" + pin_name,
                    pin->getWidth(),
                    nullptr,
                    {pin});
            }
            for (const auto& [pin_name, _] : row.outputs) {
                if (output_wires_.count(pin_name)) {
                    continue;
                }
                auto pin = io_root->getOutputPinDynamic(pin_name);
                assert(pin && "ComponentRowsTest references a missing output pin");
                output_wires_[pin_name] = builder->addNewWireDynamic(
                    "OUTPUT_" + pin_name,
                    pin->getWidth(),
                    pin,
                    {});
            }
        }

        for (size_t row_index = 0; row_index < rows_.size(); ++row_index) {
            const auto time = row_index * time_step_;
            for (const auto& [pin_name, value] : rows_[row_index].inputs) {
                auto wire_it = input_wires.find(pin_name);
                assert(wire_it != input_wires.end() && "Missing ComponentRowsTest input wire");
                auto event = makeTestWireUpdate(time, wire_it->second, value);
                assert(event && "Unsupported ComponentRowsTest wire width");
                sim->scheduleEvent(event);
            }
        }
    }

    void verifyResults() override {
        auto io_root = std::dynamic_pointer_cast<IOComponent>(root);
        assert(io_root && "ComponentRowsTest requires IOComponent root");

        for (size_t row_index = 0; row_index < rows_.size(); ++row_index) {
            const auto settle_time = ((row_index + 1) * time_step_) - 1;
            sim->setCircuitStateAtTime(settle_time);
            if (!checkExpectedOutputs(io_root, rows_[row_index], row_index, test_name_)) {
                assert(false && "ComponentRowsTest output mismatch");
            }
        }
    }

private:
    std::string test_name_;
    std::string root_name_;
    std::vector<TestRow> rows_;
    std::tuple<Args...> args_;
    std::map<std::string, std::shared_ptr<WireBase>> output_wires_;
    size_t time_step_ = 0;
    size_t run_duration_ = 0;
};
