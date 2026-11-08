#pragma once

#include "simulator/SimulationTime.hpp"

#include "components/BasicComponent.hpp"
#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include <set>

class Rewire : public BasicComponent {
public:
    enum class UnmappedBitValue {
        UNKNOWN,
        LOW,
        HIGH
    };

    struct WireSpec {
        std::string name;
        size_t width;

        WireSpec(std::string wire_name, size_t wire_width = 1);
    };

    struct BitMap {
        std::string src_wire;
        size_t src_bit;
        std::string dst_wire;
        size_t dst_bit;

        BitMap(std::string source_wire, size_t source_bit,
               std::string destination_wire, size_t destination_bit);
    };

    Rewire(std::string name,
           std::vector<WireSpec> input_specs,
           std::vector<WireSpec> output_specs,
           std::vector<BitMap> mappings,
           UnmappedBitValue unmapped = UnmappedBitValue::UNKNOWN);

    static constexpr const char* TypeName = "Rewire";
    const char* getTypeName() const override { return TypeName; }
    StateSupport stateSupport() const override { return StateSupport::Stateless; }
    bool validateMappings() const;
    size_t getTotalInputBits() const;
    size_t getTotalOutputBits() const;
    const std::vector<WireSpec>& getInputSpecs() const { return inputs; }
    const std::vector<WireSpec>& getOutputSpecs() const { return outputs; }
    const std::vector<BitMap>& getBitMappings() const { return bit_mappings; }
    UnmappedBitValue getUnmappedDefault() const { return unmapped_default; }
    void evaluate(SimulationTime current_time, Simulator& simulator) override;

protected:
    void pinsChanged() noexcept override { routes_resolved_ = false; }

private:
    // Bit routes by spec index, resolved to pin handles once; any pin-table
    // change clears them.
    struct Route { size_t input, src_bit, output, dst_bit; };
    bool routes_resolved_ = false;
    std::vector<std::shared_ptr<PinBase>> input_pins_, output_pins_;
    std::vector<Route> routes_;
    std::vector<PackedLogic> output_values_;  // per evaluation, kept to reuse its storage
    void resolveRoutes();
    std::vector<WireSpec> inputs;
    std::vector<WireSpec> outputs;
    std::vector<BitMap> bit_mappings;
    UnmappedBitValue unmapped_default;
    std::set<std::pair<std::string, size_t>> mapped_output_bits;

    const WireSpec* findWireSpec(const std::vector<WireSpec>& specs, const std::string& wire_name) const;
    LogicValue getUnmappedValue() const;
    bool isBitMapped(const std::string& wire_name, size_t bit_index) const;
};

std::vector<Rewire::BitMap> identity_mapping(
    const std::string& src_wire,
    size_t src_offset,
    size_t count,
    const std::string& dst_wire,
    size_t dst_offset = 0);

std::vector<Rewire::BitMap> unpack_mapping(
    const std::string& src_bus,
    size_t width,
    const std::vector<std::string>& dst_wires);

std::vector<Rewire::BitMap> pack_mapping(
    const std::vector<std::string>& src_wires,
    const std::string& dst_bus);

std::vector<Rewire::BitMap> slice_mapping(
    const std::string& src_wire,
    size_t start_bit,
    size_t length,
    const std::string& dst_wire);

std::vector<Rewire::BitMap> sign_extend_mapping(
    const std::string& src_wire,
    size_t src_width,
    const std::string& dst_wire,
    size_t dst_width);
