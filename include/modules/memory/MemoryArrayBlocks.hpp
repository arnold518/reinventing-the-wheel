#pragma once

#include "modules/memory/FFStorageBank.hpp"
#include "modules/memory/MemoryPeripheralWiring.hpp"
#include "modules/basic/Mux.hpp"
#include "modules/utility/Rewire.hpp"

// Structural IOComponents, not behavioral memory models. Runtime geometry is
// included in their type/layout names to keep saved layouts apart.
class MemoryAddressDecoder : public IOComponent {
public:
    MemoryAddressDecoder(std::string name, size_t depth, bool readonly)
        : IOComponent(std::move(name), [depth, readonly](IOComponent* self) {
            self->addPin<32>("WORD_ADDR", PinType::INPUT);
            self->addPin("IN_RANGE", PinType::OUTPUT);
            for (size_t bit = 0; bit < circuit::memory_detail::arrayAddressBits(depth); ++bit)
                self->addPin("SELECT_" + std::to_string(bit), PinType::OUTPUT);
            if (!readonly) for (size_t word = 0; word < depth; ++word)
                self->addPin("VALID_" + std::to_string(word), PinType::OUTPUT);
        }), depth_(depth), readonly_(readonly),
          type_("MemoryAddressDecoder<" + std::to_string(depth) + (readonly ? ",RO>" : ",RW>")) {
        circuit::memory_detail::arrayAddressBits(depth);
    }
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder& b) override {
        using namespace circuit::memory_detail;
        StructuralWiring w(b);
        auto address = b.addNewComponent<BitSplitter<32>>("ADDRESS");
        w.connect(getInputPin<32>("WORD_ADDR"), address->getInputPin<32>("IN"));
        const auto bits = arrayAddressBits(depth_);
        std::vector<StructuralWiring::PinPtr> positive, negative, high, selected;
        for (size_t bit = 0; bit < 32; ++bit) {
            auto pin = address->getOutputPin("OUT_" + std::to_string(bit));
            if (bit < bits) {
                positive.push_back(pin);
                negative.push_back(w.invert("NOT_A" + std::to_string(bit), pin));
                w.connect(pin, getOutputPin("SELECT_" + std::to_string(bit)));
            } else high.push_back(pin);
        }
        const auto highValid = w.invert("HIGH_VALID", w.reduce("HIGH", high, false));
        // A single word needs full-address validation but no word decoder.
        if (depth_ > 1) for (size_t word = 0; word < depth_; ++word) {
            selected.push_back(decodeMatch(w, "WORD_" + std::to_string(word) + "_SELECT",
                                           positive, negative, word));
        }
        const auto inRange = (size_t{1} << bits) == depth_ ? highValid :
            w.both("IN_RANGE", highValid, w.reduce("LOW_VALID", selected, false));
        w.connect(inRange, getOutputPin("IN_RANGE"));
        if (!readonly_) for (size_t word = 0; word < depth_; ++word) {
            auto valid = depth_ == 1 ? inRange : w.both("WORD_" + std::to_string(word) + "_VALID", selected[word], highValid);
            w.connect(valid, getOutputPin("VALID_" + std::to_string(word)));
        }
        w.finish();
    }
private:
    size_t depth_;
    bool readonly_;
    std::string type_;
};

class MemoryAccessControl : public IOComponent {
public:
    MemoryAccessControl(std::string name, size_t depth, bool readonly)
        : IOComponent(std::move(name), [depth, readonly](IOComponent* self) {
            for (const auto* pin : {"IN_RANGE", "READ_EN", "WE"}) self->addPin(pin, PinType::INPUT);
            for (const auto* pin : {"READ_VALID", "FAULT"}) self->addPin(pin, PinType::OUTPUT);
            if (!readonly) for (size_t word = 0; word < depth; ++word) {
                self->addPin("VALID_" + std::to_string(word), PinType::INPUT);
                self->addPin("WRITE_" + std::to_string(word), PinType::OUTPUT);
            }
        }), depth_(depth), readonly_(readonly),
          type_("MemoryAccessControl<" + std::to_string(depth) + (readonly ? ",RO>" : ",RW>")) {
        circuit::memory_detail::arrayAddressBits(depth);
    }
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder& b) override {
        circuit::memory_detail::StructuralWiring w(b);
        w.connect(w.both("READ_VALID", getInputPin("READ_EN"), getInputPin("IN_RANGE")), getOutputPin("READ_VALID"));
        auto active = w.either("ACTIVE", getInputPin("READ_EN"), getInputPin("WE"));
        auto rangeFault = w.both("RANGE_FAULT", active, w.invert("OUT_OF_RANGE", getInputPin("IN_RANGE")));
        w.connect(readonly_ ? w.either("FAULT_OUT", rangeFault, getInputPin("WE")) : rangeFault, getOutputPin("FAULT"));
        if (!readonly_) for (size_t word = 0; word < depth_; ++word) {
            const auto suffix = std::to_string(word);
            w.connect(w.both("WORD_" + suffix + "_WRITE", getInputPin("VALID_" + suffix), getInputPin("WE")),
                      getOutputPin("WRITE_" + suffix));
        }
        w.finish();
    }
private:
    size_t depth_;
    bool readonly_;
    std::string type_;
};

template<size_t Width>
class MemoryReadSelector : public IOComponent {
    static_assert(Width == 8 || Width == 16 || Width == 32);
public:
    MemoryReadSelector(std::string name, size_t depth)
        : IOComponent(std::move(name), [depth](IOComponent* self) {
            for (size_t word = 0; word < depth; ++word) self->addPin<Width>("WORD_" + std::to_string(word), PinType::INPUT);
            for (size_t bit = 0; bit < circuit::memory_detail::arrayAddressBits(depth); ++bit)
                self->addPin("SELECT_" + std::to_string(bit), PinType::INPUT);
            self->addPin("READ_VALID", PinType::INPUT);
            self->addPin<Width>("READ_DATA", PinType::OUTPUT);
        }), depth_(depth), type_("MemoryReadSelector<" + std::to_string(Width) + "," + std::to_string(depth) + ">") {
        circuit::memory_detail::arrayAddressBits(depth);
        if (depth > 4096 / Width) throw std::invalid_argument("MemoryReadSelector exceeds structural cell limit");
    }
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder& b) override {
        using namespace circuit::memory_detail;
        StructuralWiring w(b);
        std::vector<std::shared_ptr<Rewire>> words;
        for (size_t word = 0; word < depth_; ++word) {
            const auto name = "WORD_" + std::to_string(word);
            std::vector<Rewire::WireSpec> outputs;
            std::vector<Rewire::BitMap> mappings;
            for (size_t bit = 0; bit < Width; ++bit) {
                const auto out = "BIT_" + std::to_string(bit);
                outputs.emplace_back(out);
                mappings.emplace_back("WORD", bit, out, 0);
            }
            // Bus unpacking is wiring, not another one-tick BitSplitter stage.
            // Keep each word independent so a change doesn't unpack the bank.
            auto unpack = b.addNewComponent<Rewire>(name + "_BITS",
                std::vector<Rewire::WireSpec>{{"WORD", Width}}, outputs, mappings);
            w.connect(this->template getInputPin<Width>(name), unpack->getInputPinDynamic("WORD"));
            words.push_back(unpack);
        }
        auto zero = w.constant("ZERO", false);
        auto result = b.addNewComponent<BitJoiner<Width>>("RESULT");
        std::vector<StructuralWiring::PinPtr> selects;
        for (size_t bit = 0; bit < arrayAddressBits(depth_); ++bit)
            selects.push_back(this->getInputPin("SELECT_" + std::to_string(bit)));
        for (size_t bit = 0; bit < Width; ++bit) {
            std::vector<StructuralWiring::PinPtr> selected;
            for (size_t word = 0; word < depth_; ++word)
                selected.push_back(words[word]->getOutputPin("BIT_" + std::to_string(bit)));
            // Missing leaves are zero, not extra cells. One word bypasses all muxes.
            auto selectedBit = selectMuxTree(w, b, "READ_" + std::to_string(bit), std::move(selected), selects, zero);
            w.connect(w.both("ENABLE_" + std::to_string(bit), selectedBit, this->getInputPin("READ_VALID")),
                      result->getInputPin("IN_" + std::to_string(bit)));
        }
        w.connect(result->template getOutputPin<Width>("OUT"), this->template getOutputPin<Width>("READ_DATA"));
        w.finish();
    }
private:
    size_t depth_;
    std::string type_;
};
