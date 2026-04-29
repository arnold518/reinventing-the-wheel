#pragma once

#include "components/IOComponent.hpp"

class EqualityChecker8 : public IOComponent {
public:
    EqualityChecker8(std::string name);
    static constexpr const char* TypeName = "EqualityChecker8";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};

class Comparator8 : public IOComponent {
public:
    Comparator8(std::string name);
    static constexpr const char* TypeName = "Comparator8";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};

class SignedComparator8 : public IOComponent {
public:
    SignedComparator8(std::string name);
    static constexpr const char* TypeName = "SignedComparator8";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};
