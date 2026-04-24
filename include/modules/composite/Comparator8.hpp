#pragma once

#include "components/BasicComponent.hpp"

class EqualityChecker8 : public BasicComponent {
public:
    EqualityChecker8(std::string name);
    static constexpr const char* TypeName = "EqualityChecker8";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;
};

class Comparator8 : public BasicComponent {
public:
    Comparator8(std::string name);
    static constexpr const char* TypeName = "Comparator8";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;
};

class SignedComparator8 : public BasicComponent {
public:
    SignedComparator8(std::string name);
    static constexpr const char* TypeName = "SignedComparator8";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;
};
