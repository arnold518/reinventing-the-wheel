#include "modules/rv32i/RV32IMemoryRequest.hpp"
#include "modules/memory/MemoryStructuralBuilder.hpp"
#include "modules/basic/Mux.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Rewire.hpp"

using namespace circuit;
using memory_detail::StructuralWiring;

namespace {
// Fixed, combinational structural helpers. No new storage or behavioral model.
class RV32IMemoryAddress : public IOComponent {
public:
    explicit RV32IMemoryAddress(std::string name)
        : IOComponent(std::move(name), [](IOComponent* s) {
            s->addPin<32>("ADDR", PinType::INPUT);
            s->addPin<2>("SIZE", PinType::INPUT);
            s->addPin<32>("WORD_ADDR", PinType::OUTPUT);
            s->addPin<2>("OFFSET", PinType::OUTPUT);
            for (auto p : {"BYTE", "HALF", "WORD"}) s->addPin(p, PinType::OUTPUT);
        }) {}
    const char* getTypeName() const override { return "RV32IMemoryAddress"; }
    void buildInternals(ComponentBuilder& b) override {
        StructuralWiring w(b);
        auto zero = w.constant("ZERO", false);
        auto address = b.addNewComponent<BitSplitter<32>>("ADDRESS_BITS");
        auto size = b.addNewComponent<BitSplitter<2>>("SIZE_BITS");
        auto result = b.addNewComponent<BitJoiner<32>>("WORD_ADDRESS");
        auto offset = b.addNewComponent<BitJoiner<2>>("BYTE_OFFSET");
        w.connect(getInputPin<32>("ADDR"), address->getInputPin<32>("IN"));
        w.connect(getInputPin<2>("SIZE"), size->getInputPin<2>("IN"));
        auto a0 = address->getOutputPin("OUT_0"), a1 = address->getOutputPin("OUT_1");
        auto s0 = size->getOutputPin("OUT_0"), s1 = size->getOutputPin("OUT_1");
        auto not_byte_size = w.either("NOT_BYTE_SIZE", s0, s1);
        auto byte = w.invert("BYTE_SIZE", not_byte_size);
        auto ns1 = w.invert("NOT_SIZE_1", s1);
        auto half = w.both("HALF_SIZE", s0, ns1);
        auto ns0 = w.invert("NOT_SIZE_0", s0);
        auto word = w.both("WORD_SIZE", s1, ns0);
        auto not_byte = w.invert("NOT_BYTE", byte);
        auto bad_low = w.both("BAD_LOW_BIT", a0, not_byte);
        auto bad_word_high = w.both("WORD_HIGH_BIT", a1, word);
        auto reserved_size = w.both("RESERVED_SIZE", s0, s1);
        auto bad_high = w.either("BAD_HIGH_BIT", bad_word_high, reserved_size);
        auto bad = w.either("BAD_ALIGNMENT", bad_low, bad_high);
        // Preserve an invalid address for endpoint rejection; never round an
        // illegal CPU access into a legal aligned write.
        w.connect(bad, result->getInputPin("IN_0"));
        w.connect(zero, result->getInputPin("IN_1"));
        for (size_t i = 2; i < 32; ++i)
            w.connect(address->getOutputPin("OUT_" + std::to_string(i)), result->getInputPin("IN_" + std::to_string(i)));
        w.connect(a0, offset->getInputPin("IN_0"));
        w.connect(a1, offset->getInputPin("IN_1"));
        w.connect(result->getOutputPin<32>("OUT"), getOutputPin<32>("WORD_ADDR"));
        w.connect(offset->getOutputPin<2>("OUT"), getOutputPin<2>("OFFSET"));
        w.connect(byte, getOutputPin("BYTE"));
        w.connect(half, getOutputPin("HALF"));
        w.connect(word, getOutputPin("WORD"));
        w.finish();
    }
};

template<size_t Byte>
class RV32ILoadByte : public IOComponent {
public:
    explicit RV32ILoadByte(std::string name)
        : IOComponent(std::move(name), [](IOComponent* s) {
            s->addPin<8>("DATA", PinType::INPUT);
            for (auto p : {"BYTE", "HALF", "WORD", "BYTE_SIGN", "HALF_SIGN"}) s->addPin(p, PinType::INPUT);
            s->addPin<8>("OUT", PinType::OUTPUT);
        }) {}
    const char* getTypeName() const override {
        constexpr const char* types[]{"RV32ILoadByte<0>", "RV32ILoadByte<1>",
                                     "RV32ILoadByte<2>", "RV32ILoadByte<3>"};
        return types[Byte];
    }
    void buildInternals(ComponentBuilder& b) override {
        StructuralWiring w(b);
        auto bits = b.addNewComponent<BitSplitter<8>>("DATA");
        auto result = b.addNewComponent<BitJoiner<8>>("RESULT");
        w.connect(getInputPin<8>("DATA"), bits->getInputPin<8>("IN"));
        for (size_t i = 0; i < 8; ++i) {
            auto bit = bits->getOutputPin("OUT_" + std::to_string(i));
            auto byte = w.both("BYTE_" + std::to_string(i), getInputPin("BYTE"), Byte == 0 ? bit : getInputPin("BYTE_SIGN"));
            auto half = w.both("HALF_" + std::to_string(i), getInputPin("HALF"), Byte < 2 ? bit : getInputPin("HALF_SIGN"));
            auto word = w.both("WORD_" + std::to_string(i), getInputPin("WORD"), bit);
            auto value = w.reduce("RESULT_" + std::to_string(i), {byte, half, word}, false);
            w.connect(value, result->getInputPin("IN_" + std::to_string(i)));
        }
        w.connect(result->getOutputPin<8>("OUT"), getOutputPin<8>("OUT"));
        w.finish();
    }
};

class RV32ILoadExtender : public IOComponent {
public:
    explicit RV32ILoadExtender(std::string name)
        : IOComponent(std::move(name), [](IOComponent* s) {
            s->addPin<32>("DATA", PinType::INPUT);
            for (auto p : {"BYTE", "HALF", "WORD", "SIGN_EXTEND"}) s->addPin(p, PinType::INPUT);
            s->addPin<32>("OUT", PinType::OUTPUT);
        }) {}
    const char* getTypeName() const override { return "RV32ILoadExtender"; }
    void buildInternals(ComponentBuilder& b) override {
        StructuralWiring w(b);
        std::vector<Rewire::WireSpec> bytes;
        std::vector<Rewire::BitMap> split_maps, join_maps;
        for (size_t i = 0; i < 4; ++i) {
            const auto name = "BYTE_" + std::to_string(i);
            bytes.push_back({name, 8});
            for (size_t bit = 0; bit < 8; ++bit) {
                split_maps.emplace_back("DATA", i*8+bit, name, bit);
                join_maps.emplace_back(name, bit, "OUT", i*8+bit);
            }
        }
        auto split_outputs = bytes;
        split_outputs.push_back({"BYTE_SIGN", 1});
        split_outputs.push_back({"HALF_SIGN", 1});
        split_maps.emplace_back("DATA", 7, "BYTE_SIGN", 0);
        split_maps.emplace_back("DATA", 15, "HALF_SIGN", 0);
        auto split = b.addNewComponent<Rewire>("DATA_BYTES",
            std::vector<Rewire::WireSpec>{{"DATA",32}}, split_outputs, split_maps);
        auto join = b.addNewComponent<Rewire>("RESULT",
            bytes, std::vector<Rewire::WireSpec>{{"OUT",32}}, join_maps);
        w.connect(getInputPin<32>("DATA"), split->getInputPin<32>("DATA"));
        auto byte_sign = w.both("BYTE_SIGN", getInputPin("SIGN_EXTEND"), split->getOutputPin("BYTE_SIGN"));
        auto half_sign = w.both("HALF_SIGN", getInputPin("SIGN_EXTEND"), split->getOutputPin("HALF_SIGN"));
        auto add = [&]<size_t Byte>() {
            const auto name = "BYTE_" + std::to_string(Byte);
            auto unit = b.addNewComponent<RV32ILoadByte<Byte>>(name);
            w.connect(split->getOutputPin<8>(name), unit->template getInputPin<8>("DATA"));
            for (auto p : {"BYTE", "HALF", "WORD"}) w.connect(getInputPin(p), unit->template getInputPin<>(p));
            w.connect(byte_sign, unit->template getInputPin<>("BYTE_SIGN"));
            w.connect(half_sign, unit->template getInputPin<>("HALF_SIGN"));
            w.connect(unit->template getOutputPin<8>("OUT"), join->getInputPin<8>(name));
        };
        add.template operator()<0>(); add.template operator()<1>();
        add.template operator()<2>(); add.template operator()<3>();
        w.connect(join->getOutputPin<32>("OUT"), getOutputPin<32>("OUT"));
        w.finish();
    }
};

class RV32IByteWriteMask : public IOComponent {
public:
    explicit RV32IByteWriteMask(std::string name)
        : IOComponent(std::move(name), [](IOComponent* s) {
            s->addPin<2>("OFFSET", PinType::INPUT);
            for (auto p : {"BYTE", "HALF", "WORD"}) s->addPin(p, PinType::INPUT);
            s->addPin<4>("MASK", PinType::OUTPUT);
        }) {}
    const char* getTypeName() const override { return "RV32IByteWriteMask"; }
    void buildInternals(ComponentBuilder& b) override {
        StructuralWiring w(b);
        auto offset = b.addNewComponent<BitSplitter<2>>("OFFSET");
        auto mask = b.addNewComponent<BitJoiner<4>>("MASK");
        w.connect(getInputPin<2>("OFFSET"), offset->getInputPin<2>("IN"));
        auto a0 = offset->getOutputPin("OUT_0"), a1 = offset->getOutputPin("OUT_1");
        auto na0 = w.invert("NOT_A0", a0), na1 = w.invert("NOT_A1", a1);
        for (size_t i = 0; i < 4; ++i) {
            auto selected = w.both("SELECT_LANE_" + std::to_string(i), i & 1 ? a0 : na0, i & 2 ? a1 : na1);
            auto byte = w.both("BYTE_MASK_" + std::to_string(i), getInputPin("BYTE"), selected);
            auto half = w.both("HALF_MASK_" + std::to_string(i), getInputPin("HALF"), i < 2 ? na1 : a1);
            auto subword = w.either("SUBWORD_MASK_" + std::to_string(i), byte, half);
            auto value = w.either("MASK_" + std::to_string(i), getInputPin("WORD"), subword);
            w.connect(value, mask->getInputPin("IN_" + std::to_string(i)));
        }
        w.connect(mask->getOutputPin<4>("OUT"), getOutputPin<4>("MASK"));
        w.finish();
    }
};
}

RV32IMemoryLanes::RV32IMemoryLanes(std::string name)
    : IOComponent(std::move(name), [](IOComponent* s) {
        for (auto p : {"ADDR", "WRITE_DATA", "WORD_DATA"}) s->addPin<32>(p, PinType::INPUT);
        s->addPin<2>("SIZE", PinType::INPUT);
        s->addPin("SIGN_EXTEND", PinType::INPUT);
        for (auto p : {"WORD_ADDR", "WORD_WRITE_DATA", "READ_DATA"}) s->addPin<32>(p, PinType::OUTPUT);
        s->addPin<4>("WRITE_MASK", PinType::OUTPUT);
    }) {}

void RV32IMemoryLanes::buildInternals(ComponentBuilder& b) {
    StructuralWiring w(b);
    auto address = b.addNewComponent<RV32IMemoryAddress>("ADDRESS");
    auto store = b.addNewComponent<Mux4to1_32bit>("STORE_LANES");
    auto load = b.addNewComponent<Mux4to1_32bit>("LOAD_LANES");
    auto extend = b.addNewComponent<RV32ILoadExtender>("LOAD_EXTEND");
    auto mask = b.addNewComponent<RV32IByteWriteMask>("WRITE_MASK");
    w.connect(getInputPin<32>("ADDR"), address->getInputPin<32>("ADDR"));
    w.connect(getInputPin<2>("SIZE"), address->getInputPin<2>("SIZE"));
    w.connect(address->getOutputPin<32>("WORD_ADDR"), getOutputPin<32>("WORD_ADDR"));
    for (auto child : {std::static_pointer_cast<IOComponent>(store), std::static_pointer_cast<IOComponent>(load)})
        w.connect(address->getOutputPin<2>("OFFSET"), child->getInputPin<2>("SEL"));
    w.connect(address->getOutputPin<2>("OFFSET"), mask->getInputPin<2>("OFFSET"));
    for (auto p : {"BYTE", "HALF", "WORD"}) {
        w.connect(address->getOutputPin(p), extend->getInputPin(p));
        w.connect(address->getOutputPin(p), mask->getInputPin(p));
    }
    for (size_t offset = 0; offset < 4; ++offset) {
        auto routing = b.addNewComponent<Rewire>("LANE_" + std::to_string(offset),
            std::vector<Rewire::WireSpec>{{"STORE",32},{"LOAD",32}},
            std::vector<Rewire::WireSpec>{{"STORE",32},{"LOAD",32}},
            [offset] {
                auto maps = identity_mapping("STORE",0,32-offset*8,"STORE",offset*8);
                auto reads = identity_mapping("LOAD",offset*8,32-offset*8,"LOAD");
                maps.insert(maps.end(), reads.begin(), reads.end()); return maps;
            }(), Rewire::UnmappedBitValue::LOW);
        w.connect(getInputPin<32>("WRITE_DATA"), routing->getInputPin<32>("STORE"));
        w.connect(getInputPin<32>("WORD_DATA"), routing->getInputPin<32>("LOAD"));
        w.connect(routing->getOutputPin<32>("STORE"), store->getInputPin<32>("IN" + std::to_string(offset)));
        w.connect(routing->getOutputPin<32>("LOAD"), load->getInputPin<32>("IN" + std::to_string(offset)));
    }
    w.connect(store->getOutputPin<32>("OUT"), getOutputPin<32>("WORD_WRITE_DATA"));
    w.connect(load->getOutputPin<32>("OUT"), extend->getInputPin<32>("DATA"));
    w.connect(getInputPin("SIGN_EXTEND"), extend->getInputPin("SIGN_EXTEND"));
    w.connect(extend->getOutputPin<32>("OUT"), getOutputPin<32>("READ_DATA"));
    w.connect(mask->getOutputPin<4>("MASK"), getOutputPin<4>("WRITE_MASK"));
    w.finish();
}
