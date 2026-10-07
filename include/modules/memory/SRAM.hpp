#pragma once

#include "components/BasicComponent.hpp"
#include "components/selection/ComponentCatalog.hpp"
#include "modules/memory/MemoryStructuralBuilder.hpp"
#include "modules/memory/DigitalMemory.hpp"
#include <optional>

// Digital SRAM geometry. Width is the external word size, not the row width.
struct SRAMDeviceConfig {
    size_t depth_words = 4;
    size_t words_per_row = 2;
    size_t phase_ticks = 32;
    bool record_history = true;
    MemoryStorage::Limits storage_limits{true, 64 * 1024 * 1024, 16 * 1024 * 1024};
    void validate(size_t width, bool structural = false) const;
    size_t rows() const { return depth_words / words_per_row; }
    size_t columns(size_t width) const { return width * words_per_row; }
};
SRAMDeviceConfig sramConfigFromParameters(const circuit::ParameterMap& parameters);
circuit::ParameterMap sramParameters(const SRAMDeviceConfig& config);
void registerSRAMComponents(circuit::ComponentCatalog& catalog);

namespace circuit::sram {
using Bits = std::vector<LogicValue>;
bool binary(LogicValue value);
LogicValue invert(LogicValue value);
bool known(const Bits& bits);
uint32_t number(const Bits& bits);
Bits bits(size_t width, uint32_t value);

// A digital, charge-aware *symbolic* bitline pair. This is not analog resolution.
struct BitlineState {
    LogicValue bl = LogicValue::UNKNOWN, bar = LogicValue::UNKNOWN;
    LogicValue valid = LogicValue::LOW, cell_drive = LogicValue::LOW;
    LogicValue cell_bl = LogicValue::UNKNOWN, cell_bar = LogicValue::UNKNOWN;
    bool prepared = false;
    std::optional<size_t> reading_row;
    void update(LogicValue precharge, LogicValue drive, LogicValue write_bl,
                LogicValue write_bar, const Bits& wordlines, const Bits& cells);
};

// Shared sequencing contract, used by the peripheral primitive and compact device.
// Timers recheck current inputs/deadlines: superseded events cannot finish an old access.
struct AccessSequence {
    enum class Phase { Idle, Quiet, Precharge, Release, Access, Ready };
    Phase phase = Phase::Idle;
    size_t due = 0;
    Bits signature, pending_address, pending_data, pending_mask;
    Bits address, data, mask;
    bool writing = false, precharge = false, row_enable = false;
    bool write_enable = false, sense_enable = false, output_enable = false;
    explicit AccessSequence(size_t width);
    std::optional<size_t> update(const IOComponent& io, size_t time, size_t ticks);
};

template<class T, class... Args>
std::shared_ptr<IOComponent> add(ComponentBuilder& b, const std::string& id,
    const std::string& name, circuit::ParameterMap parameters, Args&&... args) {
    if (b.getBuildContext()) return b.addByContract({id, name, std::move(parameters), {}, {}, {}});
    return b.addNewComponent<T>(name, std::forward<Args>(args)...);
}
}

class SRAMCell : public IOComponent {
public:
    static constexpr const char* TypeName = "SRAMCell";
    const char* getTypeName() const override { return TypeName; }
    explicit SRAMCell(std::string name);
    void buildInternals(ComponentBuilder& builder) override;
};

class SRAMRow : public IOComponent {
public:
    SRAMRow(std::string name, size_t columns);
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder& builder) override;
private:
    size_t columns_;
    std::string type_;
};

class SRAMBitlinePair : public BasicComponent {
public:
    SRAMBitlinePair(std::string name, size_t rows);
    const char* getTypeName() const override { return type_.c_str(); }
    void evaluate(size_t time, Simulator& simulator) override;
private:
    size_t rows_;
    std::string type_;
    circuit::sram::BitlineState state_;
};

class SRAMRowDecoder : public IOComponent {
public:
    SRAMRowDecoder(std::string name, SRAMDeviceConfig config);
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder& builder) override;
private:
    SRAMDeviceConfig config_;
    std::string type_;
};

class SRAMPrecharge : public IOComponent {
public:
    SRAMPrecharge(std::string name, size_t columns);
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder& builder) override;
private:
    size_t columns_;
    std::string type_;
};

class SRAMWriteDriver : public IOComponent {
public:
    static constexpr const char* TypeName = "SRAMWriteDriver";
    const char* getTypeName() const override { return TypeName; }
    explicit SRAMWriteDriver(std::string name);
    void buildInternals(ComponentBuilder& builder) override;
};

class SRAMSenseAmplifier : public BasicComponent {
public:
    static constexpr const char* TypeName = "SRAMSenseAmplifier";
    const char* getTypeName() const override { return TypeName; }
    explicit SRAMSenseAmplifier(std::string name);
    void evaluate(size_t time, Simulator& simulator) override;
};

template<size_t W> void defineSRAMPins(IOComponent* self) {
    static_assert(W == 8 || W == 16 || W == 32);
    self->addPin<32>("WORD_ADDR", PinType::INPUT);
    self->addPin<W>("WRITE_DATA", PinType::INPUT);
    self->addPin<W / 8>("WRITE_MASK", PinType::INPUT);
    for (const auto* pin : {"CE_N", "WE_N", "OE_N"}) self->addPin(pin, PinType::INPUT);
    self->addPin<W>("READ_DATA", PinType::OUTPUT);
}

template<size_t W> class SRAMAccessControl : public BasicComponent {
public:
    SRAMAccessControl(std::string name, SRAMDeviceConfig config);
    inline static const std::string TypeName = "SRAMAccessControl<" + std::to_string(W) + ">";
    const char* getTypeName() const override { return TypeName.c_str(); }
    void evaluate(size_t time, Simulator& simulator) override;
private:
    SRAMDeviceConfig config_;
    circuit::sram::AccessSequence sequence_{W};
};

template<size_t W> class SRAMColumnSelector : public IOComponent {
public:
    SRAMColumnSelector(std::string name, size_t words_per_row);
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder& builder) override;
private:
    size_t words_per_row_;
    std::string type_;
};

// Compatibility identity for the common output-enable primitive. Keep the
// catalog ID, pins and display type without a second implementation.
template<size_t W> class SRAMOutputBuffer : public circuit::memory_device::MemoryOutputBuffer {
public:
    explicit SRAMOutputBuffer(std::string name);
    inline static const std::string TypeName = "SRAMOutputBuffer<" + std::to_string(W) + ">";
    const char* getTypeName() const override { return TypeName.c_str(); }
};

template<size_t W> class SRAMStorageBank : public IOComponent {
public:
    SRAMStorageBank(std::string name, SRAMDeviceConfig config);
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder& builder) override;
private:
    SRAMDeviceConfig config_;
    std::string type_;
};

template<size_t W> class SRAM : public IOComponent {
public:
    inline static const std::string TypeName = "SRAM<" + std::to_string(W) + ">";
    const char* getTypeName() const override { return TypeName.c_str(); }
    explicit SRAM(std::string name, SRAMDeviceConfig config = {});
    void buildInternals(ComponentBuilder& builder) override;
private:
    SRAMDeviceConfig config_;
};

// Template definitions are included in translation units, not in this header.
extern template class SRAM<8>;
extern template class SRAM<16>;
extern template class SRAM<32>;
