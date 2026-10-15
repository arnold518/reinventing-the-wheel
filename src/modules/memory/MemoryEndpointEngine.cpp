#include "MemoryEndpointEngine.hpp"
#include "components/selection/ProvenPartDelays.hpp"
#include "components/TimingProfile.hpp"
#include "modules/basic/DFlipFlopTiming.hpp"
#include "modules/memory/NativeMemoryDevice.hpp"
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
    : result(w, L), width_(w), technology_(technology), config_(config), work_result_(w, L),
      word_reg_(32, L), data_reg_(w, L), mask_reg_(w / 8, L) {
    idlePins();
    if (technology_ == Technology::SRAM || technology_ == Technology::ROM) staticPins();
}
void NativeSequence::staticPins() {
    outputs["CE_N"] = {access_ ? L : H};
    outputs["WE_N"] = {access_ && writing_ ? L : H};
    outputs["OE_N"] = {access_ && !writing_ ? L : H};
    outputs["WORD_ADDR"] = word_reg_;
    outputs["WRITE_DATA"] = data_reg_;
    outputs["WRITE_MASK"] = mask_reg_;
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
            // The word address is the byte address shifted: wiring, unknown bits pass.
            const size_t shift = width_ == 32 ? 2 : width_ == 16 ? 1 : 0;
            word_reg_.assign(32, L);
            for (size_t i = 0; i + shift < 32; ++i) word_reg_[i] = request.address[i + shift];
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
void NativeSequence::idlePins() {
    outputs["CLK"] = {L}; outputs["RST"] = {L}; outputs["CMD"] = bits(3, 0);
    outputs["CE_N"] = outputs["WE_N"] = outputs["OE_N"] = {H};
    outputs["WE"] = outputs["READ_EN"] = {L};
    outputs["REQ_VALID"] = outputs["RSP_READY"] = {L};
    outputs["ADDR"] = outputs["WORD_ADDR"] = outputs["REQ_ADDR"] = bits(32, 0);
    outputs["WRITE_DATA"] = outputs["REQ_WRITE_DATA"] = Bits(width_, L);
    outputs["WRITE_MASK"] = outputs["REQ_WRITE_MASK"] = Bits(width_ / 8, L);
    outputs["REQ_OP"] = bits(3, 0);
}
void NativeSequence::beginCommand(size_t t) {
    if (commands_.empty()) { finish(t); return; }
    const auto command = commands_.front();
    outputs["CLK"] = {L}; outputs["CMD"] = bits(3, command.op);
    outputs["ADDR"] = outputs["WORD_ADDR"] = bits(32, command.address);
    phase_ = Phase::CommandRaise; due_ = after(t, config_.guard_ticks);
}
void NativeSequence::finish(size_t t) {
    idlePins(); phase_ = Phase::Release; due_ = after(t, config_.guard_ticks);
}
void NativeSequence::step(size_t t, bool clock_edge, bool reset, bool start, const Request& request,
                         const std::function<Bits(const std::string&, size_t)>& read, bool finish_response) {
    if (technology_ == Technology::SRAM || technology_ == Technology::ROM) {
        stepStatic(clock_edge, reset, start, request, read, finish_response);
        return;
    }
    if (reset) aborted_ = true;
    if (clock_edge && finish_response) done = false;
    if (phase_ == Phase::Idle) {
        if (reset) { done = false; finished_ = settling_ = false; available = false; return; }
        // The device is timed in ticks, but the sequencer speaks to the
        // transaction port only at a clock edge: a finished operation shows
        // DONE, its result and BACK_READY at the next edge, like any other
        // clocked state. Shown the moment the device finishes, they would
        // reach the requester at any point of its cycle.
        const bool was_available = !settling_;
        if (clock_edge) {
            if (finished_) { done = true; result = work_result_; status = work_status_; finished_ = false; }
            settling_ = false;
        }
        available = !settling_;
        if (done || finished_) return;
        const bool refresh_due = technology_ == Technology::DRAM &&
                             config_.refresh_interval_ticks && t >= next_refresh_;
        // A refresh starts only once the backend has been seen ready for an
        // edge, so a waiting request is never overtaken by one refresh after
        // another.
        const bool refresh = refresh_due && clock_edge && !start && was_available;
        if (!(start && clock_edge) && !refresh) { aborted_ = false; return; }
        aborted_ = false; maintenance_ = refresh; available = false; settling_ = true;
        status = work_status_ = Status::Ok; work_result_.assign(width_, L);
        result.assign(width_, L); commands_.clear();
        if (refresh) {
            if (row_open_) commands_.push_back({4, 0});
            commands_.push_back({5, 0});
            next_refresh_ = after(t, config_.refresh_interval_ticks);
            beginCommand(t); return;
        }
        request_ = request;
        work_status_ = validateRequest(request, width_, technology_, config_);
        if (work_status_ != Status::Ok) { finish(t); return; }
        const auto op = number(request.op);
        const auto word = number(request.address) / (width_ / 8);
        outputs["WORD_ADDR"] = bits(32, word);
        outputs["WRITE_DATA"] = request.data; outputs["WRITE_MASK"] = request.mask;
        if (technology_ == Technology::FF) {
            phase_ = Phase::ArraySetup; due_ = after(t, config_.guard_ticks); return;
        }
        if (technology_ == Technology::SRAM || technology_ == Technology::ROM) {
            outputs["CE_N"] = {L}; outputs["WE_N"] = {op == 1 ? L : H};
            outputs["OE_N"] = {op == 0 ? L : H};
            phase_ = Phase::StaticAccess;
            due_ = after(t, 4 * config_.phase_ticks + 2 * config_.guard_ticks); return;
        }
        if (technology_ == Technology::DRAM) {
            const auto row = word / config_.words_per_row;
            // An accepted request cannot suppress refresh indefinitely. Perform
            // due maintenance before its access, using the already latched
            // request; no second queue or discarded START pulse is needed.
            if (refresh_due) {
                if (row_open_) commands_.push_back({4, 0});
                commands_.push_back({5, 0});
                next_refresh_ = after(t, config_.refresh_interval_ticks);
            } else if (row_open_ && row_ != row) commands_.push_back({4, 0});
            if (refresh_due || !row_open_ || row_ != row) commands_.push_back({1, uint32_t(row)});
            commands_.push_back({op == 0 ? 2u : 3u, uint32_t(word % config_.words_per_row)});
        } else {
            if (op) commands_.push_back({4, 0});
            commands_.push_back({op == 0 ? 1u : op == 2 ? 2u : 3u, uint32_t(word)});
        }
        beginCommand(t); return;
    }
    available = false;
    if (!due_ || t < due_) return;
    due_ = 0;
    switch (phase_) {
    case Phase::ArraySetup:
        outputs["WE"] = {number(request_.op) == 1 ? H : L};
        outputs["READ_EN"] = {number(request_.op) == 0 ? H : L};
        phase_ = Phase::ArrayWrite; due_ = after(t, config_.guard_ticks); break;
    case Phase::ArrayWrite:
        outputs["CLK"] = {H}; phase_ = Phase::ArrayRead;
        due_ = after(t, config_.guard_ticks); break;
    case Phase::ArrayRead:
        if (read("FAULT", 1) != Bits{L}) work_status_ = Status::Device;
        if (number(request_.op) == 0) work_result_ = read("READ_DATA", width_);
        finish(t); break;
    case Phase::StaticAccess:
        if (number(request_.op) == 0) work_result_ = read("READ_DATA", width_);
        finish(t); break;
    case Phase::CommandRaise:
        outputs["CLK"] = {H}; phase_ = Phase::CommandLower;
        due_ = after(t, config_.guard_ticks); break;
    case Phase::CommandLower: {
        outputs["CLK"] = {L}; outputs["CMD"] = bits(3, 0);
        const auto op = commands_.front().op;
        size_t wait = config_.phase_ticks;
        if (technology_ == Technology::DRAM) wait *= op == 5 ? 5 : op == 2 ? 1 : 3;
        else if (op == 2 || op == 3) wait = (op == 2 ? config_.program_ticks : config_.erase_ticks) + 2 * config_.phase_ticks;
        phase_ = Phase::CommandWait; due_ = after(t, wait + config_.guard_ticks); break;
    }
    case Phase::CommandWait: {
        if (read("BUSY", 1) != Bits{L}) { due_ = after(t, config_.guard_ticks); break; }
        if (read("ERROR", 1) != Bits{L}) { work_status_ = Status::Device; commands_.clear(); finish(t); break; }
        const auto command = commands_.front(); commands_.pop_front();
        if (technology_ == Technology::DRAM) {
            if (command.op == 1) { row_open_ = true; row_ = command.address; }
            if (command.op == 4) row_open_ = false;
        }
        if ((technology_ == Technology::DRAM && command.op == 2) ||
            (technology_ == Technology::NORFlash && command.op == 1)) {
            if (read("READ_VALID", 1) != Bits{H}) work_status_ = Status::Device;
            else work_result_ = read("READ_DATA", width_);
        }
        beginCommand(t); break;
    }
    case Phase::Release:
        phase_ = Phase::Idle; finished_ = !maintenance_ && !aborted_; break;
    default: break;
    }
    if (work_status_ != Status::Ok) work_result_.assign(width_, L);
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
class DRAMDeviceModel final : public DeviceModel {
    size_t row_ = 0, col_ = 0, refresh_row_ = 0, op_ = 0, stage_ = 0;
    bool open_ = false;
    std::map<size_t, size_t> closed_;
    Bits buffer_, data_, mask_;
public:
    DRAMDeviceModel(size_t w, EndpointConfig c) : DeviceModel(w, c, X), buffer_(w * c.words_per_row, X) {
        outputs["ROW_OPEN"] = {L};
    }
    void update(size_t t, const Signals& s) override {
        if (due_ && t >= due_) {
            due_ = 0; ++stage_;
            const auto start = uint32_t(row_ * config_.words_per_row * (width_ / 8));
            if (op_ == 1 || op_ == 5) {
                if (stage_ == 1) {
                    auto it = closed_.find(row_); size_t closed = it == closed_.end() ? 0 : it->second;
                    buffer_ = t - closed >= config_.retention_ticks ? Bits(buffer_.size(), X) : storage.readLogic(start, buffer_.size() / 8);
                    storage.writeLogic(t, start, Bits(buffer_.size(), X));
                } else if (stage_ == 2) storage.writeLogic(t, start, buffer_);
                else if (stage_ == 3) {
                    if (op_ == 1) { open_ = true; outputs["ROW_OPEN"] = {H}; outputs["BUSY"] = {L}; }
                    else closed_[row_] = t;
                } else if (stage_ == 5) { refresh_row_ = (refresh_row_ + 1) % (config_.depth_words / config_.words_per_row); outputs["BUSY"] = {L}; }
            } else if (op_ == 2) {
                outputs["READ_DATA"] = Bits(buffer_.begin() + col_ * width_, buffer_.begin() + (col_ + 1) * width_);
                outputs["READ_VALID"] = {H}; outputs["BUSY"] = {L};
            } else if (op_ == 3) {
                if (stage_ == 1) {
                    for (size_t b = 0; b < width_; ++b) if (mask_[b / 8] == H) buffer_[col_ * width_ + b] = stored(data_[b]);
                    // Report the addressed lanes, not the entire open row:
                    // neighboring bytes may still be unknown or unmodified.
                    writeWord(t, row_ * config_.words_per_row + col_, data_, mask_);
                } else if (stage_ == 3) outputs["BUSY"] = {L};
            } else if (op_ == 4) {
                if (stage_ == 1) closed_[row_] = t;
                if (stage_ == 3) { open_ = false; outputs["ROW_OPEN"] = {L}; outputs["BUSY"] = {L}; }
            }
            if (high(outputs, "BUSY")) due_ = after(t, config_.phase_ticks);
        }
        if (!edge(s)) return;
        const auto op = number(signal(s, "CMD", 3)), address = number(signal(s, "ADDR", 32));
        if (!op) return;
        if (high(outputs, "BUSY")) { outputs["ERROR"] = {H}; return; }
        outputs["READ_VALID"] = {L}; outputs["READ_DATA"] = Bits(width_, Z);
        if ((op == 1 && open_) || ((op == 2 || op == 3 || op == 4) && !open_) || (op == 5 && open_)) {
            outputs["ERROR"] = {H}; return;
        }
        outputs["ERROR"] = {L}; outputs["BUSY"] = {H}; op_ = op; stage_ = 0;
        if (op == 1 || op == 5) row_ = op == 1 ? address : refresh_row_;
        if (op == 2 || op == 3) col_ = address;
        if (op == 3) { data_ = signal(s, "WRITE_DATA", width_); mask_ = signal(s, "WRITE_MASK", width_ / 8); }
        due_ = after(t, config_.phase_ticks);
        if (closed_.size() > (config_.max_resident_bytes - std::min(config_.max_resident_bytes, storage.allocatedDataBytes())) / 128)
            throw std::length_error("DRAM row metadata budget exceeded");
    }
};
class NORFlashDeviceModel final : public DeviceModel {
    size_t op_ = 0, address_ = 0, stage_ = 0;
    bool enabled_ = false;
    Bits data_, mask_;
public:
    NORFlashDeviceModel(size_t w, EndpointConfig c) : DeviceModel(w, c, H) {
        outputs["WRITE_ENABLED"] = {L};
    }
    void update(size_t t, const Signals& s) override {
        if (due_ && t >= due_) {
            due_ = 0; ++stage_;
            if (op_ == 1) {
                outputs["READ_DATA"] = storage.readLogic(address_ * (width_ / 8), width_ / 8);
                outputs["READ_VALID"] = {H}; outputs["BUSY"] = {L};
            } else if (stage_ == 1) {
                if (op_ == 2) writeWord(t, address_, data_, mask_, true);
                else {
                    const auto first = address_ / config_.block_words * config_.block_words;
                    for (size_t word = first; word < first + config_.block_words; ++word)
                        storage.writeLogic(t, word * (width_ / 8), Bits(width_, H), true);
                }
                due_ = after(t, config_.phase_ticks);
            } else if (stage_ == 2) due_ = after(t, config_.phase_ticks);
            else outputs["BUSY"] = {L};
        }
        if (!edge(s)) return;
        const auto op = number(signal(s, "CMD", 3));
        if (!op) return;
        outputs["ERROR"] = {L}; outputs["READ_VALID"] = {L}; outputs["READ_DATA"] = Bits(width_, Z);
        if (high(outputs, "BUSY") || ((op == 2 || op == 3) && !enabled_)) { outputs["ERROR"] = {H}; return; }
        if (op == 4 || op == 5) { enabled_ = op == 4; outputs["WRITE_ENABLED"] = {enabled_ ? H : L}; return; }
        op_ = op; stage_ = 0; address_ = number(signal(s, "WORD_ADDR", 32));
        data_ = signal(s, "WRITE_DATA", width_); mask_ = signal(s, "WRITE_MASK", width_ / 8);
        if (op != 1) { enabled_ = false; outputs["WRITE_ENABLED"] = {L}; }
        outputs["BUSY"] = {H}; due_ = after(t, op == 1 ? config_.phase_ticks : op == 2 ? config_.program_ticks : config_.erase_ticks);
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
