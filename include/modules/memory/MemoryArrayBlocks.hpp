#pragma once

#include "modules/memory/FFStorageBank.hpp"
#include "modules/memory/MemoryPeripheralWiring.hpp"
#include "modules/basic/Mux.hpp"
#include "modules/utility/Rewire.hpp"
#include "modules/composite/WordSliceBuilder.hpp"

// Structural IOComponents, not behavioral memory models. Runtime geometry is
// included in their type/layout names to keep saved layouts apart.
class MemoryAddressDecoder : public IOComponent {
public:
    MemoryAddressDecoder(std::string name, size_t depth, bool readonly);
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder& b) override;
private:
    size_t depth_;
    bool readonly_;
    std::string type_;
};

class MemoryAccessControl : public IOComponent {
public:
    MemoryAccessControl(std::string name, size_t depth, bool readonly);
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder& b) override;
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
    // A tree of 2:1 word muxes over the words (a Mux<2,1> per bit, as a
    // bit-by-bit tree), missing leaves zero; READ_VALID, spread across the
    // word by wiring, masks the result in one word AND.
    void buildInternals(ComponentBuilder& b) override {
        using namespace circuit::memory_detail;
        circuit::detail::WordSliceBuilder n(*this, b);
        std::vector<circuit::detail::WordSliceBuilder::Signal> words;
        for (size_t word = 0; word < depth_; ++word) words.push_back(n.input("WORD_" + std::to_string(word)));
        circuit::detail::WordSliceBuilder::Signal zero;
        for (size_t level = 0; words.size() > 1; ++level) {
            decltype(words) next;
            for (size_t i = 0; i < words.size(); i += 2) {
                if (i + 1 == words.size() && !zero) zero = n.constant<Width>("ZERO", 0);
                next.push_back(n.wordMux("READ_" + std::to_string(level) + "_" + std::to_string(i / 2), words[i],
                    i + 1 < words.size() ? words[i + 1] : zero, n.input("SELECT_" + std::to_string(level))));
            }
            words = std::move(next);
        }
        n.connect(n.wordGate(circuit::word::GateKind::And, "ENABLE", words.front(),
                             n.broadcast("READ_VALID_WORD", n.input("READ_VALID"), Width)), n.output("READ_DATA"));
        n.finish();
    }
private:
    size_t depth_;
    std::string type_;
};
