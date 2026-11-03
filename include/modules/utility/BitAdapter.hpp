#pragma once

#include "simulator/SimulationTime.hpp"

#include "components/BasicComponent.hpp"
#include <array>
#include <cstddef>
#include <memory>
#include <string>

template<size_t WIDTH>
class BitSplitter : public BasicComponent {
public:
    explicit BitSplitter(std::string name);
    static constexpr const char* TypeName = "BitSplitter";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(SimulationTime current_time, Simulator& simulator) override;
};

template<size_t WIDTH>
class BitJoiner : public BasicComponent {
    // Input handles resolved once; any pin-table change clears them.
    bool resolved_ = false;
    std::array<std::shared_ptr<Pin<1>>, WIDTH> inputs_;
protected:
    void pinsChanged() noexcept override { resolved_ = false; inputs_ = {}; }
public:
    explicit BitJoiner(std::string name);
    static constexpr const char* TypeName = "BitJoiner";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(SimulationTime current_time, Simulator& simulator) override;
};

#include "modules/utility/BitAdapter.tpp"
