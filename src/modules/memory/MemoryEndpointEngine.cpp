#include "MemoryEndpointEngine.hpp"
#include "components/selection/ProvenPartDelays.hpp"
#include "components/TimingProfile.hpp"
#include "modules/basic/DFlipFlopTiming.hpp"
#include "modules/memory/NativeMemoryDevice.hpp"
#include "DRAMSyncControl.hpp"
#include "NORSyncControl.hpp"
#include "modules/memory/MemoryTimingParts.hpp"
#include "modules/memory/SRAM.tpp"
#include <algorithm>

namespace circuit::memory {
Status validateRequest(const Request& r, size_t width, Technology technology, const EndpointConfig& c) {
    if (!known(r.op) || !known(r.address)) return Status::InvalidRequest;
    const auto op = number(r.op), address = number(r.address);
    const bool burst = op == unsigned(Operation::ReadBurst);
    // Bursts: the static and DRAM devices (a block of block_words words).
    if (burst && ((technology != Technology::SRAM && technology != Technology::ROM && technology != Technology::DRAM) ||
                  (technology == Technology::DRAM && c.block_words > c.words_per_row)))
        return Status::Unsupported;
    if (op > unsigned(Operation::ReadBurst)) return Status::Unsupported;
    if (op == unsigned(Operation::Write) || op == unsigned(Operation::Program))
        if (!known(r.mask)) return Status::InvalidRequest;
    if (address % (width / 8)) return Status::Alignment;
    if (address / (width / 8) >= c.depth_words) return Status::Range;
    if (burst) return Status::Ok;
    if (technology == Technology::ROM && op) return Status::ReadOnly;
    if (technology == Technology::NORFlash) {
        if (op == unsigned(Operation::Write)) return Status::Unsupported;
        if (op == unsigned(Operation::EraseBlock) && (address / (width / 8)) % c.block_words)
            return Status::Alignment;
    } else if (op > unsigned(Operation::Write)) return Status::Unsupported;
    return Status::Ok;
}
std::vector<std::string> nativeActivityInputs(Technology t) {
    switch (t) {
    case Technology::FF: return {"WORD_ADDR", "WE", "READ_EN"};
    case Technology::SRAM: return {"WORD_ADDR", "CE_N", "OE_N", "WE_N"};
    case Technology::ROM: return {"WORD_ADDR", "CE_N", "OE_N"};
    case Technology::DRAM: return {"ADDR", "CMD"};
    case Technology::NORFlash: return {"WORD_ADDR", "CMD"};
    }
    return {};
}
namespace {
// The word address register's input: the byte address shifted (wiring, so
// unknown bits pass).
Bits wordAddress(const Bits& address, size_t width) {
    const size_t shift = width == 32 ? 2 : width == 16 ? 1 : 0;
    Bits word(32, L);
    for (size_t i = 0; i + shift < 32; ++i) word[i] = address[i + shift];
    return word;
}
}
NativeSequence::NativeSequence(size_t w, Technology technology, EndpointConfig config)
    : result(w, L), width_(w), technology_(technology), config_(config),
      command_(technology == Technology::DRAM || technology == Technology::NORFlash),
      word_reg_(32, L), data_reg_(w, L), mask_reg_(w / 8, L),
      shown_{Bits(3, L), Bits(32, L), Bits(w, L), Bits(w / 8, L)}, needs_(technology == Technology::DRAM ? 4 : 2, false) {
    if (config_.clocked) outputs["CLK"] = {L};
    if (command_) {
        outputs["CLK"] = {L};
        commandPins([](const std::string& pin, size_t width) { return pin == "READ_DATA" ? Bits(width, Z) : Bits(width, L); });
    }
    else staticPins();
}
// The next word of the block after the current word address (wrapping).
Bits NativeSequence::nextWordAddress() const {
    const size_t words = config_.block_words ? config_.block_words : 1;
    Bits next = word_reg_;
    bool carry = true;
    for (size_t i = 0; i < size_t(std::countr_zero(words)); ++i) {
        const auto bit = word_reg_[i];
        next[i] = bit == X || bit == Z ? X : ((bit == H) != carry ? H : L);
        carry = carry && bit == H;
    }
    return next;
}
void NativeSequence::staticPins() {
    if (config_.clocked) {
        // StaticMemorySequencer's clocked form: CE_N and WE_N from START and
        // the request check, the fields straight from the request; a further
        // burst word is shown its address as it starts.
        const bool valid = shown_start_ && validateRequest(shown_, width_, technology_, config_) == Status::Ok;
        const bool write = valid && number(shown_.op) == unsigned(Operation::Write);
        const bool next_word = nextWord();
        outputs["CE_N"] = {valid || next_word ? L : H};
        outputs["WE_N"] = {write ? L : H};
        outputs["OE_N"] = {access_ && !writing_ ? L : H};
        outputs["WORD_ADDR"] = valid ? wordAddress(shown_.address, width_) : nextWordAddress();
        outputs["WRITE_DATA"] = shown_.data;
        outputs["WRITE_MASK"] = shown_.mask;
        return;
    }
    outputs["CE_N"] = {access_ ? L : H};
    outputs["WE_N"] = {access_ && writing_ ? L : H};
    outputs["OE_N"] = {access_ && !writing_ ? L : H};
    outputs["WORD_ADDR"] = word_reg_;
    outputs["WRITE_DATA"] = data_reg_;
    outputs["WRITE_MASK"] = mask_reg_;
}
void NativeSequence::step(size_t, bool clock_edge, bool reset, bool start, const Request& request,
                          const std::function<Bits(const std::string&, size_t)>& read, bool finish) {
    if (command_) stepCommand(clock_edge, reset, start, request, read, finish);
    else stepStatic(clock_edge, reset, start, request, read, finish);
}
// One clock edge of StaticMemorySequencer (MemorySequencers.cpp); every
// signal here is a state bit, register or gate of that circuit.
void NativeSequence::stepStatic(bool clock_edge, bool reset, bool start, const Request& request,
                                const std::function<Bits(const std::string&, size_t)>& read, bool finish) {
    if (reset) {
        access_ = writing_ = done = burst_ = last = false; wait_count_ = 0; beats_left_ = 0;
        word_reg_.assign(32, L); data_reg_.assign(width_, L); mask_reg_.assign(width_ / 8, L);
        result.assign(width_, L); status = Status::Ok;
    } else if (clock_edge) {
        const bool complete = staticComplete();
        const bool more = burst_ && beats_left_ > 0, next_word = complete && more;
        const auto checked = validateRequest(request, width_, technology_, config_);
        const bool accept = checked == Status::Ok;
        const bool valid_start = start && accept, invalid_start = start && !accept;
        // READY at the counted edge checks the access (a multicycle path).
        const bool ready = read("READY", 1) == Bits{H};
        if (complete) {
            result = !writing_ && ready ? read("READ_DATA", width_) : Bits(width_, L);
            status = ready ? Status::Ok : Status::Device;
        }
        if (invalid_start) { result.assign(width_, L); status = checked; }
        const bool next_access = valid_start || (access_ && !complete) || next_word;
        done = invalid_start || complete || (done && !finish);
        last = invalid_start || (complete && !more) || (last && !complete);
        wait_count_ = start || next_word ? config_.access_cycles - 1 : access_ && wait_count_ ? wait_count_ - 1 : 0;
        const bool burst_request = known(request.op) && number(request.op) == unsigned(Operation::ReadBurst);
        if (start) {
            word_reg_ = wordAddress(request.address, width_);
            data_reg_ = request.data; mask_reg_ = request.mask;
            writing_ = known(request.op) && number(request.op) == unsigned(Operation::Write);
            burst_ = burst_request;
            beats_left_ = burst_request && config_.block_words > 1 ? config_.block_words - 1 : 0;
        } else if (next_word) {
            word_reg_ = nextWordAddress();
            --beats_left_;
        }
        access_ = next_access;
    }
    available = !access_;
    staticPins();
}
std::optional<size_t> NativeSequence::shownStep(const std::function<Bits(const std::string&, size_t)>& read) const {
    const bool dram = technology_ == Technology::DRAM;
    const bool idle = read("BUSY", 1) == Bits{L};
    const bool cur_read = dram ? current_ == 2u || current_ == 6u : current_ == 1u;
    const bool fault = read("ERROR", 1) != Bits{L} || (cur_read && read("READ_VALID", 1) != Bits{H});
    if (!active_ || !idle || (waiting_ && fault)) return std::nullopt;
    for (size_t k = 0; k < needs_.size(); ++k) if (needs_[k]) return k;
    return std::nullopt;
}
unsigned NativeSequence::stepCode(size_t step) const {
    if (technology_ == Technology::DRAM) return step == 0 ? 4 : step == 1 ? 1 : step == 2 ? (is_write_ ? 3 : is_burst_ ? 6 : 2) : 5;
    return step == 0 ? 4 : (is_erase_ ? 3 : is_program_ ? 2 : 1);
}
void NativeSequence::commandPins(const std::function<Bits(const std::string&, size_t)>& read) {
    const bool dram = technology_ == Technology::DRAM;
    const auto shown = shownStep(read);
    presenting_ = shown.has_value();
    outputs["CMD"] = bits(3, shown ? stepCode(*shown) : 0);
    outputs["WRITE_DATA"] = data_reg_;
    outputs["WRITE_MASK"] = mask_reg_;
    if (!dram) { outputs["WORD_ADDR"] = word_reg_; return; }
    // The row while ACTIVATE is the selected step, the column otherwise.
    const size_t column_bits = std::countr_zero(config_.words_per_row);
    bool activate = false;
    for (size_t k = 0; k < needs_.size(); ++k) if (needs_[k]) { activate = k == 1; break; }
    Bits field(32, L);
    if (activate) for (size_t i = 0; i + column_bits < 32; ++i) field[i] = word_reg_[i + column_bits];
    else for (size_t i = 0; i < column_bits; ++i) field[i] = word_reg_[i];
    outputs["ADDR"] = field;
}
// One clock edge of CommandMemorySequencer (MemorySequencers.cpp).
void NativeSequence::stepCommand(bool clock_edge, bool reset, bool start, const Request& request,
                                 const std::function<Bits(const std::string&, size_t)>& read, bool finish) {
    const bool dram = technology_ == Technology::DRAM;
    if (reset) {
        active_ = waiting_ = done = last = false;
        is_write_ = is_program_ = is_erase_ = is_burst_ = false;
        std::fill(needs_.begin(), needs_.end(), false);
        current_ = 0; row_known_ = refreshing_ = refresh_pending_ = false; open_row_ = 0; refresh_counter_ = 0;
        word_reg_.assign(32, L); data_reg_.assign(width_, L); mask_reg_.assign(width_ / 8, L);
        result.assign(width_, L); status = Status::Ok;
    } else if (clock_edge) {
        const auto checked = validateRequest(request, width_, technology_, config_);
        const bool valid_start = start && checked == Status::Ok, invalid_start = start && checked != Status::Ok;
        const unsigned op = known(request.op) ? number(request.op) : 8;
        const bool op_write = op == unsigned(Operation::Write), op_program = op == unsigned(Operation::Program),
                   op_erase = op == unsigned(Operation::EraseBlock);
        const bool idle = read("BUSY", 1) == Bits{L};
        const bool step_done = waiting_ && idle;
        const bool cur_read = dram ? current_ == 2u || current_ == 6u : current_ == 1u;
        // A burst word before the last: the device still busy, the word shown.
        const bool word = dram && waiting_ && current_ == 6u && !idle && read("READ_VALID", 1) == Bits{H};
        const bool fault = read("ERROR", 1) != Bits{L} || (cur_read && read("READ_VALID", 1) != Bits{H});
        const bool fail = step_done && fault;
        const auto shown = shownStep(read);
        const bool present = shown.has_value();
        const bool ref_start = dram && refresh_pending_ && !active_ && !start;
        const size_t column_bits = std::countr_zero(config_.words_per_row);
        bool hit = false, row_open = false;
        if (dram) {
            row_open = read("ROW_OPEN", 1) == Bits{H};
            const auto live = wordAddress(request.address, width_);
            const uint32_t live_row = known(live) ? uint32_t(number(live) >> column_bits) : 0;
            hit = row_open && row_known_ && known(live) && open_row_ == live_row;
        }
        std::vector<bool> plan_request, plan_refresh;
        if (dram) {
            plan_request = {row_open && !hit, !hit, true, false};
            plan_refresh = {row_open, false, false, true};
        } else {
            plan_request = {op_program || op_erase, true};
            plan_refresh = {false, false};
        }
        bool any_pending = false;
        for (bool need : needs_) any_pending = any_pending || need;
        const bool finish_now = step_done && (fault || !any_pending);
        const bool request_ends = finish_now && !refreshing_;
        const bool plan = valid_start || ref_start;
        // Clocked updates, all from the values before the edge.
        const unsigned before = current_;
        if (present) current_ = stepCode(*shown);
        for (size_t k = 0; k < needs_.size(); ++k) {
            const bool held = needs_[k] && !(present && k == *shown) && !fail;
            needs_[k] = (valid_start && plan_request[k]) || (ref_start && plan_refresh[k]) || (held && !plan);
        }
        if ((step_done && cur_read && !fault) || word) result = read("READ_DATA", width_);
        else if ((step_done && cur_read) || request_ends || invalid_start) result.assign(width_, L);
        if (request_ends || invalid_start || word) status = invalid_start ? checked : step_done && fault ? Status::Device : Status::Ok;
        // An ACTIVATE that ended without fault records its row.
        if (dram && step_done && !fault && before == 1) {
            row_known_ = true;
            open_row_ = known(word_reg_) ? uint32_t(number(word_reg_) >> column_bits) : 0;
        }
        if (dram) {
            const size_t interval = config_.refresh_interval_cycles;
            const bool due = interval && refresh_counter_ == interval - 1;
            if (interval) refresh_counter_ = due ? 0 : refresh_counter_ + 1;
            refreshing_ = ref_start || (refreshing_ && !finish_now);
            refresh_pending_ = due || (refresh_pending_ && !ref_start);
        }
        done = request_ends || invalid_start || word || (done && !finish);
        last = request_ends || invalid_start ? true : word ? false : last;
        waiting_ = present || (waiting_ && !idle);
        active_ = plan || (active_ && !finish_now);
        if (start) {
            word_reg_ = wordAddress(request.address, width_);
            data_reg_ = request.data; mask_reg_ = request.mask;
            is_write_ = op_write; is_program_ = op_program; is_erase_ = op_erase;
            is_burst_ = op == unsigned(Operation::ReadBurst);
        }
    }
    available = !active_ && !(dram && refresh_pending_);
    commandPins(read);
}

DeviceModel::DeviceModel(size_t width, EndpointConfig c, LogicValue fill)
    : storage(c.depth_words, width, fill, c.record_history,
              {c.paged, c.max_resident_bytes, c.max_history_bytes}), width_(width), config_(c) {
    outputs["READ_DATA"] = Bits(width, Z);
    for (auto p : {"BUSY", "ERROR", "READ_VALID", "FAULT"}) outputs[p] = {L};
}
bool DeviceModel::edge(const Signals& s) {
    const auto clk = signal(s, "CLK")[0];
    const bool edge = previous_clock_ == L && clk == H; previous_clock_ = clk; return edge;
}
void DeviceModel::writeWord(size_t t, uint32_t word, const Bits& data, const Bits& mask, bool program, LogicValue enable) {
    auto old = storage.readLogic(word * (width_ / 8), width_ / 8);
    for (size_t byte = 0; byte < width_ / 8; ++byte) if (const auto on = both(enable, mask[byte]); on != L) {
        Bits lane(8);
        for (size_t bit = 0; bit < 8; ++bit) {
            const auto b = byte * 8 + bit;
            const auto d = stored(data[b]);
            lane[bit] = on != H ? (old[b] == L && d == L ? L : X)
                : !program ? d : old[b] == L || d == L ? L : old[b] == H && d == H ? H : X;
        }
        storage.writeLogic(t, word * (width_ / 8) + byte, lane, true);
    }
}
class FFDeviceModel final : public DeviceModel {
public:
    FFDeviceModel(size_t w, EndpointConfig c) : DeviceModel(w, c, X) {}
    void update(size_t t, const Signals& s) override {
        const bool rising = edge(s); const auto word = number(signal(s, "WORD_ADDR", 32));
        if (rising && signal(s, "WE") != Bits{L})
            writeWord(t, word, signal(s, "WRITE_DATA", width_), signal(s, "WRITE_MASK", width_ / 8), false, clean(signal(s, "WE")[0]));
        outputs["READ_DATA"] = high(s, "READ_EN") ? storage.readLogic(word * (width_ / 8), width_ / 8) : Bits(width_, L);
    }
};
// The longest delay an input takes to reach `output` in the table proven on
// a part's gate-level form, or `fallback` without a table.
size_t provenMaximum(const std::string& implementation, const ParameterMap& parameters, const std::string& output, size_t fallback) {
    const auto* table = provenPartDelays(implementation, parameters);
    if (!table) return fallback;
    size_t longest = 0;
    for (const auto& arc : table->arcs) if (arc.output == output) longest = std::max(longest, arc.delay.maximum);
    return longest ? longest : fallback;
}
// When an access by the fused endpoint completes, counted from the clock
// edge that changes the controller's outputs: the controller's own output
// delay plus the device's slowest input-to-READY delay, both from the tables
// proven on their gate-level forms.
size_t staticAccessTime(size_t width, Technology technology, const EndpointConfig& config);
// Launch from the controller's edge, the device's access to READY (READY
// follows the data), and a capture margin: the flip-flop setup and the
// result multiplexers (two gates).
size_t staticAccessCycles(size_t width, Technology technology, const EndpointConfig& config, size_t period) {
    if (!period) throw std::invalid_argument("staticAccessCycles needs a period");
    const size_t margin = circuit::dflipflop_timing::setup() + 2 * timing::leafDelay(timing::Leaf::And);
    const size_t needed = staticAccessTime(width, technology, config) + margin;
    return std::clamp<size_t>((needed + period - 1) / period, 1, 255);
}
size_t staticAccessTime(size_t width, Technology technology, const EndpointConfig& config) {
    const auto& memory = timing::activeTimingProfile().memory;
    const size_t gate = timing::leafDelay(timing::Leaf::And);
    const size_t flop = circuit::dflipflop_timing::clockToOutputMaximum();
    const auto device = nativeDeviceRequest(width, technology, config);
    const size_t rows = config.depth_words / config.words_per_row;
    const size_t device_fallback = technology == Technology::SRAM
        ? memoryTimerSettle(memory.sram_settle, 34 + width + width / 8, true) + memory.sram_precharge + memory.sramBitline(rows) + 2 * timing::leafDelay(timing::Leaf::MemoryPrimitive) + 14 * gate
        : memoryTimerSettle(memory.rom_settle, 33, false) + memory.romColumn(rows) + timing::leafDelay(timing::Leaf::MemoryPrimitive) + 14 * gate;
    // A clocked macro registers the request at the edge itself: its access
    // is the later of the proven launches from CLK to READY and to the data
    // (the static analysis bounds the data path more loosely than READY).
    if (config.clocked) {
        const size_t fallback = flop + circuit::sram::clockedSettle(rows) + std::max(memory.sram_precharge, memory.sramBitline(rows)) + memory.sramBitline(rows)
            + 2 * timing::leafDelay(timing::Leaf::MemoryPrimitive) + 14 * gate;
        const auto* table = provenPartDelays(device.contract_id + ".behavioral", device.parameters);
        if (!table) return fallback;
        const auto ready = table->launches.find("READY"), data = table->launches.find("READ_DATA");
        if (ready == table->launches.end()) return fallback;
        return std::max(ready->second.maximum, data == table->launches.end() ? size_t{0} : data->second.maximum);
    }
    const size_t controller = provenMaximum(memoryContractId(technology, "controller", width) + ".behavioral",
                                            endpointParameters(config), "MEM_CE_N", flop + gate);
    return controller + provenMaximum(device.contract_id + ".behavioral", device.parameters, "READY", device_fallback);
}

// SRAM as the fused endpoint sees it: an access restarts on any change of
// the controller's address, data, mask, CE_N or WE_N; READY and the read word
// appear one access time later; a write lands then.
class SRAMDeviceModel final : public DeviceModel {
    Bits address_, data_, mask_;
    LogicValue ce_ = X, we_ = X, oe_ = X, clock_ = X;
    bool seen_ = false;
    size_t access_time_;
    bool selected() const { return known(address_) && number(address_) < config_.depth_words; }
    Bits output() const {
        if (ce_ != L || we_ != H || oe_ != L) return Bits(width_, Z);
        return selected() ? storage.readLogic(number(address_) * (width_ / 8), width_ / 8) : Bits(width_, X);
    }
public:
    SRAMDeviceModel(size_t w, EndpointConfig c) : DeviceModel(w, c, X), access_time_(staticAccessTime(w, Technology::SRAM, c)) {
        outputs["READY"] = {L};
    }
    // A clocked macro: an edge with CE_N low registers the request; READY and
    // the word one access time after the edge, held until the next access.
    void updateClocked(size_t t, const Signals& s) {
        const auto clock = signal(s, "CLK")[0], ce = signal(s, "CE_N")[0], oe = signal(s, "OE_N")[0];
        const bool edge = clock_ == L && clock == H;
        clock_ = clock;
        if (edge && ce != H) {
            address_ = signal(s, "WORD_ADDR", 32); data_ = signal(s, "WRITE_DATA", width_); mask_ = signal(s, "WRITE_MASK", width_ / 8);
            ce_ = ce; we_ = signal(s, "WE_N")[0]; oe_ = oe; seen_ = true;
            outputs["READY"] = {L}; outputs["READ_DATA"] = Bits(width_, Z);
            due_ = after(t, access_time_);
            return;
        }
        if (oe != oe_) { oe_ = oe; if (!due_ && seen_) outputs["READ_DATA"] = output(); }
        if (!due_ || t < due_) return;
        due_ = 0;
        if (ce_ == L && we_ == L && selected()) writeWord(t, number(address_), data_, mask_);
        outputs["READY"] = {ce_ == L && binary(we_) ? H : X};
        outputs["READ_DATA"] = output();
    }
    void update(size_t t, const Signals& s) override {
        if (config_.clocked) { updateClocked(t, s); return; }
        const auto address = signal(s, "WORD_ADDR", 32), data = signal(s, "WRITE_DATA", width_), mask = signal(s, "WRITE_MASK", width_ / 8);
        const auto ce = signal(s, "CE_N")[0], we = signal(s, "WE_N")[0], oe = signal(s, "OE_N")[0];
        const bool writing = we == L || we_ == L;
        if (!seen_ || address != address_ || ce != ce_ || we != we_ || (writing && (data != data_ || mask != mask_))) {
            seen_ = true; address_ = address; data_ = data; mask_ = mask; ce_ = ce; we_ = we; oe_ = oe;
            outputs["READY"] = {L}; outputs["READ_DATA"] = Bits(width_, Z);
            due_ = after(t, access_time_);
            return;
        }
        data_ = data; mask_ = mask;
        if (oe != oe_) { oe_ = oe; if (!due_) outputs["READ_DATA"] = output(); }
        if (!due_ || t < due_) return;
        due_ = 0;
        if (ce_ == L && we_ == L && selected()) writeWord(t, number(address_), data_, mask_);
        outputs["READY"] = {ce_ == H ? L : ce_ == L && binary(we_) ? H : X};
        outputs["READ_DATA"] = output();
    }
};
// ROM as the fused endpoint sees it: READY one access time after the last
// change of address or CE_N; the read word follows OE_N.
class ROMDeviceModel final : public DeviceModel {
    Bits address_;
    LogicValue ce_ = X, oe_ = X;
    bool seen_ = false;
    size_t access_time_;
    Bits output() const {
        if (ce_ == H || oe_ == H) return Bits(width_, Z);
        return ce_ == L && oe_ == L && known(address_) && number(address_) < config_.depth_words
            ? storage.readLogic(number(address_) * (width_ / 8), width_ / 8) : Bits(width_, X);
    }
public:
    ROMDeviceModel(size_t w, EndpointConfig c) : DeviceModel(w, c, L), access_time_(staticAccessTime(w, Technology::ROM, c)) {
        std::vector<uint8_t> image;
        for (size_t i = 0; i < c.image_hex.size(); i += 2) image.push_back(std::stoul(c.image_hex.substr(i, 2), nullptr, 16));
        storage.loadBytes(0, image);
        outputs["READY"] = {L};
    }
    void update(size_t t, const Signals& s) override {
        const auto address = signal(s, "WORD_ADDR", 32);
        const auto ce = signal(s, "CE_N")[0], oe = signal(s, "OE_N")[0];
        if (!seen_ || address != address_ || ce != ce_) {
            seen_ = true; address_ = address; ce_ = ce; oe_ = oe;
            outputs["READY"] = {L};
            due_ = after(t, access_time_);
        } else if (oe != oe_) oe_ = oe;
        outputs["READ_DATA"] = output();
        if (!due_ || t < due_) return;
        due_ = 0;
        outputs["READY"] = {ce_ == H ? L : ce_ == L ? H : X};
    }
};
// DRAM and NOR flash as the fused endpoint sees them: the gate-level command
// control cycle for cycle (DRAMSyncControl, NORSyncControl), clocked by the
// controller's MEM_CLK, with plain storage.
class DRAMDeviceModel final : public DeviceModel {
    memory_detail::DRAMSyncControl control_;
    Bits buffer_;
    std::map<size_t, size_t> closed_;
    bool wl_ = false, sense_ = false, write_ = false;
public:
    DRAMDeviceModel(size_t w, EndpointConfig c)
        : DeviceModel(w, c, X), control_(w, c.depth_words / c.words_per_row, c.words_per_row, c.block_words), buffer_(w * c.words_per_row, X) {
        outputs["ROW_OPEN"] = {L};
    }
    void update(size_t t, const Signals& s) override {
        if (edge(s)) control_.edge(t, signal(s, "CMD", 3), signal(s, "ADDR", 32), signal(s, "WRITE_DATA", width_), signal(s, "WRITE_MASK", width_ / 8));
        const bool wl = control_.wordline(t), sense = control_.sensing(t), write = control_.writing(t), precharge = control_.precharging(t);
        const auto start = uint32_t(control_.row * config_.words_per_row * (width_ / 8));
        if (!wl_ && wl) {
            auto it = closed_.find(control_.row); size_t closed = it == closed_.end() ? 0 : it->second;
            // Storage keeps the logical row while it is open (DRAMBehavioral).
            buffer_ = t - closed >= config_.retention_ticks ? Bits(buffer_.size(), X) : storage.readLogic(start, buffer_.size() / 8);
        }
        if (!sense_ && sense) storage.writeLogic(t, start, buffer_);
        if (!write_ && write) {
            for (size_t b = 0; b < width_; ++b) if (control_.mask[b / 8] == H) buffer_[control_.column * width_ + b] = stored(control_.data[b]);
            writeWord(t, control_.row * config_.words_per_row + control_.column, control_.data, control_.mask);
        }
        if (wl_ && !wl) closed_[control_.row] = t;
        if (precharge) std::fill(buffer_.begin(), buffer_.end(), X);
        wl_ = wl; sense_ = sense; write_ = write;
        outputs["BUSY"] = {control_.busyShown() ? H : L};
        outputs["ERROR"] = {control_.error ? H : L};
        outputs["READ_VALID"] = {control_.validShown() ? H : L};
        outputs["ROW_OPEN"] = {control_.openShown() ? H : L};
        outputs["READ_DATA"] = control_.validShown()
            ? Bits(buffer_.begin() + control_.column * width_, buffer_.begin() + (control_.column + 1) * width_) : Bits(width_, Z);
        due_ = control_.nextStep(t);
    }
};
class NORFlashDeviceModel final : public DeviceModel {
    memory_detail::NORSyncControl control_;
    bool program_ = false, erase_ = false;
public:
    NORFlashDeviceModel(size_t w, EndpointConfig c)
        : DeviceModel(w, c, H), control_(w, c.depth_words, c.depth_words / c.words_per_row, c.program_ticks, c.erase_ticks) {
        outputs["WRITE_ENABLED"] = {L};
    }
    void update(size_t t, const Signals& s) override {
        if (edge(s)) control_.edge(t, signal(s, "CMD", 3), signal(s, "WORD_ADDR", 32), signal(s, "WRITE_DATA", width_), signal(s, "WRITE_MASK", width_ / 8));
        const bool program = control_.programPulse(t), erase = control_.erasePulse(t);
        if (program && !program_) writeWord(t, control_.address, control_.data, control_.mask, true);
        if (erase && !erase_) {
            const auto first = control_.address / config_.block_words * config_.block_words;
            for (size_t word = first; word < first + config_.block_words; ++word)
                storage.writeLogic(t, word * (width_ / 8), Bits(width_, H), true);
        }
        program_ = program; erase_ = erase;
        outputs["BUSY"] = {control_.busyShown() ? H : L};
        outputs["ERROR"] = {control_.error ? H : L};
        outputs["READ_VALID"] = {control_.valid ? H : L};
        outputs["WRITE_ENABLED"] = {control_.wel ? H : L};
        outputs["READ_DATA"] = control_.valid ? storage.readLogic(control_.address * (width_ / 8), width_ / 8) : Bits(width_, Z);
        due_ = control_.nextStep(t);
    }
};
std::unique_ptr<DeviceModel> makeDeviceModel(size_t w, Technology t, EndpointConfig c) {
    switch (t) {
    case Technology::FF: return std::make_unique<FFDeviceModel>(w, c);
    case Technology::SRAM: return std::make_unique<SRAMDeviceModel>(w, c);
    case Technology::DRAM: return std::make_unique<DRAMDeviceModel>(w, c);
    case Technology::ROM: return std::make_unique<ROMDeviceModel>(w, c);
    case Technology::NORFlash: return std::make_unique<NORFlashDeviceModel>(w, c);
    default: throw std::invalid_argument("A container is an endpoint, not a physical technology model");
    }
}
}
