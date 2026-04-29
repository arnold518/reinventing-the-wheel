#pragma once

#include "components/BasicComponent.hpp"
#include <cstddef>
#include <string>

template<size_t WIDTH>
class BitSplitter : public BasicComponent {
public:
    explicit BitSplitter(std::string name);
    static constexpr const char* TypeName = "BitSplitter";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;
};

template<size_t WIDTH>
class BitJoiner : public BasicComponent {
public:
    explicit BitJoiner(std::string name);
    static constexpr const char* TypeName = "BitJoiner";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;
};

#include "modules/utility/BitAdapter.tpp"
