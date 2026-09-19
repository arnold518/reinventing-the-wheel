#pragma once
#include "modules/memory/DigitalMemory.hpp"

struct DRAMDeviceConfig : circuit::memory_device::Geometry {
    size_t phase_ticks = 32;
    size_t retention_ticks = 100000;
    void validate(size_t width, bool expanded = false) const;
};
DRAMDeviceConfig dramConfigFromParameters(const circuit::ParameterMap &);
circuit::ParameterMap dramParameters(const DRAMDeviceConfig &);
void registerDRAMComponents(circuit::ComponentCatalog &);

// Digital charge storage is a primitive, not a flip-flop made to look like DRAM.
class DRAMCell : public circuit::memory_device::Primitive {
  public:
    DRAMCell(std::string name, size_t retention_ticks = 100000);
    void evaluate(size_t, Simulator &) override;

  private:
    size_t retention_, closed_ = 0;
    LogicValue charge_ = LogicValue::UNKNOWN, sample_ = LogicValue::UNKNOWN, last_wl_ = LogicValue::LOW;
    bool restored_ = false;
};
class DRAMRow : public IOComponent {
  public:
    DRAMRow(std::string name, size_t columns = 4, size_t retention_ticks = 100000);
    const char *getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder &) override;

  private:
    size_t columns_, retention_;
    std::string type_;
};
class DRAMSenseColumn : public circuit::memory_device::Primitive {
  public:
    DRAMSenseColumn(std::string name, size_t rows = 2);
    void evaluate(size_t, Simulator &) override;

  private:
    size_t rows_;
    bool prepared_ = true, sensed_ = false;
    size_t selected_ = 0;
    LogicValue buffer_ = LogicValue::UNKNOWN;
};
class DRAMBank : public IOComponent {
  public:
    DRAMBank(std::string name, size_t width, DRAMDeviceConfig config = {});
    const char *getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder &) override;

  private:
    size_t width_;
    DRAMDeviceConfig config_;
    std::string type_;
};
template <size_t W> class DRAMStorageBank : public DRAMBank {
  public:
    DRAMStorageBank(std::string n, DRAMDeviceConfig c = {}) : DRAMBank(std::move(n), W, c) {}
};
class DRAMDevice : public IOComponent {
  public:
    DRAMDevice(std::string name, size_t width, DRAMDeviceConfig config = {});
    const char *getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder &) override;

  private:
    size_t width_;
    DRAMDeviceConfig config_;
    std::string type_;
};
template <size_t W> class DRAM : public DRAMDevice {
  public:
    DRAM(std::string n, DRAMDeviceConfig c = {}) : DRAMDevice(std::move(n), W, c) {}
};
