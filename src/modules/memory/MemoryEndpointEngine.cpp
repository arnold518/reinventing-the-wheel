#include "MemoryEndpointEngine.hpp"
#include "components/selection/ProvenPartDelays.hpp"
#include "components/TimingProfile.hpp"
#include "modules/basic/DFlipFlopTiming.hpp"
#include "modules/memory/NativeMemoryDevice.hpp"
#include "DRAMSyncControl.hpp"
#include "NORSyncControl.hpp"
#include <algorithm>

namespace circuit::memory {
Status validateRequest(const Request& r, size_t width, Technology technology, const EndpointConfig& c) {
    if (!known(r.op) || !known(r.address)) return Status::InvalidRequest;
    const auto op = number(r.op), address = number(r.address);
    if (op > unsigned(Operation::EraseBlock)) return Status::Unsupported;
    if (op == unsigned(Operation::Write) || op == unsigned(Operation::Program))
        if (!known(r.mask)) return Status::InvalidRequest;
    if (address % (width / 8)) return Status::Alignment;
    if (address / (width / 8) >= c.depth_words) return Status::Range;
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
NativeSequence::NativeSequence(size_t w, Technology technology, EndpointConfig config)
    : result(w, L), width_(w), technology_(technology), config_(config),
      command_(technology == Technology::DRAM || technology == Technology::NORFlash),
      word_reg_(32, L), data_reg_(w, L), mask_reg_(w / 8, L), needs_(technology == Technology::DRAM ? 4 : 2, false) {
    if (command_) { outputs["CLK"] = {L}; commandPins(); }
    else staticPins();
}
void NativeSequence::staticPins() {
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
// One clock edge of StaticMemorySequencer (MemorySequencers.cpp); every
// signal here is a state bit, register or gate of that circuit.
void NativeSequence::stepStatic(bool clock_edge, bool reset, bool start, const Request& request,
                                const std::function<Bits(const std::string&, size_t)>& read, bool finish) {
    if (reset) {
        access_ = writing_ = done = false; sync1_ = sync2_ = L;
        word_reg_.assign(32, L); data_reg_.assign(width_, L); mask_reg_.assign(width_ / 8, L);
        result.assign(width_, L); status = Status::Ok;
    } else if (clock_edge) {
        const bool complete = access_ && sync2_ == H;
        const auto checked = validateRequest(request, width_, technology_, config_);
        const bool accept = checked == Status::Ok;
        const bool valid_start = start && accept, invalid_start = start && !accept;
        if (complete) { result = writing_ ? Bits(width_, L) : read("READ_DATA", width_); status = Status::Ok; }
        if (invalid_start) { result.assign(width_, L); status = checked; }
        const bool next_access = valid_start || (access_ && !complete);
        done = invalid_start || complete || (done && !finish);
        if (start) {
            word_reg_ = wordAddress(request.address, width_);
            data_reg_ = request.data; mask_reg_ = request.mask;
            writing_ = known(request.op) && number(request.op) == unsigned(Operation::Write);
        }
        sync2_ = sync1_;
        const auto ready = read("READY", 1)[0];
        if (ready == L || ready == H) sync1_ = ready;
        access_ = next_access;
    }
    available = !access_ && sync2_ == L;
    staticPins();
}
void NativeSequence::commandPins() {
    const bool dram = technology_ == Technology::DRAM;
    outputs["CMD"] = bits(3, issuing_ ? current_ : 0);
    outputs["WRITE_DATA"] = data_reg_;
    outputs["WRITE_MASK"] = mask_reg_;
    if (!dram) { outputs["WORD_ADDR"] = word_reg_; return; }
    // The row for ACTIVATE, the column otherwise.
    const size_t column_bits = std::countr_zero(config_.words_per_row);
    Bits field(32, L);
    if (current_ == 1) for (size_t i = 0; i + column_bits < 32; ++i) field[i] = word_reg_[i + column_bits];
    else for (size_t i = 0; i < column_bits; ++i) field[i] = word_reg_[i];
    outputs["ADDR"] = field;
}
// One clock edge of CommandMemorySequencer (MemorySequencers.cpp).
void NativeSequence::stepCommand(bool clock_edge, bool reset, bool start, const Request& request,
                                 const std::function<Bits(const std::string&, size_t)>& read, bool finish) {
    const bool dram = technology_ == Technology::DRAM;
    if (reset) {
        active_ = issuing_ = waiting_ = done = false;
        is_write_ = is_program_ = is_erase_ = false;
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
        const bool busy = read("BUSY", 1) != Bits{L};
        const bool step_done = waiting_ && !busy;
        const bool cur_read = current_ == (dram ? 2u : 1u);
        const bool fail = step_done && (read("ERROR", 1) != Bits{L} || (cur_read && read("READ_VALID", 1) != Bits{H}));
        const bool ref_start = dram && refresh_pending_ && !active_ && !start;
        const size_t column_bits = std::countr_zero(config_.words_per_row);
        bool hit = false, row_open = false;
        uint32_t live_row = 0;
        if (dram) {
            row_open = read("ROW_OPEN", 1) == Bits{H};
            const auto live = wordAddress(request.address, width_);
            live_row = known(live) ? uint32_t(number(live) >> column_bits) : 0;
            hit = row_open && row_known_ && known(live) && open_row_ == live_row;
        }
        // The planned steps of a new request and of a refresh.
        std::vector<bool> plan_request, plan_refresh;
        if (dram) {
            plan_request = {row_open && !hit, !hit, true, false};
            plan_refresh = {row_open, false, false, true};
        } else {
            plan_request = {op_program || op_erase, true};
            plan_refresh = {false, false};
        }
        const bool fresh = valid_start || ref_start;
        std::vector<bool> list(needs_.size());
        bool any_pending = false;
        for (size_t k = 0; k < list.size(); ++k) {
            list[k] = (valid_start && plan_request[k]) || (ref_start && plan_refresh[k]) || (needs_[k] && !fresh);
            any_pending = any_pending || list[k];
        }
        const bool proceed = step_done && !fail;
        const bool issue_now = any_pending && (valid_start || ref_start || proceed);
        size_t selected = list.size();
        for (size_t k = 0; k < list.size(); ++k) if (list[k]) { selected = k; break; }
        // On the starting edge the kind comes from the new request.
        const bool write_kind = valid_start ? op_write : is_write_;
        const bool program_kind = valid_start ? op_program : is_program_;
        const bool erase_kind = valid_start ? op_erase : is_erase_;
        unsigned next_code = 0;
        if (selected < list.size()) {
            if (dram) next_code = selected == 0 ? 4 : selected == 1 ? 1 : selected == 2 ? (write_kind ? 3 : 2) : 5;
            else next_code = selected == 0 ? 4 : (erase_kind ? 3 : program_kind ? 2 : 1);
        }
        const bool finish_now = step_done && (fail || !any_pending);
        const bool request_ends = finish_now && !refreshing_;
        // Clocked updates, all from the values before the edge.
        for (size_t k = 0; k < list.size(); ++k)
            needs_[k] = list[k] && !(issue_now && k == selected) && !fail;
        const bool opened = step_done && current_ == 1 && !fail && dram;
        if (step_done && cur_read && !fail) result = read("READ_DATA", width_);
        else if ((step_done && cur_read) || request_ends || invalid_start) result.assign(width_, L);
        if (request_ends || invalid_start) status = invalid_start ? checked : fail ? Status::Device : Status::Ok;
        if (opened) {
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
        if (issue_now) current_ = next_code;
        done = request_ends || invalid_start || (done && !finish);
        waiting_ = issuing_ || (waiting_ && !step_done);
        issuing_ = issue_now;
        active_ = fresh || (active_ && !finish_now);
        if (start) {
            word_reg_ = wordAddress(request.address, width_);
            data_reg_ = request.data; mask_reg_ = request.mask;
            is_write_ = op_write; is_program_ = op_program; is_erase_ = op_erase;
        }
    }
    available = !active_ && !(dram && refresh_pending_);
    commandPins();
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
size_t staticAccessTime(size_t width, Technology technology, const EndpointConfig& config) {
    const auto& memory = timing::activeTimingProfile().memory;
    const size_t gate = timing::leafDelay(timing::Leaf::And);
    const size_t flop = circuit::dflipflop_timing::clockToOutputMaximum();
    const auto device = nativeDeviceRequest(width, technology, config);
    const size_t rows = config.depth_words / config.words_per_row;
    const size_t device_fallback = technology == Technology::SRAM
        ? memory.sram_settle + memory.sram_precharge + memory.sramBitline(rows) + 12 * gate
        : memory.rom_settle + memory.romColumn(rows) + 8 * gate;
    const size_t controller = provenMaximum(memoryContractId(technology, "controller", width) + ".behavioral",
                                            endpointParameters(config), "MEM_CE_N", flop + gate);
    return controller + provenMaximum(device.contract_id + ".behavioral", device.parameters, "READY", device_fallback);
}

// SRAM as the fused endpoint sees it: an access restarts on any change of
// the controller's address, data, mask, CE_N or WE_N; READY and the read word
// appear one access time later; a write lands then.
class SRAMDeviceModel final : public DeviceModel {
    Bits address_, data_, mask_;
    LogicValue ce_ = X, we_ = X, oe_ = X;
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
    void update(size_t t, const Signals& s) override {
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
        : DeviceModel(w, c, X), control_(w, c.depth_words / c.words_per_row, c.words_per_row), buffer_(w * c.words_per_row, X) {
        outputs["ROW_OPEN"] = {L};
    }
    void update(size_t t, const Signals& s) override {
        if (edge(s)) control_.edge(t, signal(s, "CMD", 3), signal(s, "ADDR", 32), signal(s, "WRITE_DATA", width_), signal(s, "WRITE_MASK", width_ / 8));
        const bool wl = control_.wordline(t), sense = control_.sensing(t), write = control_.writing(t), precharge = control_.precharging(t);
        const auto start = uint32_t(control_.row * config_.words_per_row * (width_ / 8));
        if (!wl_ && wl) {
            auto it = closed_.find(control_.row); size_t closed = it == closed_.end() ? 0 : it->second;
            buffer_ = t - closed >= config_.retention_ticks ? Bits(buffer_.size(), X) : storage.readLogic(start, buffer_.size() / 8);
            storage.writeLogic(t, start, Bits(buffer_.size(), X));
        }
        if (!sense_ && sense) storage.writeLogic(t, start, buffer_);
        if (!write_ && write) {
            for (size_t b = 0; b < width_; ++b) if (control_.mask[b / 8] == H) buffer_[control_.column * width_ + b] = stored(control_.data[b]);
            writeWord(t, control_.row * config_.words_per_row + control_.column, control_.data, control_.mask);
        }
        if (wl_ && !wl) closed_[control_.row] = t;
        if (precharge) std::fill(buffer_.begin(), buffer_.end(), X);
        wl_ = wl; sense_ = sense; write_ = write;
        outputs["BUSY"] = {control_.busy ? H : L};
        outputs["ERROR"] = {control_.error ? H : L};
        outputs["READ_VALID"] = {control_.valid ? H : L};
        outputs["ROW_OPEN"] = {control_.open ? H : L};
        outputs["READ_DATA"] = control_.valid
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
        outputs["BUSY"] = {control_.busy ? H : L};
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
