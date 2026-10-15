#include "MemorySequencers.hpp"
#include "MemoryFFController.hpp"
#include "modules/memory/MemoryRangeCheck.hpp"
#include "modules/memory/MemoryTimingParts.hpp"
#include "modules/storage/ClockedStateBit.hpp"
#include "modules/storage/Register.hpp"
#include "modules/utility/Constant.hpp"
#include "components/ComponentBuilder.tpp"
#include <bit>

namespace circuit::memory_detail {
using circuit::memory::Technology;
using PinPtr = StructuralWiring::PinPtr;

namespace {
PinPtr constant3(ComponentBuilder& b, const std::string& name, unsigned value) {
    return b.addNewComponent<ConstantValue<3>>(name, value)->getOutputPin<3>("OUT");
}
std::string typeName(const char* kind, size_t width, Technology t) {
    return std::string(kind) + "<" + circuit::memory::technologyName(t) + "," + std::to_string(width) + ">";
}
}

void defineSequencerPins(IOComponent* s, size_t w, Technology t) {
    for (auto p : {"CLK", "RST", "START", "FINISH"}) s->addPin(p, PinType::INPUT);
    for (auto [name, width] : std::vector<std::pair<std::string, size_t>>{{"OP", 3}, {"ADDR", 32}, {"DATA", w}, {"MASK", w / 8}})
        s->addPinDynamic(name, PinType::INPUT, width);
    for (auto p : {"BACK_READY", "DONE"}) s->addPin(p, PinType::OUTPUT);
    s->addPinDynamic("RESULT", PinType::OUTPUT, w);
    s->addPin<3>("STATUS", PinType::OUTPUT);
    circuit::memory::nativePins(s, w, t, true);
}

MemoryRequestCheck::MemoryRequestCheck(std::string name, size_t width, Technology technology, circuit::memory::EndpointConfig config)
    : IOComponent(std::move(name), [width](IOComponent* s) {
          s->addPin<3>("OP", PinType::INPUT);
          s->addPin<32>("ADDR", PinType::INPUT);
          s->addPinDynamic("MASK", PinType::INPUT, width / 8);
          for (auto p : {"ACCEPT", "READ", "WRITE", "PROGRAM", "ERASE"}) s->addPin(p, PinType::OUTPUT);
          s->addPin<3>("STATUS", PinType::OUTPUT);
          s->addPin<32>("WORD_ADDR", PinType::OUTPUT);
      }),
      width_(width), technology_(technology), config_(config), type_(typeName("MemoryRequestCheck", width, technology)) {
    if (technology == Technology::NORFlash && !std::has_single_bit(config.block_words))
        throw std::invalid_argument("The gate-level NOR flash request check needs a power-of-two erase block");
}

void MemoryRequestCheck::buildInternals(ComponentBuilder& b) {
    StructuralWiring w(b);
    auto op = memory_device::bitAdapter(b, "OP_BITS", 3, true);
    auto addr = memory_device::bitAdapter(b, "ADDRESS_BITS", 32, true);
    auto known = b.addNewComponent<RequestKnownBits>("KNOWN_FIELDS", width_);
    w.connect(getInputPin<3>("OP"), {op->getInputPin<3>("IN"), known->getInputPin<3>("OP")});
    w.connect(getInputPin<32>("ADDR"), {addr->getInputPin<32>("IN"), known->getInputPin<32>("ADDR")});
    w.connect(getInputPinDynamic("MASK"), known->getInputPinDynamic("MASK"));
    auto o0 = op->getOutputPin("OUT_0"), o1 = op->getOutputPin("OUT_1"), o2 = op->getOutputPin("OUT_2");
    auto n0 = w.invert("NOT_OP0", o0), n1 = w.invert("NOT_OP1", o1), n2 = w.invert("NOT_OP2", o2);
    auto read = w.reduce("READ_OP", {n0, n1, n2}, true);
    auto write = w.reduce("WRITE_OP", {o0, n1, n2}, true);
    auto program = w.reduce("PROGRAM_OP", {n0, o1, n2}, true);
    auto erase = w.reduce("ERASE_OP", {o0, o1, n2}, true);
    const size_t shift = std::countr_zero(width_ / 8);
    std::vector<PinPtr> low, word;
    for (size_t i = 0; i < 32; ++i) (i < shift ? low : word).push_back(addr->getOutputPin("OUT_" + std::to_string(i)));
    auto unaligned = w.reduce("UNALIGNED", low, false);
    auto in_range = addressBelow(w, word, config_.depth_words, "RANGE");
    auto bad_mask = w.both("BAD_MASK", w.either("MASK_NEEDED", write, program),
                           w.invert("MASK_UNKNOWN", known->getOutputPin("MASK_KNOWN")));
    // Technology permission, checked after alignment and range.
    PinPtr refused = w.constant("ALLOWED", false);
    PinPtr status = constant3(b, "OK_CODE", unsigned(circuit::memory::Status::Ok));
    if (technology_ == Technology::ROM) {
        refused = w.invert("NOT_READ", read);
        status = payloadMux(b, w, "READ_ONLY_STATUS", status, constant3(b, "READ_ONLY_CODE", unsigned(circuit::memory::Status::ReadOnly)), refused);
    } else if (technology_ == Technology::NORFlash) {
        std::vector<PinPtr> block_low;
        for (size_t i = 0; i < size_t(std::countr_zero(config_.block_words)); ++i) block_low.push_back(word[i]);
        auto misaligned_block = w.both("BLOCK_MISALIGNED", erase, w.reduce("BLOCK_OFFSET", block_low, false));
        status = payloadMux(b, w, "BLOCK_STATUS", status, constant3(b, "BLOCK_ALIGNMENT_CODE", unsigned(circuit::memory::Status::Alignment)), misaligned_block);
        status = payloadMux(b, w, "WRITE_STATUS", status, constant3(b, "NO_WRITE_CODE", unsigned(circuit::memory::Status::Unsupported)), write);
        refused = w.either("NOR_REFUSED", write, misaligned_block);
    } else {
        refused = w.either("NOT_READ_WRITE", program, erase);
        status = payloadMux(b, w, "OPERATION_STATUS", status, constant3(b, "UNSUPPORTED_OPERATION_CODE", unsigned(circuit::memory::Status::Unsupported)), refused);
    }
    auto out_of_range = w.invert("OUT_OF_RANGE", in_range);
    status = payloadMux(b, w, "RANGE_STATUS", status, constant3(b, "RANGE_CODE", unsigned(circuit::memory::Status::Range)), out_of_range);
    status = payloadMux(b, w, "ALIGN_STATUS", status, constant3(b, "ALIGNMENT_CODE", unsigned(circuit::memory::Status::Alignment)), unaligned);
    status = payloadMux(b, w, "MASK_STATUS", status, constant3(b, "BAD_MASK_CODE", unsigned(circuit::memory::Status::InvalidRequest)), bad_mask);
    status = payloadMux(b, w, "RESERVED_STATUS", status, constant3(b, "RESERVED_CODE", unsigned(circuit::memory::Status::Unsupported)), o2);
    status = payloadMux(b, w, "KNOWN_STATUS", constant3(b, "UNKNOWN_CODE", unsigned(circuit::memory::Status::InvalidRequest)), status,
                        known->getOutputPin("FIELDS_KNOWN"));
    w.connect(status, getOutputPin<3>("STATUS"));
    w.connect(w.reduce("ACCEPTED", {known->getOutputPin("FIELDS_KNOWN"), n2, w.invert("MASK_OK", bad_mask),
                                    w.invert("ALIGNED", unaligned), in_range, w.invert("PERMITTED", refused)}, true),
              getOutputPin("ACCEPT"));
    for (auto [name, signal] : std::vector<std::pair<const char*, PinPtr>>{{"READ", read}, {"WRITE", write}, {"PROGRAM", program}, {"ERASE", erase}})
        w.connect(signal, getOutputPin(name));
    auto shifted = b.addNewComponent<BitJoiner<32>>("WORD_INDEX");
    const auto zero = w.constant("ZERO", false);
    for (size_t i = 0; i < 32; ++i) w.connect(i < word.size() ? word[i] : zero, shifted->getInputPin("IN_" + std::to_string(i)));
    w.connect(shifted->getOutputPin<32>("OUT"), getOutputPin<32>("WORD_ADDR"));
    w.finish();
}

StaticMemorySequencer::StaticMemorySequencer(std::string name, size_t width, Technology technology, circuit::memory::EndpointConfig config)
    : IOComponent(std::move(name), [width, technology](IOComponent* s) { defineSequencerPins(s, width, technology); }),
      width_(width), technology_(technology), config_(config), type_(typeName("StaticMemorySequencer", width, technology)) {
    if (technology != Technology::SRAM && technology != Technology::ROM)
        throw std::invalid_argument("The static sequencer drives SRAM or ROM");
}

void StaticMemorySequencer::buildInternals(ComponentBuilder& b) {
    StructuralWiring w(b);
    const bool sram = technology_ == Technology::SRAM;
    auto check = b.addNewComponent<MemoryRequestCheck>("REQUEST_CHECK", width_, technology_, config_);
    w.connect(getInputPin<3>("OP"), check->getInputPin<3>("OP"));
    w.connect(getInputPin<32>("ADDR"), check->getInputPin<32>("ADDR"));
    w.connect(getInputPinDynamic("MASK"), check->getInputPinDynamic("MASK"));
    const auto start = getInputPin("START");
    auto valid_start = w.both("VALID_START", start, check->getOutputPin("ACCEPT"));
    auto invalid_start = w.both("INVALID_START", start, w.invert("REFUSED", check->getOutputPin("ACCEPT")));

    auto state = [&](const char* name) {
        auto bit = b.add(circuit::families::ClockedStateBit, name);
        for (auto p : {"CLK", "RST"}) w.connect(getInputPin(p), bit->getInputPin(p));
        return bit;
    };
    auto reg = [&](const char* name, size_t width) {
        auto r = b.add(circuit::families::registerWord(width), name);
        for (auto p : {"CLK", "RST"}) w.connect(getInputPin(p), r->getInputPin(p));
        return r;
    };
    auto access = state("ACCESS");
    auto done = state("DONE");
    auto writing = state("IS_WRITE");
    auto sync = b.addNewComponent<circuit::memory::Synchronizer>("READY_SYNC");
    for (auto p : {"CLK", "RST"}) w.connect(getInputPin(p), sync->getInputPin(p));
    w.connect(getInputPin("MEM_READY"), sync->getInputPin("D"));
    const auto ready = sync->getOutputPin("Q");
    const auto active = access->getOutputPin("Q");
    auto complete = w.both("COMPLETE", active, ready);
    w.connect(w.either("NEXT_ACCESS", valid_start, w.both("STAY_ACCESS", active, w.invert("NOT_COMPLETE", complete))),
              access->getInputPin("D"));
    auto finished = w.either("FINISHED", invalid_start, complete);
    w.connect(w.either("NEXT_DONE", finished, w.both("KEEP_DONE", done->getOutputPin("Q"), w.invert("NOT_FINISH", getInputPin("FINISH")))),
              done->getInputPin("D"));
    // A clocked state bit has no enable: load the operation on START, hold it otherwise.
    w.connect(w.either("NEXT_WRITE", w.both("LOAD_WRITE", start, check->getOutputPin("WRITE")),
                       w.both("HOLD_WRITE", writing->getOutputPin("Q"), w.invert("NO_START", start))),
              writing->getInputPin("D"));
    const auto is_write = writing->getOutputPin("Q");

    auto word = reg("WORD_ADDRESS", 32);
    w.connect(start, word->getInputPin("WE"));
    w.connect(check->getOutputPin<32>("WORD_ADDR"), word->getInputPin<32>("D"));
    w.connect(word->getOutputPin<32>("Q"), getOutputPin<32>("MEM_WORD_ADDR"));
    if (sram) {
        auto data = reg("WRITE_DATA", width_);
        auto mask = reg("WRITE_MASK", width_ / 8);
        for (auto r : {data, mask}) w.connect(start, r->getInputPin("WE"));
        w.connect(getInputPinDynamic("DATA"), data->getInputPinDynamic("D"));
        w.connect(getInputPinDynamic("MASK"), mask->getInputPinDynamic("D"));
        w.connect(data->getOutputPinDynamic("Q"), getOutputPinDynamic("MEM_WRITE_DATA"));
        w.connect(mask->getOutputPinDynamic("Q"), getOutputPinDynamic("MEM_WRITE_MASK"));
        w.connect(w.invert("WRITE_STROBE", w.both("WRITE_ACCESS", active, is_write)), getOutputPin("MEM_WE_N"));
    }
    w.connect(w.invert("CHIP_SELECT", active), getOutputPin("MEM_CE_N"));
    w.connect(w.invert("OUTPUT_STROBE", w.both("READ_ACCESS", active, w.invert("IS_READ", is_write))), getOutputPin("MEM_OE_N"));

    auto result = reg("RESULT", width_);
    auto status = reg("RESULT_STATUS", 3);
    for (auto r : {result, status}) w.connect(finished, r->getInputPin("WE"));
    auto zero_data = memory_device::bitAdapter(b, "ZERO_DATA", width_, false);
    for (size_t i = 0; i < width_; ++i) w.connect(w.constant("ZERO_" + std::to_string(i), false), zero_data->getInputPin("IN_" + std::to_string(i)));
    auto returns_data = w.both("RETURNS_DATA", complete, w.invert("READ_RESULT", is_write));
    // The read word is taken only once READY has passed the synchronizer, two
    // or more edges after the device raised it with its data valid.
    auto held = b.addNewComponent<circuit::memory::ProtocolStableData>("READ_DATA_HELD", width_);
    w.connect(getInputPinDynamic("MEM_READ_DATA"), held->getInputPinDynamic("IN"));
    w.connect(payloadMux(b, w, "RESULT_SOURCE", zero_data->getOutputPinDynamic("OUT"), held->getOutputPinDynamic("OUT"), returns_data),
              result->getInputPinDynamic("D"));
    w.connect(payloadMux(b, w, "STATUS_SOURCE", constant3(b, "OK_CODE", 0), check->getOutputPin<3>("STATUS"), invalid_start),
              status->getInputPin<3>("D"));
    w.connect(result->getOutputPinDynamic("Q"), getOutputPinDynamic("RESULT"));
    w.connect(status->getOutputPin<3>("Q"), getOutputPin<3>("STATUS"));
    w.connect(done->getOutputPin("Q"), getOutputPin("DONE"));
    // Idle, and the device seen idle: its READY from the last access has
    // passed through the synchronizer as low.
    w.connect(w.both("BACK_READY_NOW", w.invert("IDLE", active), w.invert("READY_LOW", ready)), getOutputPin("BACK_READY"));
    w.finish();
}

} // namespace circuit::memory_detail
