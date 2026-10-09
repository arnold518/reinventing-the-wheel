#pragma once
#include "modules/memory/DigitalMemory.hpp"

struct NORFlashConfig : circuit::memory_device::Geometry {
    size_t block_words = 2;
    size_t phase_ticks = 32, program_ticks = 128, erase_ticks = 256;
    void validate(size_t width, bool expanded = false) const;
};
NORFlashConfig norFlashConfigFromParameters(const circuit::ParameterMap &);
circuit::ParameterMap norFlashParameters(const NORFlashConfig &);
void registerNORFlashComponents(circuit::ComponentCatalog &);

// Digital nonvolatile charge primitive. Erased = 1; programming only clears bits.
class NORFlashCell : public circuit::memory_device::Primitive {
  public:
    explicit NORFlashCell(std::string name);
    void evaluate(size_t, Simulator &) override;
    PrimitiveTiming primitiveTiming() const override { return PrimitiveTiming::SelfTimed; }

  private:
    LogicValue value_ = LogicValue::HIGH;
    uint8_t initialized_ = 0;
};
class NORFlashRow : public IOComponent {
  public:
    NORFlashRow(std::string name, size_t columns = 4);
    const char *getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder &) override;

  private:
    size_t columns_;
    std::string type_;
};
class NORFlashBank : public IOComponent {
  public:
    NORFlashBank(std::string name, size_t width, NORFlashConfig config = {});
    const char *getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder &) override;

  private:
    size_t width_;
    NORFlashConfig config_;
    std::string type_;
};
template <size_t W> class NORFlashStorageBank : public NORFlashBank {
  public:
    NORFlashStorageBank(std::string n, NORFlashConfig c = {}) : NORFlashBank(std::move(n), W, c) {}
};
class NORFlashDevice : public IOComponent {
  public:
    NORFlashDevice(std::string name, size_t width, NORFlashConfig config = {});
    const char *getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder &) override;

  private:
    size_t width_;
    NORFlashConfig config_;
    std::string type_;
};
template <size_t W> class NORFlash : public NORFlashDevice {
  public:
    NORFlash(std::string n, NORFlashConfig c = {}) : NORFlashDevice(std::move(n), W, c) {}
};
