#pragma once

#include "components/IOComponent.hpp"
#include "components/selection/ComponentFamily.hpp"
#include <string>
#include <vector>

namespace circuit::families {
extern const ComponentFamily RV32IIFIDPipelineRegister;
extern const ComponentFamily RV32IIDEXPipelineRegister;
extern const ComponentFamily RV32IEXMEMPipelineRegister;
extern const ComponentFamily RV32IMEMWBPipelineRegister;
}

/**
 * Shared structural blueprint for the four stage-specific pipeline registers.
 *
 * Each public family supplies meaningful field names, while this base keeps
 * the storage implementation identical: one Register32 per payload field and
 * one MemoryBit for the valid flag.
 */
class RV32IPipelineRegisterBlock : public IOComponent {
public:
    RV32IPipelineRegisterBlock(
        std::string name,
        std::vector<std::string> fields,
        PinInitFunction initializer);

    void buildInternals(ComponentBuilder& builder) override;

protected:
    const std::vector<std::string>& fields() const { return fields_; }

private:
    std::vector<std::string> fields_;
};

class RV32IIFIDPipelineRegister : public RV32IPipelineRegisterBlock {
public:
    explicit RV32IIFIDPipelineRegister(std::string name);
    static constexpr const char* TypeName = "RV32IIFIDPipelineRegister";
    const char* getTypeName() const override { return TypeName; }
};

class RV32IIDEXPipelineRegister : public RV32IPipelineRegisterBlock {
public:
    explicit RV32IIDEXPipelineRegister(std::string name);
    static constexpr const char* TypeName = "RV32IIDEXPipelineRegister";
    const char* getTypeName() const override { return TypeName; }
};

class RV32IEXMEMPipelineRegister : public RV32IPipelineRegisterBlock {
public:
    explicit RV32IEXMEMPipelineRegister(std::string name);
    static constexpr const char* TypeName = "RV32IEXMEMPipelineRegister";
    const char* getTypeName() const override { return TypeName; }
};

class RV32IMEMWBPipelineRegister : public RV32IPipelineRegisterBlock {
public:
    explicit RV32IMEMWBPipelineRegister(std::string name);
    static constexpr const char* TypeName = "RV32IMEMWBPipelineRegister";
    const char* getTypeName() const override { return TypeName; }
};

