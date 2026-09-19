#include "MemoryEndpointEngine.hpp"
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
    if (technology == Technology::Container) return Status::Ok;
    if (technology == Technology::ROM && op) return Status::ReadOnly;
    if (technology == Technology::NORFlash) {
        if (op == unsigned(Operation::Write)) return Status::Unsupported;
        if (op == unsigned(Operation::EraseBlock) && (address / (width / 8)) % c.block_words)
            return Status::Alignment;
    } else if (op > unsigned(Operation::Write)) return Status::Unsupported;
    return Status::Ok;
}
NativeSequence::NativeSequence(size_t w, Technology technology, EndpointConfig config)
    : result(w, L), width_(w), technology_(technology), config_(config) { idlePins(); }
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
                         const std::function<Bits(const std::string&, size_t)>& read) {
    if (reset) aborted_ = true;
    if (phase_ == Phase::Idle) {
        // A downstream structural transaction frontend needs an initial reset.
        // Forward reset only while idle: resetting a live downstream request
        // would discard its response and prevent the relay from draining it.
        if (technology_ == Technology::Container) outputs["RST"] = {reset ? H : L};
        if (!start) done = false;
        if (reset) { available = false; return; }
        available = !done;
        if (done) return;
        const bool refresh = technology_ == Technology::DRAM && clock_edge &&
                             config_.refresh_interval_ticks && t >= next_refresh_;
        if (!start && !refresh) { aborted_ = false; return; }
        aborted_ = false; maintenance_ = refresh; available = false; status = Status::Ok;
        result.assign(width_, L); commands_.clear();
        if (refresh) {
            if (row_open_) commands_.push_back({4, 0});
            commands_.push_back({5, 0});
            next_refresh_ = after(t, config_.refresh_interval_ticks);
            beginCommand(t); return;
        }
        request_ = request;
        status = validateRequest(request, width_, technology_, config_);
        if (status != Status::Ok) { finish(t); return; }
        const auto op = number(request.op);
        const auto word = number(request.address) / (width_ / 8);
        outputs["WORD_ADDR"] = bits(32, word);
        outputs["WRITE_DATA"] = request.data; outputs["WRITE_MASK"] = request.mask;
        if (technology_ == Technology::Container) {
            outputs["REQ_ADDR"] = request.address; outputs["REQ_OP"] = request.op;
            outputs["REQ_WRITE_DATA"] = request.data; outputs["REQ_WRITE_MASK"] = request.mask;
            outputs["REQ_VALID"] = {H}; outputs["RSP_READY"] = {L};
            phase_ = Phase::RelayRequest; return;
        }
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
            if (row_open_ && row_ != row) commands_.push_back({4, 0});
            if (!row_open_ || row_ != row) commands_.push_back({1, uint32_t(row)});
            commands_.push_back({op == 0 ? 2u : 3u, uint32_t(word % config_.words_per_row)});
        } else {
            if (op) commands_.push_back({4, 0});
            commands_.push_back({op == 0 ? 1u : op == 2 ? 2u : 3u, uint32_t(word)});
        }
        beginCommand(t); return;
    }
    available = false;
    if (phase_ == Phase::RelayRequest || phase_ == Phase::RelayResponse) {
        // This path shares the external bus clock with the attached endpoint.
        if (!clock_edge) return;
        if (phase_ == Phase::RelayRequest && read("REQ_READY", 1) == Bits{H}) {
            outputs["REQ_VALID"] = {L}; outputs["RSP_READY"] = {H}; phase_ = Phase::RelayResponse;
        } else if (phase_ == Phase::RelayResponse && read("RSP_VALID", 1) == Bits{H}) {
            auto s = read("RSP_STATUS", 3);
            status = known(s) && number(s) <= 6 ? Status(number(s)) : Status::Device;
            result = read("RSP_READ_DATA", width_); finish(t);
        }
        return;
    }
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
        if (read("FAULT", 1) != Bits{L}) status = Status::Device;
        if (number(request_.op) == 0) result = read("READ_DATA", width_);
        finish(t); break;
    case Phase::StaticAccess:
        if (number(request_.op) == 0) result = read("READ_DATA", width_);
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
        if (read("ERROR", 1) != Bits{L}) { status = Status::Device; commands_.clear(); finish(t); break; }
        const auto command = commands_.front(); commands_.pop_front();
        if (technology_ == Technology::DRAM) {
            if (command.op == 1) { row_open_ = true; row_ = command.address; }
            if (command.op == 4) row_open_ = false;
        }
        if ((technology_ == Technology::DRAM && command.op == 2) ||
            (technology_ == Technology::NORFlash && command.op == 1)) {
            if (read("READ_VALID", 1) != Bits{H}) status = Status::Device;
            else result = read("READ_DATA", width_);
        }
        beginCommand(t); break;
    }
    case Phase::Release:
        phase_ = Phase::Idle; done = !maintenance_ && !aborted_;
        available = !done && !reset; break;
    default: break;
    }
    if (status != Status::Ok) result.assign(width_, L);
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
void DeviceModel::writeWord(size_t t, uint32_t word, const Bits& data, const Bits& mask, bool program) {
    auto old = storage.readLogic(word * (width_ / 8), width_ / 8);
    for (size_t byte = 0; byte < width_ / 8; ++byte) if (mask[byte] == H) {
        Bits lane(8);
        for (size_t bit = 0; bit < 8; ++bit) {
            const auto b = byte * 8 + bit;
            const auto d = stored(data[b]);
            lane[bit] = !program ? d : old[b] == L || d == L ? L : old[b] == H && d == H ? H : X;
        }
        storage.writeLogic(t, word * (width_ / 8) + byte, lane, true);
    }
}
class FFDeviceModel final : public DeviceModel {
public:
    FFDeviceModel(size_t w, EndpointConfig c) : DeviceModel(w, c, X) {}
    void update(size_t t, const Signals& s) override {
        const bool rising = edge(s); const auto word = number(signal(s, "WORD_ADDR", 32));
        if (rising && high(s, "WE")) writeWord(t, word, signal(s, "WRITE_DATA", width_), signal(s, "WRITE_MASK", width_ / 8));
        outputs["READ_DATA"] = high(s, "READ_EN") ? storage.readLogic(word * (width_ / 8), width_ / 8) : Bits(width_, L);
    }
};
class SRAMDeviceModel final : public DeviceModel {
    Signals latched_;
    bool selected_ = false;
public:
    SRAMDeviceModel(size_t w, EndpointConfig c) : DeviceModel(w, c, X) {}
    void update(size_t t, const Signals& s) override {
        if (high(s, "CE_N")) { selected_ = false; due_ = 0; outputs["READ_DATA"] = Bits(width_, Z); return; }
        if (!selected_) { selected_ = true; latched_ = s; due_ = after(t, 4 * config_.phase_ticks); }
        if (!due_ || t < due_) return;
        due_ = 0; const auto word = number(signal(latched_, "WORD_ADDR", 32));
        if (!high(latched_, "WE_N")) writeWord(t, word, signal(latched_, "WRITE_DATA", width_), signal(latched_, "WRITE_MASK", width_ / 8));
        else outputs["READ_DATA"] = storage.readLogic(word * (width_ / 8), width_ / 8);
    }
};
class ROMDeviceModel final : public DeviceModel {
public:
    ROMDeviceModel(size_t w, EndpointConfig c) : DeviceModel(w, c, L) {
        std::vector<uint8_t> image;
        for (size_t i = 0; i < c.image_hex.size(); i += 2) image.push_back(std::stoul(c.image_hex.substr(i, 2), nullptr, 16));
        storage.loadBytes(0, image);
    }
    void update(size_t, const Signals& s) override {
        outputs["READ_DATA"] = high(s, "CE_N") || high(s, "OE_N") ? Bits(width_, Z) :
            storage.readLogic(number(signal(s, "WORD_ADDR", 32)) * (width_ / 8), width_ / 8);
    }
};
class DRAMDeviceModel final : public DeviceModel {
    size_t row_ = 0, col_ = 0, refresh_row_ = 0, op_ = 0, stage_ = 0;
    bool open_ = false;
    std::map<size_t, size_t> closed_;
    Bits buffer_, data_, mask_;
public:
    DRAMDeviceModel(size_t w, EndpointConfig c) : DeviceModel(w, c, X), buffer_(w * c.words_per_row, X) {}
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
                    if (op_ == 1) { open_ = true; outputs["BUSY"] = {L}; }
                    else closed_[row_] = t;
                } else if (stage_ == 5) { refresh_row_ = (refresh_row_ + 1) % (config_.depth_words / config_.words_per_row); outputs["BUSY"] = {L}; }
            } else if (op_ == 2) {
                outputs["READ_DATA"] = Bits(buffer_.begin() + col_ * width_, buffer_.begin() + (col_ + 1) * width_);
                outputs["READ_VALID"] = {H}; outputs["BUSY"] = {L};
            } else if (op_ == 3) {
                if (stage_ == 1) {
                    for (size_t b = 0; b < width_; ++b) if (mask_[b / 8] == H) buffer_[col_ * width_ + b] = stored(data_[b]);
                    storage.writeLogic(t, start, buffer_, true);
                } else if (stage_ == 3) outputs["BUSY"] = {L};
            } else if (op_ == 4) {
                if (stage_ == 1) closed_[row_] = t;
                if (stage_ == 3) { open_ = false; outputs["BUSY"] = {L}; }
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
    NORFlashDeviceModel(size_t w, EndpointConfig c) : DeviceModel(w, c, H) {}
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
        if (op == 4 || op == 5) { enabled_ = op == 4; return; }
        op_ = op; stage_ = 0; address_ = number(signal(s, "WORD_ADDR", 32));
        data_ = signal(s, "WRITE_DATA", width_); mask_ = signal(s, "WRITE_MASK", width_ / 8);
        if (op != 1) enabled_ = false;
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
