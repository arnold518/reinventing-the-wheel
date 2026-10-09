#pragma once

#include "components/BasicComponent.hpp"
#include "components/capabilities/MemoryStateView.hpp"
#include "components/selection/ComponentCatalog.hpp"
#include "modules/memory/MemoryStructuralBuilder.hpp"
#include "modules/memory/MemoryDeviceStorage.hpp"
#include <optional>

namespace circuit::memory_device {
using Bits = std::vector<LogicValue>;
inline constexpr auto L = LogicValue::LOW, H = LogicValue::HIGH, X = LogicValue::UNKNOWN,
                      Z = LogicValue::HIGH_Z;
bool binary(LogicValue value);
bool known(const Bits &bits);
uint32_t number(const Bits &bits);
Bits bits(size_t width, uint32_t value);
LogicValue stored(LogicValue value);
size_t parameter(const ParameterMap &params, const std::string &key, size_t fallback);
void keys(const ParameterMap &params, std::initializer_list<const char *> allowed);
bool historyParameter(const ParameterMap &params);
MemoryStorage::Limits storageLimits(const ParameterMap &params);
ParameterMap storageParameters(const MemoryStorage::Limits &limits);

struct Geometry {
    size_t depth_words = 4, words_per_row = 2;
    bool record_history = true;
    MemoryStorage::Limits storage_limits{true, 64 * 1024 * 1024, 16 * 1024 * 1024};
    size_t rows() const { return depth_words / words_per_row; }
    size_t columns(size_t width) const { return words_per_row * width; }
    void validate(size_t width, bool expanded = false) const;
};

// The settings every matrix device shares (depth, words per row, history and
// storage budgets), read from and written to a parameter map in one place.
void readGeometry(const ParameterMap &params, Geometry &geometry);
ParameterMap geometryParameters(const Geometry &geometry);
// Common event/pin plumbing, never a common cell behavior.
class Primitive : public BasicComponent {
  public:
    Primitive(std::string name, std::string type, PinInitFunction pins)
        : BasicComponent(std::move(name), 1, std::move(pins)), type_(std::move(type)) {}
    const char *getTypeName() const override { return type_.c_str(); }
    // True for a primitive whose outputs change only one delay after an
    // input change: it never wakes itself for a later, timed step. The timing
    // analysis may then treat it as "every input reaches every output in
    // one delay", whatever it remembers inside.
    virtual bool respondsOnlyToInputs() const { return false; }
    void scheduleInitialEvents(Simulator &sim, size_t time) override {
        if (getAllInputPins().empty())
            wake(sim, time);
    }

  protected:
    Bits read(const std::string &pin) const { return getInputPinDynamic(pin)->getValueAsVector(); }
    void publish(Simulator &sim, size_t time, const std::string &pin, const Bits &value);
    void publish(Simulator &sim, size_t time, const std::string &pin, LogicValue value) {
        publish(sim, time, pin, Bits{value});
    }
    void wake(Simulator &sim, size_t time);

  private:
    std::string type_;
    std::map<std::string, Bits> published_;
};

using Schema = std::function<std::vector<PinDescriptor>(const ParameterMap &)>;
std::vector<PinDescriptor> describe(IOComponent::PinInitFunction pins);
void registerBlock(ComponentCatalog &catalog, const std::string &id, const std::string &type, Schema schema,
                   ComponentFactory structural, ComponentFactory behavioral, const std::string &test,
                   bool terminal = false, ParameterPredicate structural_support = {});
std::shared_ptr<IOComponent> child(ComponentBuilder &builder, IOComponent &parent, const std::string &id,
                                   const std::string &name, ParameterMap params = {});
std::shared_ptr<IOComponent> bitAdapter(ComponentBuilder &b, const std::string &name, size_t width,
                                        bool split);
void registerMatrixComponents(ComponentCatalog &catalog);

class MatrixRowDecoder : public IOComponent {
  public:
    MatrixRowDecoder(std::string name, size_t rows);
    const char *getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder &) override;

  private:
    size_t rows_;
    std::string type_;
};
class MatrixColumnMux : public IOComponent {
  public:
    MatrixColumnMux(std::string name, size_t width, size_t words_per_row);
    const char *getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder &) override;

  private:
    size_t width_, k_;
    std::string type_;
};
class MatrixWriteDistributor : public IOComponent {
  public:
    MatrixWriteDistributor(std::string name, size_t width, size_t words_per_row);
    const char *getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder &) override;

  private:
    size_t width_, k_;
    std::string type_;
};
// Symbolic single-selected-row sensing, not electrical multi-driver resolution.
class StaticReadColumn : public Primitive {
  public:
    StaticReadColumn(std::string name, size_t rows);
    void evaluate(size_t, Simulator &) override;
    bool respondsOnlyToInputs() const override { return true; }

  private:
    size_t rows_;
};
class MemoryOutputBuffer : public Primitive {
  public:
    MemoryOutputBuffer(std::string name, size_t width);
    void evaluate(size_t, Simulator &) override;
    bool respondsOnlyToInputs() const override { return true; }

  private:
    size_t width_;
};
} // namespace circuit::memory_device
