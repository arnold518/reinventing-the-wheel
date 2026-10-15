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

CommandMemorySequencer::CommandMemorySequencer(std::string name, size_t width, Technology technology, circuit::memory::EndpointConfig config)
    : IOComponent(std::move(name), [width, technology](IOComponent* s) { defineSequencerPins(s, width, technology); }),
      width_(width), technology_(technology), config_(config), type_(typeName("CommandMemorySequencer", width, technology)) {
    if (technology != Technology::DRAM && technology != Technology::NORFlash)
        throw std::invalid_argument("The command sequencer drives DRAM or NOR flash");
    if (!std::has_single_bit(config.words_per_row))
        throw std::invalid_argument("The gate-level command sequencer needs power-of-two rows");
}

void CommandMemorySequencer::buildInternals(ComponentBuilder& b) {
    StructuralWiring w(b);
    const bool dram = technology_ == Technology::DRAM;
    auto check = b.addNewComponent<MemoryRequestCheck>("REQUEST_CHECK", width_, technology_, config_);
    w.connect(getInputPin<3>("OP"), check->getInputPin<3>("OP"));
    w.connect(getInputPin<32>("ADDR"), check->getInputPin<32>("ADDR"));
    w.connect(getInputPinDynamic("MASK"), check->getInputPinDynamic("MASK"));
    const auto start = getInputPin("START");
    auto valid_start = w.both("VALID_START", start, check->getOutputPin("ACCEPT"));
    auto invalid_start = w.both("INVALID_START", start, w.invert("REFUSED", check->getOutputPin("ACCEPT")));
    auto state = [&](const std::string& name) {
        auto bit = b.add(circuit::families::ClockedStateBit, name);
        for (auto p : {"CLK", "RST"}) w.connect(getInputPin(p), bit->getInputPin(p));
        return bit;
    };
    auto reg = [&](const std::string& name, size_t width) {
        auto r = b.add(circuit::families::registerWord(width), name);
        for (auto p : {"CLK", "RST"}) w.connect(getInputPin(p), r->getInputPin(p));
        return r;
    };
    auto set_hold = [&](const std::shared_ptr<IOComponent>& bit, PinPtr set, PinPtr clear, const std::string& name) {
        w.connect(w.either("NEXT_" + name, set, w.both("HOLD_" + name, bit->getOutputPin("Q"), w.invert("NO_CLEAR_" + name, clear))),
                  bit->getInputPin("D"));
    };
    auto active = state("ACTIVE"), issuing = state("ISSUE"), waiting = state("WAIT"), done = state("DONE");
    const auto A = active->getOutputPin("Q"), I = issuing->getOutputPin("Q"), WT = waiting->getOutputPin("Q");
    // Request registers.
    auto word = reg("WORD", 32), data = reg("WRITE_DATA", width_), mask = reg("WRITE_MASK", width_ / 8);
    for (auto r : {word, data, mask}) w.connect(start, r->getInputPin("WE"));
    w.connect(check->getOutputPin<32>("WORD_ADDR"), word->getInputPin<32>("D"));
    w.connect(getInputPinDynamic("DATA"), data->getInputPinDynamic("D"));
    w.connect(getInputPinDynamic("MASK"), mask->getInputPinDynamic("D"));
    w.connect(data->getOutputPinDynamic("Q"), getOutputPinDynamic("MEM_WRITE_DATA"));
    w.connect(mask->getOutputPinDynamic("Q"), getOutputPinDynamic("MEM_WRITE_MASK"));
    auto op_bit = [&](const std::string& name, PinPtr decoded) {
        auto bit = state(name);
        w.connect(w.either("NEXT_" + name, w.both("LOAD_" + name, start, decoded),
                           w.both("HOLD_" + name, bit->getOutputPin("Q"), w.invert("NO_START_" + name, start))),
                  bit->getInputPin("D"));
        return bit->getOutputPin("Q");
    };
    const auto is_write = op_bit("IS_WRITE", check->getOutputPin("WRITE"));
    const auto is_program = op_bit("IS_PROGRAM", check->getOutputPin("PROGRAM"));
    const auto is_erase = op_bit("IS_ERASE", check->getOutputPin("ERASE"));
    // Fields of the live and the held word address.
    const size_t column_bits = std::countr_zero(config_.words_per_row);
    auto live_bits = memory_device::bitAdapter(b, "LIVE_WORD_BITS", 32, true);
    auto held_bits = memory_device::bitAdapter(b, "HELD_WORD_BITS", 32, true);
    w.connect(check->getOutputPin<32>("WORD_ADDR"), live_bits->getInputPin<32>("IN"));
    w.connect(word->getOutputPin<32>("Q"), held_bits->getInputPin<32>("IN"));
    std::vector<PinPtr> live, held;
    for (size_t i = 0; i < 32; ++i) {
        live.push_back(live_bits->getOutputPin("OUT_" + std::to_string(i)));
        held.push_back(held_bits->getOutputPin("OUT_" + std::to_string(i)));
    }
    const auto low = w.constant("LOW", false);
    auto field = [&](const std::string& name, const std::vector<PinPtr>& bits, size_t from, size_t count) {
        auto join = memory_device::bitAdapter(b, name, 32, false);
        for (size_t i = 0; i < 32; ++i) w.connect(i < count && from + i < 32 ? bits[from + i] : low, join->getInputPin("IN_" + std::to_string(i)));
        return join->getOutputPinDynamic("OUT");
    };
    // The step chosen from the pending steps (a priority list) and its code.
    std::vector<std::shared_ptr<IOComponent>> needs;
    std::vector<PinPtr> pending;
    auto current = reg("COMMAND", 3);
    auto device_busy = getInputPin("MEM_BUSY");
    auto step_done = w.both("STEP_DONE", WT, w.invert("DEVICE_IDLE", device_busy));
    auto cur_bits = memory_device::bitAdapter(b, "COMMAND_BITS", 3, true);
    w.connect(current->getOutputPin<3>("Q"), cur_bits->getInputPin<3>("IN"));
    auto code_is = [&](unsigned code, const std::string& name) {
        std::vector<PinPtr> terms;
        for (size_t i = 0; i < 3; ++i) {
            auto bit = cur_bits->getOutputPin("OUT_" + std::to_string(i));
            terms.push_back((code >> i) & 1 ? bit : w.invert(name + "_NOT_" + std::to_string(i), bit));
        }
        return w.reduce(name, terms, true);
    };
    auto cur_read = dram ? code_is(2, "CURRENT_READ") : code_is(1, "CURRENT_READ");
    auto fail = w.both("STEP_FAILED", step_done, w.either("DEVICE_FAULT", getInputPin("MEM_ERROR"),
        w.both("NO_READ_DATA", cur_read, w.invert("READ_NOT_VALID", getInputPin("MEM_READ_VALID")))));
    PinPtr refreshing_q = low, pending_refresh = low, ref_start = low;
    std::shared_ptr<IOComponent> refreshing, refresh_pending, row_known, open_row;
    PinPtr hit = low;
    if (dram) {
        refreshing = state("REFRESHING");
        refresh_pending = state("REFRESH_PENDING");
        row_known = state("ROW_KNOWN");
        open_row = reg("OPEN_ROW", 32);
        refreshing_q = refreshing->getOutputPin("Q");
        pending_refresh = refresh_pending->getOutputPin("Q");
        ref_start = w.reduce("REFRESH_START", {pending_refresh, w.invert("NOT_ACTIVE", A), w.invert("NO_REQUEST", start)}, true);
        // The requested row is the open one: the controller knows the device's
        // open row and it matches.
        auto stored = memory_device::bitAdapter(b, "OPEN_ROW_BITS", 32, true);
        w.connect(open_row->getOutputPin<32>("Q"), stored->getInputPin<32>("IN"));
        std::vector<PinPtr> same;
        for (size_t i = 0; i + column_bits < 32; ++i)
            same.push_back(w.invert("ROW_SAME_" + std::to_string(i),
                w.binary<XORGate>("ROW_DIFFER_" + std::to_string(i), stored->getOutputPin("OUT_" + std::to_string(i)), live[i + column_bits])));
        hit = w.reduce("ROW_HIT", {getInputPin("MEM_ROW_OPEN"), row_known->getOutputPin("Q"), w.reduce("SAME_ROW", same, true)}, true);
    }
    // Steps in priority order. DRAM: PRECHARGE, ACTIVATE, READ/WRITE, REFRESH.
    // NOR: WREN, then the operation.
    struct Step { std::string name; PinPtr planned_request, planned_refresh; unsigned code; PinPtr code_override; };
    std::vector<Step> steps;
    if (dram) {
        auto row_open = getInputPin("MEM_ROW_OPEN");
        steps = {{"PRECHARGE", w.both("PLAN_PRECHARGE", row_open, w.invert("MISS", hit)), row_open, 4, nullptr},
                 {"ACTIVATE", w.invert("PLAN_ACTIVATE", hit), low, 1, nullptr},
                 {"ACCESS", w.constant("PLAN_ACCESS", true), low, 2, nullptr},
                 {"REFRESH", low, w.constant("PLAN_REFRESH", true), 5, nullptr}};
    } else {
        steps = {{"WREN", w.either("PLAN_WREN", check->getOutputPin("PROGRAM"), check->getOutputPin("ERASE")), low, 4, nullptr},
                 {"OPERATION", w.constant("PLAN_OPERATION", true), low, 1, nullptr}};
    }
    // The pending list in force this cycle: a new request's or a refresh's
    // plan on the edge that starts it, the held list otherwise.
    std::vector<PinPtr> list;
    for (auto& step : steps) {
        auto need = state("NEED_" + step.name);
        needs.push_back(need);
        list.push_back(w.reduce("PENDING_" + step.name, {
            w.both("FROM_REQUEST_" + step.name, valid_start, step.planned_request),
            w.both("FROM_REFRESH_" + step.name, ref_start, step.planned_refresh),
            w.both("HELD_" + step.name, need->getOutputPin("Q"), w.invert("FRESH_" + step.name, w.either("FRESH_PLAN_" + step.name, valid_start, ref_start)))}, false));
    }
    auto any_pending = w.reduce("ANY_PENDING", list, false);
    // Issue now: a fresh plan, or the previous step finished without fault.
    auto proceed = w.both("PROCEED", step_done, w.invert("NO_FAULT", fail));
    auto issue_now = w.both("ISSUE_NOW", any_pending, w.reduce("ISSUE_TIME", {valid_start, ref_start, proceed}, false));
    std::vector<PinPtr> selected;
    PinPtr earlier = low;
    for (size_t k = 0; k < steps.size(); ++k) {
        selected.push_back(w.both("SELECT_" + steps[k].name, list[k], w.invert("NONE_BEFORE_" + steps[k].name, earlier)));
        earlier = w.either("BEFORE_" + std::to_string(k + 1), earlier, list[k]);
    }
    for (size_t k = 0; k < steps.size(); ++k) {
        // A pending step stays until it is issued; every step clears on a fault or a finished transaction.
        auto taken = w.both("TAKEN_" + steps[k].name, issue_now, selected[k]);
        w.connect(w.reduce("NEXT_NEED_" + steps[k].name, {list[k], w.invert("NOT_TAKEN_" + steps[k].name, taken), w.invert("NOT_FAILED_" + steps[k].name, fail)}, true),
                  needs[k]->getInputPin("D"));
    }
    // The command code of the selected step. On the starting edge the kind
    // comes from the new request (the kind bits still hold the last one's).
    auto not_starting = w.invert("NOT_STARTING", valid_start);
    auto kind = [&](const std::string& name, PinPtr held, PinPtr decoded) {
        return w.either(name + "_KIND", w.both(name + "_HELD", held, not_starting), w.both(name + "_AT_START", valid_start, decoded));
    };
    auto code_bit = [&](size_t bit) {
        std::vector<PinPtr> terms;
        for (size_t k = 0; k < steps.size(); ++k) {
            unsigned code = steps[k].code;
            PinPtr term = (code >> bit) & 1 ? selected[k] : low;
            if (dram && steps[k].name == "ACCESS" && bit == 0) term = w.both("WRITE_CODE", selected[k], kind("WRITE", is_write, check->getOutputPin("WRITE")));
            if (!dram && steps[k].name == "OPERATION") {
                const auto prog = kind("PROGRAM_" + std::to_string(bit), is_program, check->getOutputPin("PROGRAM"));
                const auto erase = kind("ERASE_" + std::to_string(bit), is_erase, check->getOutputPin("ERASE"));
                // READ 1, PROGRAM 2, ERASE 3.
                term = bit == 0 ? w.both("OP_CODE0", selected[k], w.invert("NOT_PROGRAM_CODE", w.both("ONLY_PROGRAM", prog, w.invert("NOT_ERASE_CODE", erase))))
                     : bit == 1 ? w.both("OP_CODE1", selected[k], w.either("PROGRAM_OR_ERASE", prog, erase)) : low;
            }
            terms.push_back(term);
        }
        return w.reduce("CODE_BIT_" + std::to_string(bit), terms, false);
    };
    auto next_code = memory_device::bitAdapter(b, "NEXT_COMMAND", 3, false);
    for (size_t bit = 0; bit < 3; ++bit) w.connect(code_bit(bit), next_code->getInputPin("IN_" + std::to_string(bit)));
    w.connect(next_code->getOutputPinDynamic("OUT"), current->getInputPin<3>("D"));
    w.connect(issue_now, current->getInputPin("WE"));
    // Sequence state.
    auto finish_now = w.both("TRANSACTION_ENDS", step_done, w.either("NOTHING_LEFT", fail, w.invert("NO_MORE", any_pending)));
    set_hold(issuing, issue_now, w.constant("ISSUE_ONE_CYCLE", true), "ISSUE");
    w.connect(w.either("NEXT_WAIT", I, w.both("KEEP_WAITING", WT, w.invert("STEP_OVER", step_done))), waiting->getInputPin("D"));
    set_hold(active, w.either("BEGINS", valid_start, ref_start), finish_now, "ACTIVE");
    auto request_ends = w.both("REQUEST_ENDS", finish_now, w.invert("NOT_REFRESH", refreshing_q));
    set_hold(done, w.either("SHOW_RESULT", request_ends, invalid_start), getInputPin("FINISH"), "DONE");
    // Result and status.
    auto result = reg("RESULT", width_), status = reg("RESULT_STATUS", 3);
    auto capture = w.either("CAPTURE", w.both("READ_CAPTURE", step_done, cur_read), w.either("CAPTURE_END", request_ends, invalid_start));
    w.connect(capture, result->getInputPin("WE"));
    w.connect(w.either("STATUS_UPDATE", request_ends, invalid_start), status->getInputPin("WE"));
    auto zero_data = memory_device::bitAdapter(b, "ZERO_DATA", width_, false);
    for (size_t i = 0; i < width_; ++i) w.connect(low, zero_data->getInputPin("IN_" + std::to_string(i)));
    // Read data are stable once the device has released BUSY with READ_VALID.
    auto held_data = b.addNewComponent<circuit::memory::ProtocolStableData>("READ_DATA_HELD", width_);
    w.connect(getInputPinDynamic("MEM_READ_DATA"), held_data->getInputPinDynamic("IN"));
    auto keep_data = w.reduce("RETURN_DATA", {step_done, cur_read, w.invert("READ_OK", fail)}, true);
    w.connect(payloadMux(b, w, "RESULT_SOURCE", zero_data->getOutputPinDynamic("OUT"), held_data->getOutputPinDynamic("OUT"), keep_data),
              result->getInputPinDynamic("D"));
    auto device_status = payloadMux(b, w, "DEVICE_STATUS", constant3(b, "OK_CODE", 0), constant3(b, "DEVICE_CODE", unsigned(circuit::memory::Status::Device)), fail);
    w.connect(payloadMux(b, w, "STATUS_SOURCE", device_status, check->getOutputPin<3>("STATUS"), invalid_start), status->getInputPin<3>("D"));
    w.connect(result->getOutputPinDynamic("Q"), getOutputPinDynamic("RESULT"));
    w.connect(status->getOutputPin<3>("Q"), getOutputPin<3>("STATUS"));
    w.connect(done->getOutputPin("Q"), getOutputPin("DONE"));
    // Device pins: the command for one cycle, NOP otherwise; the address field
    // the command needs.
    auto shown = memory_device::bitAdapter(b, "SHOWN_COMMAND", 3, false);
    for (size_t bit = 0; bit < 3; ++bit) w.connect(w.both("SHOW_" + std::to_string(bit), I, cur_bits->getOutputPin("OUT_" + std::to_string(bit))), shown->getInputPin("IN_" + std::to_string(bit)));
    w.connect(shown->getOutputPinDynamic("OUT"), getOutputPin<3>("MEM_CMD"));
    w.connect(memory_detail::bridge(b, w, "DEVICE_CLOCK", getInputPin("CLK")), getOutputPin("MEM_CLK"));
    if (dram) {
        auto row = field("ROW_FIELD", held, column_bits, 32 - column_bits), column = field("COLUMN_FIELD", held, 0, column_bits);
        w.connect(payloadMux(b, w, "ADDRESS_FIELD", column, row, code_is(1, "CURRENT_ACTIVATE")), getOutputPin<32>("MEM_ADDR"));
        // Open-row knowledge: an ACTIVATE that succeeds records its row; a reset
        // forgets it (the device may still hold a row open: precharge first).
        auto opened = w.reduce("ROW_OPENED", {step_done, code_is(1, "OPENED_BY_ACTIVATE"), w.invert("ACTIVATE_OK", fail)}, true);
        set_hold(row_known, opened, low, "ROW_KNOWN");
        w.connect(row, open_row->getInputPin<32>("D"));
        w.connect(opened, open_row->getInputPin("WE"));
        // Refresh: a cycle counter and a pending bit.
        const size_t interval = config_.refresh_interval_cycles;
        PinPtr due = low;
        if (interval) {
            // At least two bits: a one-bit register needs no bit adapters.
            const size_t bits = supportedWidth(std::max<size_t>(2, std::bit_width(interval - 1)));
            auto counter = reg("REFRESH_COUNTER", bits);
            auto q = memory_device::bitAdapter(b, "REFRESH_COUNTER_BITS", bits, true);
            w.connect(counter->getOutputPinDynamic("Q"), q->getInputPinDynamic("IN"));
            std::vector<PinPtr> last;
            PinPtr carry = w.constant("COUNT", true);
            auto next = memory_device::bitAdapter(b, "NEXT_REFRESH_COUNTER", bits, false);
            std::vector<PinPtr> sums;
            for (size_t i = 0; i < bits; ++i) {
                auto bit = q->getOutputPin("OUT_" + std::to_string(i));
                sums.push_back(w.binary<XORGate>("COUNT_SUM_" + std::to_string(i), bit, carry));
                carry = w.both("COUNT_CARRY_" + std::to_string(i), bit, carry);
                last.push_back(((interval - 1) >> i) & 1 ? bit : w.invert("COUNT_ZERO_" + std::to_string(i), bit));
            }
            due = w.reduce("REFRESH_DUE", last, true);
            for (size_t i = 0; i < bits; ++i)
                w.connect(w.both("COUNT_NEXT_" + std::to_string(i), sums[i], w.invert("COUNT_WRAP_" + std::to_string(i), due)), next->getInputPin("IN_" + std::to_string(i)));
            w.connect(next->getOutputPinDynamic("OUT"), counter->getInputPinDynamic("D"));
            w.connect(w.constant("COUNT_ALWAYS", true), counter->getInputPin("WE"));
        }
        set_hold(refresh_pending, due, ref_start, "REFRESH_PENDING");
        set_hold(refreshing, ref_start, finish_now, "REFRESHING");
        w.connect(w.reduce("BACK_READY_NOW", {w.invert("IDLE", A), w.invert("NO_REFRESH_DUE", pending_refresh)}, true), getOutputPin("BACK_READY"));
    } else {
        w.connect(word->getOutputPin<32>("Q"), getOutputPin<32>("MEM_WORD_ADDR"));
        w.connect(w.invert("BACK_READY_NOW", A), getOutputPin("BACK_READY"));
    }
    w.finish();
}

} // namespace circuit::memory_detail
