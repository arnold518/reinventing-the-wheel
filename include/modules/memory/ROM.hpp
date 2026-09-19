#pragma once
#include "modules/memory/DigitalMemory.hpp"

struct ROMDeviceConfig : circuit::memory_device::Geometry {
    // Little-endian bytes, two hex digits per byte; unspecified tail is zero.
    std::string image_hex;
    void validate(size_t width, bool expanded = false) const;
    std::vector<uint8_t> image(size_t width) const;
};
ROMDeviceConfig romConfigFromParameters(const circuit::ParameterMap &);
circuit::ParameterMap romParameters(const ROMDeviceConfig &);
void registerROMComponents(circuit::ComponentCatalog &);

class ROMCell : public IOComponent {
  public:
    ROMCell(std::string name, bool value = false);
    const char *getTypeName() const override { return "ROMCell"; }
    void buildInternals(ComponentBuilder &) override;

  private:
    bool value_;
};
class ROMRow : public IOComponent {
  public:
    ROMRow(std::string name, std::string contents = "0000");
    const char *getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder &) override;

  private:
    std::string contents_, type_;
};
class ROMBank : public IOComponent {
  public:
    ROMBank(std::string name, size_t width, ROMDeviceConfig config = {});
    const char *getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder &) override;

  private:
    size_t width_;
    ROMDeviceConfig config_;
    std::string type_;
};
template <size_t W> class ROMStorageBank : public ROMBank {
  public:
    ROMStorageBank(std::string name, ROMDeviceConfig c = {}) : ROMBank(std::move(name), W, std::move(c)) {}
};
class ROMDevice : public IOComponent {
  public:
    ROMDevice(std::string name, size_t width, ROMDeviceConfig config = {});
    const char *getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder &) override;

  private:
    size_t width_;
    ROMDeviceConfig config_;
    std::string type_;
};
template <size_t W> class ROM : public ROMDevice {
  public:
    ROM(std::string name, ROMDeviceConfig c = {}) : ROMDevice(std::move(name), W, std::move(c)) {}
};
