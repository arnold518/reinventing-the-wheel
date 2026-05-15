#include "modules/utility/Rewire.hpp"
#include "basic/PinBase.hpp"
#include "basic/Wire.hpp"
#include "basic/WireBase.hpp"
#include "simulator/Event.hpp"
#include "simulator/Simulator.hpp"
#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <utility>

namespace {
bool isSupportedWidth(size_t width) {
    switch (width) {
        case 1:
        case 2:
        case 3:
        case 4:
        case 8:
        case 16:
        case 32:
            return true;
        default:
            return false;
    }
}

std::shared_ptr<Event> makeWireUpdate(size_t time, const std::shared_ptr<WireBase>& wire,
                                      const std::vector<LogicValue>& values) {
    if (!wire) {
        return nullptr;
    }

    switch (wire->getWidth()) {
        case 1: return std::make_shared<WireUpdateEvent<1>>(time, std::dynamic_pointer_cast<Wire<1>>(wire), values);
        case 2: return std::make_shared<WireUpdateEvent<2>>(time, std::dynamic_pointer_cast<Wire<2>>(wire), values);
        case 3: return std::make_shared<WireUpdateEvent<3>>(time, std::dynamic_pointer_cast<Wire<3>>(wire), values);
        case 4: return std::make_shared<WireUpdateEvent<4>>(time, std::dynamic_pointer_cast<Wire<4>>(wire), values);
        case 8: return std::make_shared<WireUpdateEvent<8>>(time, std::dynamic_pointer_cast<Wire<8>>(wire), values);
        case 16: return std::make_shared<WireUpdateEvent<16>>(time, std::dynamic_pointer_cast<Wire<16>>(wire), values);
        case 32: return std::make_shared<WireUpdateEvent<32>>(time, std::dynamic_pointer_cast<Wire<32>>(wire), values);
        default: return nullptr;
    }
}
}

Rewire::WireSpec::WireSpec(std::string wire_name, size_t wire_width)
    : name(std::move(wire_name)), width(wire_width) {}

Rewire::BitMap::BitMap(std::string source_wire, size_t source_bit,
                       std::string destination_wire, size_t destination_bit)
    : src_wire(std::move(source_wire)),
      src_bit(source_bit),
      dst_wire(std::move(destination_wire)),
      dst_bit(destination_bit) {}

Rewire::Rewire(std::string name,
               std::vector<WireSpec> input_specs,
               std::vector<WireSpec> output_specs,
               std::vector<BitMap> mappings,
               UnmappedBitValue unmapped)
    : BasicComponent(std::move(name), 0,
          [inputs = input_specs, outputs = output_specs](IOComponent* self) {
              for (const auto& input : inputs) {
                  self->addPinDynamic(input.name, PinType::INPUT, input.width);
              }
              for (const auto& output : outputs) {
                  self->addPinDynamic(output.name, PinType::OUTPUT, output.width);
              }
          }),
      inputs(std::move(input_specs)),
      outputs(std::move(output_specs)),
      bit_mappings(std::move(mappings)),
      unmapped_default(unmapped) {
    for (const auto& mapping : bit_mappings) {
        mapped_output_bits.emplace(mapping.dst_wire, mapping.dst_bit);
    }

    if (!validateMappings()) {
        throw std::invalid_argument("Invalid Rewire bit mapping");
    }
}

bool Rewire::validateMappings() const {
    std::set<std::string> input_names;
    for (const auto& spec : inputs) {
        if (spec.name.empty()
            || !input_names.insert(spec.name).second
            || spec.width == 0
            || !isSupportedWidth(spec.width)) {
            return false;
        }
    }

    std::set<std::string> output_names;
    for (const auto& spec : outputs) {
        if (spec.name.empty()
            || !output_names.insert(spec.name).second
            || spec.width == 0
            || !isSupportedWidth(spec.width)) {
            return false;
        }
    }

    std::set<std::pair<std::string, size_t>> destination_bits;
    for (const auto& mapping : bit_mappings) {
        const auto* source = findWireSpec(inputs, mapping.src_wire);
        const auto* destination = findWireSpec(outputs, mapping.dst_wire);
        if (!source || !destination) {
            return false;
        }
        if (mapping.src_bit >= source->width || mapping.dst_bit >= destination->width) {
            return false;
        }
        if (!destination_bits.insert({mapping.dst_wire, mapping.dst_bit}).second) {
            return false;
        }
    }

    return true;
}

size_t Rewire::getTotalInputBits() const {
    size_t total = 0;
    for (const auto& input : inputs) {
        total += input.width;
    }
    return total;
}

size_t Rewire::getTotalOutputBits() const {
    size_t total = 0;
    for (const auto& output : outputs) {
        total += output.width;
    }
    return total;
}

void Rewire::evaluate(size_t current_time, Simulator& simulator) {
    std::map<std::string, std::vector<LogicValue>> output_values;
    for (const auto& output : outputs) {
        output_values[output.name] = std::vector<LogicValue>(output.width, getUnmappedValue());
    }

    for (const auto& mapping : bit_mappings) {
        auto input_pin = getInputPinDynamic(mapping.src_wire);
        if (!input_pin) {
            continue;
        }

        output_values[mapping.dst_wire][mapping.dst_bit] = input_pin->getBit(mapping.src_bit);
    }

    for (const auto& output : outputs) {
        auto output_pin = getOutputPinDynamic(output.name);
        if (!output_pin) {
            continue;
        }

        const auto& values = output_values[output.name];
        output_pin->setValueFromVector(values);
        if (auto wire = output_pin->getExternalWireBase()) {
            if (auto event = makeWireUpdate(current_time + getDelay(), wire, values)) {
                simulator.scheduleEvent(event);
            }
        }
    }
}

const Rewire::WireSpec* Rewire::findWireSpec(const std::vector<WireSpec>& specs, const std::string& wire_name) const {
    auto it = std::find_if(specs.begin(), specs.end(),
                           [&wire_name](const WireSpec& spec) { return spec.name == wire_name; });
    return it == specs.end() ? nullptr : &*it;
}

LogicValue Rewire::getUnmappedValue() const {
    switch (unmapped_default) {
        case UnmappedBitValue::LOW: return LogicValue::LOW;
        case UnmappedBitValue::HIGH: return LogicValue::HIGH;
        case UnmappedBitValue::UNKNOWN:
        default: return LogicValue::UNKNOWN;
    }
}

bool Rewire::isBitMapped(const std::string& wire_name, size_t bit_index) const {
    return mapped_output_bits.count({wire_name, bit_index}) > 0;
}

std::vector<Rewire::BitMap> identity_mapping(
    const std::string& src_wire,
    size_t src_offset,
    size_t count,
    const std::string& dst_wire,
    size_t dst_offset) {
    std::vector<Rewire::BitMap> mappings;
    mappings.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        mappings.emplace_back(src_wire, src_offset + i, dst_wire, dst_offset + i);
    }
    return mappings;
}

std::vector<Rewire::BitMap> unpack_mapping(
    const std::string& src_bus,
    size_t width,
    const std::vector<std::string>& dst_wires) {
    if (dst_wires.size() != width) {
        throw std::invalid_argument("unpack_mapping requires one destination wire per source bit");
    }

    std::vector<Rewire::BitMap> mappings;
    mappings.reserve(width);
    for (size_t i = 0; i < width; ++i) {
        mappings.emplace_back(src_bus, i, dst_wires[i], 0);
    }
    return mappings;
}

std::vector<Rewire::BitMap> pack_mapping(
    const std::vector<std::string>& src_wires,
    const std::string& dst_bus) {
    std::vector<Rewire::BitMap> mappings;
    mappings.reserve(src_wires.size());
    for (size_t i = 0; i < src_wires.size(); ++i) {
        mappings.emplace_back(src_wires[i], 0, dst_bus, i);
    }
    return mappings;
}

std::vector<Rewire::BitMap> slice_mapping(
    const std::string& src_wire,
    size_t start_bit,
    size_t length,
    const std::string& dst_wire) {
    return identity_mapping(src_wire, start_bit, length, dst_wire, 0);
}

std::vector<Rewire::BitMap> sign_extend_mapping(
    const std::string& src_wire,
    size_t src_width,
    const std::string& dst_wire,
    size_t dst_width) {
    if (src_width == 0 || dst_width < src_width) {
        throw std::invalid_argument("sign_extend_mapping requires 0 < src_width <= dst_width");
    }

    auto mappings = identity_mapping(src_wire, 0, src_width, dst_wire, 0);
    mappings.reserve(dst_width);
    for (size_t i = src_width; i < dst_width; ++i) {
        mappings.emplace_back(src_wire, src_width - 1, dst_wire, i);
    }
    return mappings;
}
