#pragma once

#include "components/BasicComponent.hpp"
#include <array>
#include <vector>

class BehavioralMemory64Kx32 : public BasicComponent {
public:
    BehavioralMemory64Kx32(std::string name);
    static constexpr const char* TypeName = "BehavioralMemory64Kx32";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;

private:
    static constexpr size_t WordCount = 64 * 1024;
    static constexpr size_t BytesPerWord = 4;
    static constexpr size_t ByteCount = WordCount * BytesPerWord;

    std::vector<std::array<LogicValue, 8>> bytes;
    LogicValue previous_clk;
};
