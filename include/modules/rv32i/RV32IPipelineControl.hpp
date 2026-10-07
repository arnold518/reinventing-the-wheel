#pragma once

#include "components/IOComponent.hpp"
#include "components/selection/ComponentFamily.hpp"

namespace circuit::families {
extern const ComponentFamily RV32IForwardingUnit;
extern const ComponentFamily RV32IHazardDetectionUnit;
extern const ComponentFamily RV32IMemoryAlignmentUnit;
extern const ComponentFamily RV32IPipelineRetirementUnit;
extern const ComponentFamily RV32IPipelineCoordinator;
}

class RV32IForwardingUnit : public IOComponent {
public:
    explicit RV32IForwardingUnit(std::string name);
    static constexpr const char* TypeName = "RV32IForwardingUnit";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};

// Decode only needs a writeback-to-register-read bypass. Execute retains the
// full two-producer forwarding unit.
class RV32IWritebackBypass : public IOComponent {
public:
    explicit RV32IWritebackBypass(std::string name);
    static constexpr const char* TypeName = "RV32IWritebackBypass";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};

class RV32IHazardDetectionUnit : public IOComponent {
public:
    explicit RV32IHazardDetectionUnit(std::string name);
    static constexpr const char* TypeName = "RV32IHazardDetectionUnit";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};

class RV32IMemoryAlignmentUnit : public IOComponent {
public:
    explicit RV32IMemoryAlignmentUnit(std::string name);
    static constexpr const char* TypeName =
        "RV32IMemoryAlignmentUnit";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};

class RV32IPipelineRetirementUnit : public IOComponent {
public:
    explicit RV32IPipelineRetirementUnit(std::string name);
    static constexpr const char* TypeName =
        "RV32IPipelineRetirementUnit";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};

class RV32IPipelineCoordinator : public IOComponent {
public:
    explicit RV32IPipelineCoordinator(std::string name);
    static constexpr const char* TypeName =
        "RV32IPipelineCoordinator";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};
