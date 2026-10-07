#include "visualizer/VisualizerSession.hpp"
#include "visualizer/MemoryInspector.hpp"

#include "basic/LogicValue.hpp"
#include "basic/PinBase.hpp"
#include "basic/WireBase.hpp"
#include "components/Component.hpp"
#include "components/IOComponent.hpp"
#include "components/capabilities/RegisterStateView.hpp"
#include "components/selection/BuildProfile.hpp"
#include "components/capabilities/MemoryStateView.hpp"
#include "components/capabilities/MemoryActivityView.hpp"
#include "modules/rv32i/RV32IBitPatternMatcher.hpp"
#include "modules/utility/Constant.hpp"
#include "modules/utility/Rewire.hpp"
#include "rv32i/RV32IDecoder.hpp"
#include "simulator/SimulationTest.hpp"
#include "simulator/Simulator.hpp"
#include "tests/TestRegistry.hpp"
#include "tests/ComponentTestModel.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace circuit::visualizer {
namespace {

std::atomic<size_t> next_session_id{1};

void appendEscaped(std::string& out, const std::string& value) {
    static constexpr char hex[] = "0123456789abcdef";
    out.push_back('"');
    for (const unsigned char ch : value) {
        switch (ch) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (ch < 0x20) {
                    out += "\\u00";
                    out.push_back(hex[(ch >> 4) & 0xf]);
                    out.push_back(hex[ch & 0xf]);
                } else {
                    out.push_back(static_cast<char>(ch));
                }
        }
    }
    out.push_back('"');
}

void appendNumber(std::string& out, double value) {
    std::ostringstream stream;
    stream << std::setprecision(std::numeric_limits<double>::max_digits10)
           << value;
    out += stream.str();
}

class JsonObject {
public:
    explicit JsonObject(std::string& out) : out_(out) { out_ += '{'; }
    void key(const std::string& name) {
        if (!first_) out_ += ',';
        first_ = false;
        appendEscaped(out_, name);
        out_ += ':';
    }
    void string(const std::string& name, const std::string& value) {
        key(name);
        appendEscaped(out_, value);
    }
    void number(const std::string& name, size_t value) {
        key(name);
        out_ += std::to_string(value);
    }
    void integer(const std::string& name, int64_t value) {
        key(name);
        out_ += std::to_string(value);
    }
    void decimal(const std::string& name, double value) {
        key(name);
        appendNumber(out_, value);
    }
    void boolean(const std::string& name, bool value) {
        key(name);
        out_ += value ? "true" : "false";
    }
    void null(const std::string& name) {
        key(name);
        out_ += "null";
    }
    void raw(const std::string& name, const std::string& value) {
        key(name);
        out_ += value;
    }
    void end() { out_ += '}'; }

private:
    std::string& out_;
    bool first_ = true;
};

std::string stringArray(const std::vector<std::string>& values) {
    std::string out = "[";
    bool first = true;
    for (const auto& value : values) {
        if (!first) out += ',';
        first = false;
        appendEscaped(out, value);
    }
    out += ']';
    return out;
}

template<typename T, typename Render>
std::string arrayJson(const std::vector<T>& values, Render render) {
    std::string out = "[";
    bool first = true;
    for (const auto& value : values) {
        if (!first) out += ',';
        first = false;
        out += render(value);
    }
    out += ']';
    return out;
}

char logicToken(LogicValue value) {
    switch (value) {
        case LogicValue::LOW: return '0';
        case LogicValue::HIGH: return '1';
        case LogicValue::HIGH_Z: return 'Z';
        case LogicValue::UNKNOWN: return 'X';
    }
    return 'X';
}

std::string logicVectorValue(const std::vector<LogicValue>& values) {
    if (values.empty()) return "";
    if (values.size() == 1) return std::string(1, logicToken(values[0]));
    std::string result(values.size(), 'X');
    for (size_t index = 0; index < values.size(); ++index) {
        result[values.size() - index - 1] = logicToken(values[index]);
    }
    return result;
}

std::string hexValue(const std::string& bits) {
    if (bits.empty() || std::any_of(bits.begin(), bits.end(), [](char bit) {
            return bit != '0' && bit != '1';
        })) {
        return {};
    }
    const size_t width = std::max<size_t>(1, (bits.size() + 3) / 4);
    uint64_t value = 0;
    for (const char bit : bits) {
        value = (value << 1) | static_cast<uint64_t>(bit == '1');
    }
    std::ostringstream stream;
    stream << "0x" << std::hex << std::nouppercase << std::setfill('0')
           << std::setw(static_cast<int>(width)) << value;
    return stream.str();
}

bool binaryValue(const std::string& bits, uint64_t& value) {
    if (bits.empty() || bits.size() > 64) return false;
    value = 0;
    for (const char bit : bits) {
        if (bit != '0' && bit != '1') return false;
        value = (value << 1) | static_cast<uint64_t>(bit == '1');
    }
    return true;
}

bool naturalLess(const std::string& left, const std::string& right) {
    size_t lhs = 0;
    size_t rhs = 0;
    while (lhs < left.size() && rhs < right.size()) {
        const bool left_digit = std::isdigit(static_cast<unsigned char>(left[lhs]));
        const bool right_digit = std::isdigit(static_cast<unsigned char>(right[rhs]));
        if (left_digit && right_digit) {
            size_t left_end = lhs;
            size_t right_end = rhs;
            while (left_end < left.size() && std::isdigit(static_cast<unsigned char>(left[left_end]))) ++left_end;
            while (right_end < right.size() && std::isdigit(static_cast<unsigned char>(right[right_end]))) ++right_end;
            const auto left_number = std::stoull(left.substr(lhs, left_end - lhs));
            const auto right_number = std::stoull(right.substr(rhs, right_end - rhs));
            if (left_number != right_number) return left_number < right_number;
            lhs = left_end;
            rhs = right_end;
            continue;
        }
        const char left_char = static_cast<char>(std::tolower(static_cast<unsigned char>(left[lhs])));
        const char right_char = static_cast<char>(std::tolower(static_cast<unsigned char>(right[rhs])));
        if (left_char != right_char) return left_char < right_char;
        ++lhs;
        ++rhs;
    }
    return left.size() < right.size();
}

bool endsWith(const std::string& value, const std::string& suffix) {
    return value.size() >= suffix.size()
        && value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string normalizedProfileFingerprint(
    const std::shared_ptr<Component>& component) {
    if (!component) return "explicit";
    const auto fingerprint = component->getProfileFingerprint();
    return fingerprint.empty() ? "explicit" : fingerprint;
}

std::string componentLayoutType(const std::shared_ptr<Component>& component) {
    const std::string type = component ? component->getTypeName() : "Component";
    if (!component) return type;
    const auto& children = component->getChildren();
    auto has_child = [&](const auto& predicate) {
        return std::any_of(children.begin(), children.end(), [&](const auto& child) {
            return predicate(child->getName());
        });
    };
    if (type.starts_with("MemoryController<ff,") && !children.empty() &&
        !has_child([](const std::string& name) { return name == "RESPONSE"; }))
        return type.substr(0, type.size() - 1) + ",always-ready>";
    if (type == "Decoder5to32" && has_child([](const std::string& name) { return name == "OUT0_ZERO"; }))
        return "Decoder5to32<x0-unused>";
    if (type.ends_with("PipelineRegister") &&
        has_child([](const std::string& name) { return name.ends_with("_SPLIT"); }))
        return type + "<compact>";
    // Display the FF-specific names without invalidating saved type layouts.
    if (type.starts_with("FFMemory")) return "FFArray" + type.substr(8);
    if (type.starts_with("FFWord<")) return "MemoryWord<FFArray," + type.substr(7);
    if (type.starts_with("FFStorageBank<")) return "MemoryStorageBank<FFArray," + type.substr(14);
    const auto io = std::dynamic_pointer_cast<IOComponent>(component);
    if (io && type == "Rewire") {
        // Rewires with different pin counts need independent aspect ratios.
        // Keep the logical type/implementation identity unchanged.
        return "Rewire<in=" + std::to_string(io->getAllInputPins().size()) +
               ",out=" + std::to_string(io->getAllOutputPins().size()) + ">";
    }
    if (io && type == "BitSplitter") {
        const auto pin = io->getInputPinDynamic("IN");
        return pin ? "BitSplitter<" + std::to_string(pin->getWidth()) + ">" : type;
    }
    if (io && type == "BitJoiner") {
        const auto pin = io->getOutputPinDynamic("OUT");
        return pin ? "BitJoiner<" + std::to_string(pin->getWidth()) + ">" : type;
    }
    if (io && type == "ConstantValue") {
        const auto pin = io->getOutputPinDynamic("OUT");
        return pin ? "ConstantValue<" + std::to_string(pin->getWidth()) + ">" : type;
    }
    if (const auto matcher = std::dynamic_pointer_cast<RV32IBitPatternMatcher>(component)) {
        std::ostringstream stream;
        stream << "RV32IBitPatternMatcher<mask=0x" << std::hex
               << std::setfill('0') << std::setw(8) << matcher->mask()
               << ",value=0x" << std::setw(8) << matcher->value() << ">";
        return stream.str();
    }
    return type;
}

std::string constantValueJson(const std::shared_ptr<Component>& component) {
    if (auto value = std::dynamic_pointer_cast<ConstantValue<1>>(component)) return std::to_string(value->getConstantValue());
    if (auto value = std::dynamic_pointer_cast<ConstantValue<2>>(component)) return std::to_string(value->getConstantValue());
    if (auto value = std::dynamic_pointer_cast<ConstantValue<4>>(component)) return std::to_string(value->getConstantValue());
    if (auto value = std::dynamic_pointer_cast<ConstantValue<8>>(component)) return std::to_string(value->getConstantValue());
    if (auto value = std::dynamic_pointer_cast<ConstantValue<32>>(component)) return std::to_string(value->getConstantValue());
    return {};
}

std::string rewireJson(const std::shared_ptr<Component>& component) {
    const auto rewire = std::dynamic_pointer_cast<Rewire>(component);
    if (!rewire) return {};
    std::string out;
    JsonObject object(out);
    auto specs = [](const std::vector<Rewire::WireSpec>& values) {
        return arrayJson(values, [](const Rewire::WireSpec& spec) {
            std::string result;
            JsonObject item(result);
            item.string("name", spec.name);
            item.number("width", spec.width);
            item.end();
            return result;
        });
    };
    object.raw("inputs", specs(rewire->getInputSpecs()));
    object.raw("outputs", specs(rewire->getOutputSpecs()));
    object.raw("mappings", arrayJson(rewire->getBitMappings(), [](const Rewire::BitMap& mapping) {
        std::string result;
        JsonObject item(result);
        item.string("srcWire", mapping.src_wire);
        item.number("srcBit", mapping.src_bit);
        item.string("dstWire", mapping.dst_wire);
        item.number("dstBit", mapping.dst_bit);
        item.end();
        return result;
    }));
    const char* unmapped = "UNKNOWN";
    if (rewire->getUnmappedDefault() == Rewire::UnmappedBitValue::LOW) unmapped = "LOW";
    if (rewire->getUnmappedDefault() == Rewire::UnmappedBitValue::HIGH) unmapped = "HIGH";
    object.string("unmappedDefault", unmapped);
    object.end();
    return out;
}

std::string memoryWordJson(
    uint32_t address,
    const std::vector<LogicValue>& word,
    bool decode_instruction) {
    const auto bits = logicVectorValue(word);
    const auto hex = hexValue(bits);
    std::ostringstream address_stream;
    address_stream << "0x" << std::hex << std::setfill('0') << std::setw(8) << address;
    std::string out;
    JsonObject object(out);
    object.number("address", address);
    object.string("addressHex", address_stream.str());
    object.string("bits", bits);
    if (hex.empty()) object.null("hex"); else object.string("hex", hex);
    object.boolean("known", !hex.empty());
    if (decode_instruction) {
        uint64_t raw = 0;
        if (binaryValue(bits, raw)) {
            object.string("instructionText", rv32i::RV32IDecoder::disassemble(static_cast<uint32_t>(raw), address));
        }
    }
    object.end();
    return out;
}

} // namespace

std::vector<size_t> memoryActivityTimestamps(const std::shared_ptr<Component>& root) {
    std::vector<size_t> times;
    const auto visit = [&](auto&& self, const std::shared_ptr<Component>& component) -> void {
        if (auto view = std::dynamic_pointer_cast<MemoryActivityView>(component))
            view->appendMemoryActivityTimes(times);
        for (const auto& child : component->getChildren()) self(self, child);
    };
    if (root) visit(visit, root);
    std::sort(times.begin(), times.end());
    times.erase(std::unique(times.begin(), times.end()), times.end());
    return times;
}

MemoryInspector::MemoryInspector(std::shared_ptr<Component> component, const Simulator& simulator)
    : component_(std::dynamic_pointer_cast<IOComponent>(component)), simulator_(simulator) {
    if (!component_) throw std::invalid_argument("Memory inspection requires an IO component");
    // A visible native MEMORY inherits its enclosing endpoint's bus context.
    for (auto current = component; current; current = current->getParent()) {
        auto io = std::dynamic_pointer_cast<IOComponent>(current);
        if (io && std::string(io->getTypeName()).starts_with("MemoryEndpoint<")) {
            endpoint_ = io;
            break;
        }
    }
}

std::shared_ptr<PinBase> MemoryInspector::pin(const std::shared_ptr<IOComponent>& io, const std::string& name) const {
    if (!io) return {};
    if (auto it = io->getAllInputPins().find(name); it != io->getAllInputPins().end()) return it->second;
    if (auto it = io->getAllOutputPins().find(name); it != io->getAllOutputPins().end()) return it->second;
    return {};
}

std::string MemoryInspector::value(const std::shared_ptr<IOComponent>& io, const std::string& name, size_t time) const {
    auto p = pin(io, name);
    if (!p) return {};
    const auto& history = simulator_.signalHistory(p);
    auto it = std::upper_bound(history.begin(), history.end(), time,
        [](size_t t, const auto& entry) { return t < entry.first; });
    return it == history.begin() ? std::string(p->getWidth(), 'X') : logicVectorValue(std::prev(it)->second);
}

void MemoryInspector::rebuildTransactions() {
    if (!endpoint_ || revision_ == simulator_.historyRevision()) return;
    revision_ = simulator_.historyRevision();
    edges_.clear();
    std::set<size_t> times;
    for (const auto& name : {"CLK", "RST"})
        for (const auto& [time, bits] : simulator_.signalHistory(pin(endpoint_, name))) {
            (void)bits;
            times.insert(time);
        }
    std::optional<size_t> pending, completed;
    for (const auto time : times) {
        // The interface is synchronous: request/ready and response/ready must
        // be stable before the edge. Reading post-edge pins can instead see the
        // next instruction, or READY deasserted by the just-accepted request.
        const bool reset = value(endpoint_, "RST", time) != "0";
        const bool edge = time > 0 && value(endpoint_, "CLK", time - 1) == "0"
            && value(endpoint_, "CLK", time) == "1";
        if (reset) {
            pending.reset(); completed.reset();
        } else if (edge) {
            const bool accept = value(endpoint_, "REQ_VALID", time - 1) == "1"
                && value(endpoint_, "REQ_READY", time - 1) == "1";
            const bool consume = value(endpoint_, "RSP_VALID", time - 1) == "1"
                && value(endpoint_, "RSP_READY", time - 1) == "1";
            const bool older = pending.has_value();
            if (consume && older) { completed = pending; pending.reset(); }
            if (accept) {
                if (consume && !older) completed = time; // zero-wait bypass
                else pending = time;                    // held/delayed response
            }
        } else continue;
        if (edges_.empty() || edges_.back().pending != pending || edges_.back().completed != completed)
            edges_.push_back({time, pending, completed});
    }
}

std::string MemoryInspector::stateJson(size_t index, size_t time, const std::string& scenario,
                                      int64_t address, size_t count) {
    if (address < -1 || address > int64_t(UINT32_MAX) || count == 0 || count > 256)
        throw std::invalid_argument("Memory page requires a 32-bit byte address and 1..256 words");
    std::string out;
    JsonObject object(out);
    object.number("index", index); object.number("time", time);
    object.string("componentId", component_->getID()); object.string("type", component_->getTypeName());
    auto view = std::dynamic_pointer_cast<MemoryStateView>(component_);
    if (!view) { object.boolean("supported", false); object.end(); return out; }
    const MemoryStorage* storage = nullptr;
    try { storage = &view->memoryStorage(); }
    catch (const std::logic_error&) { object.boolean("supported", false); object.end(); return out; }
    const auto& memory = *storage;
    if (!memory.isHistoryRecordingEnabled() || !simulator_.isHistoryRecordingEnabled()) {
        object.boolean("supported", false); object.end(); return out;
    }
    object.boolean("supported", true);
    std::string role = scenario == "rv32i-program-loader" ? "instruction" : "generic";
    for (std::shared_ptr<Component> current = component_; current; current = current->getParent()) {
        if (current->getName() == "INSTRUCTION_MEMORY") { role = "instruction"; break; }
        if (current->getName() == "DATA_MEMORY") { role = "data"; break; }
    }
    if (memory.wordBits() != 32 && role == "instruction") role = "generic";
    const size_t word_bytes = memory.wordBits() / 8;
    bool has_address = false, active = false;
    uint64_t active_address = 0;
    std::string transaction = "null";
    if (endpoint_) {
        rebuildTransactions();
        auto it = std::upper_bound(edges_.begin(), edges_.end(), time,
            [](size_t t, const Edge& edge) { return t < edge.time; });
        const Edge* edge = it == edges_.begin() ? nullptr : &*std::prev(it);
        auto accepted = edge ? edge->pending : std::optional<size_t>{};
        const auto completed = edge ? edge->completed : std::optional<size_t>{};
        const bool reset = value(endpoint_, "RST", time) != "0";
        const bool request = value(endpoint_, "REQ_VALID", time) == "1";
        const bool ready = value(endpoint_, "REQ_READY", time) == "1";
        const bool response = value(endpoint_, "RSP_VALID", time) == "1";
        const bool take = value(endpoint_, "RSP_READY", time) == "1";
        if (reset) accepted.reset();
        const bool live = !reset && !accepted && request;
        const bool completing = !reset && !accepted && !live && response && completed.has_value();
        if (completing) accepted = completed;
        active = !reset && (accepted.has_value() || live);
        const size_t sample_time = accepted ? *accepted - 1 : time;
        const auto operation_bits = value(endpoint_, "REQ_OP", sample_time);
        uint64_t operation = 0, status = 0;
        const bool known_op = binaryValue(operation_bits, operation);
        static const std::vector<std::string> operations{"READ", "WRITE", "PROGRAM", "ERASE"};
        const auto operation_name = !known_op ? "UNKNOWN" : operation < operations.size() ? operations[operation] : "INVALID";
        has_address = active && binaryValue(value(endpoint_, "REQ_ADDR", sample_time), active_address);
        const bool known_status = binaryValue(value(endpoint_, "RSP_STATUS", time), status);
        static const std::vector<std::string> statuses{"OK", "ALIGNMENT", "RANGE", "READ_ONLY", "UNSUPPORTED", "DEVICE", "INVALID_REQUEST"};
        transaction.clear();
        JsonObject tx(transaction);
        tx.string("phase", reset ? "reset" : response ? (take ? "response" : "response-held")
            : accepted ? "pending" : live ? (ready ? "offered" : "request-wait") : "idle");
        tx.string("source", accepted ? "accepted" : live ? "live" : "none");
        tx.string("operation", active ? operation_name : "");
        tx.string("operationBits", active ? operation_bits : "");
        if (has_address) tx.number("address", active_address); else tx.null("address");
        if (accepted) tx.number("acceptedTime", *accepted); else tx.null("acceptedTime");
        if (completed) tx.number("lastCompletedRequestTime", *completed); else tx.null("lastCompletedRequestTime");
        tx.string("writeData", active ? value(endpoint_, "REQ_WRITE_DATA", sample_time) : "");
        tx.string("writeMask", active ? value(endpoint_, "REQ_WRITE_MASK", sample_time) : "");
        tx.string("status", !response ? "" : known_status && status < statuses.size() ? statuses[status] : "UNKNOWN");
        tx.string("endpointId", endpoint_->getID());
        tx.end();
    } else {
        has_address = binaryValue(value(component_, "WORD_ADDR", time), active_address);
        if (has_address) active_address *= word_bytes;
        const auto high = [&](const std::string& p) { return value(component_, p, time) == "1"; };
        const auto low = [&](const std::string& p) { return value(component_, p, time) == "0"; };
        active = high("READ_EN") || high("WE") || high("WRITE_EN")
            || (low("CE_N") && (low("WE_N") || (low("OE_N") && !low("WE_N"))));
        // Native command devices retain snapshot-only addressing: DRAM's ADDR
        // is a row/column command operand, not a universal byte address.
        if (pin(component_, "CMD")) { has_address = false; active = false; }
    }
    if (active_address > UINT32_MAX) has_address = false;
    const uint32_t active_base = uint32_t(active_address - active_address % word_bytes);
    const bool in_range = has_address && active_address < memory.capacityBytes();
    active = active && in_range;
    if (address >= 0 && uint64_t(address) >= memory.capacityBytes())
        throw std::invalid_argument("Memory page address is outside this device");
    const size_t page_bytes = count * word_bytes;
    const uint32_t page_address = address >= 0 ? uint32_t(address - address % word_bytes)
        : active ? uint32_t(active_base / page_bytes * page_bytes) : 0;
    const size_t page_count = std::min(count, (memory.capacityBytes() - page_address) / word_bytes);
    const auto touched = memory.getTouchedWordsAtTime(time, 256);
    const auto touched_count = memory.getTouchedWordCountAtTime(time);
    object.number("capacityBytes", memory.capacityBytes()); object.number("capacityWords", memory.capacityWords());
    object.number("wordBits", memory.wordBits()); object.boolean("historyEnabled", true);
    object.string("memoryRole", role); object.boolean("windowActive", active);
    if (has_address) {
        object.number("activeAddress", active_address); object.number("activeBaseAddress", active_base);
    } else { object.null("activeAddress"); object.null("activeBaseAddress"); }
    object.number("touchedWordCount", touched_count); object.boolean("touchedWordsTruncated", touched_count > touched.size());
    object.number("pageAddress", page_address); object.number("pageWords", page_count);
    object.number("pageSize", count); object.boolean("manualPage", address >= 0);
    object.boolean("hasPreviousPage", page_address > 0);
    object.boolean("hasNextPage", page_address + page_count * word_bytes < memory.capacityBytes());
    const auto render = [&](const auto& word) { return memoryWordJson(word.first, word.second, role == "instruction"); };
    object.raw("touchedWords", arrayJson(touched, render));
    object.raw("windowWords", arrayJson(memory.getWordsAtTime(time, page_address, page_count), render));
    object.raw("transaction", transaction);
    std::shared_ptr<MemoryActivityView> activity_view = std::dynamic_pointer_cast<MemoryActivityView>(endpoint_);
    if (!activity_view && endpoint_) {
        for (const auto& controller : endpoint_->getChildren())
            if (controller->getName() == "CONTROLLER")
                for (const auto& child : controller->getChildren())
                    if (auto candidate = std::dynamic_pointer_cast<MemoryActivityView>(child)) activity_view = candidate;
    }
    if (activity_view) {
        auto activity = activity_view->memoryActivityAtTime(time);
        // These native-device status pins need not be connected to the
        // controller, but remain useful when inspecting the whole endpoint.
        for (const auto& child : endpoint_->getChildren()) if (child->getName() == "MEMORY") {
            auto device = std::dynamic_pointer_cast<IOComponent>(child);
            for (auto name : {"ROW_OPEN", "WRITE_ENABLED"}) if (pin(device, name)) {
                std::vector<LogicValue> bits;
                for (const auto bit : value(device, name, time))
                    bits.push_back(bit == '1' ? LogicValue::HIGH : bit == '0' ? LogicValue::LOW
                        : bit == 'Z' ? LogicValue::HIGH_Z : LogicValue::UNKNOWN);
                activity.signals[name] = std::move(bits);
            }
        }
        object.key("deviceActivity"); JsonObject detail(out);
        detail.string("phase", activity.phase); detail.boolean("maintenance", activity.maintenance);
        detail.key("signals"); JsonObject signals(out);
        for (const auto& [name, bits] : activity.signals) signals.string(name, logicVectorValue(bits));
        signals.end(); detail.end();
    } else object.null("deviceActivity");
    std::string ports;
    JsonObject port_object(ports);
    for (const auto& [name, p] : [&] {
        auto pins = component_->getAllInputPins();
        pins.insert(component_->getAllOutputPins().begin(), component_->getAllOutputPins().end());
        return pins;
    }()) {
        (void)p;
        const auto bits = value(component_, name, time), hex = hexValue(bits);
        port_object.key(name); JsonObject item(ports); item.string("value", bits);
        if (hex.empty()) item.null("hex"); else item.string("hex", hex);
        item.end();
    }
    port_object.end(); object.raw("ports", ports); object.end();
    return out;
}

struct VisualizerSession::ComponentRecord {
    std::shared_ptr<Component> handle;
    std::string id;
    std::string name;
    std::string type;
    std::string layout_type;
    size_t depth = 0;
    size_t parent_index = 0;
    bool has_parent = false;
    size_t input_pin_count = 0;
    size_t output_pin_count = 0;
};

struct VisualizerSession::PinRecord {
    std::shared_ptr<PinBase> handle;
    std::string id;
    std::string component_id;
    std::string name;
    std::string type;
    size_t width = 1;
    size_t state_index = 0;
};

struct VisualizerSession::WireRecord {
    std::shared_ptr<WireBase> handle;
    std::string id;
    std::string name;
    std::string owner_id;
    std::string source_pin_id;
    bool has_source = false;
    std::vector<std::string> sink_pin_ids;
    size_t width = 1;
    size_t state_index = 0;
};

VisualizerSession::VisualizerSession(
    std::string registered_test_name,
    std::string scenario_key,
    std::map<std::string, std::string> exact_overrides,
    size_t profile_revision)
    : registered_test_name_(std::move(registered_test_name)),
      scenario_key_(std::move(scenario_key)),
      exact_overrides_(std::move(exact_overrides)),
      profile_revision_(profile_revision) {
    test_ = createTestByName(registered_test_name_);
    if (!test_) {
        throw std::invalid_argument("Unknown simulation test: " + registered_test_name_);
    }
    profile_editable_ = test_->supportsBuildProfile();
    if (!exact_overrides_.empty()) {
        if (!profile_editable_) {
            throw std::invalid_argument("Scenario does not accept component fidelity changes");
        }
        auto profile = test_->getBuildProfile();
        std::vector<circuit::ProfileRule> rules;
        rules.reserve(exact_overrides_.size());
        for (const auto& [path, requested] : exact_overrides_) {
            circuit::Fidelity fidelity;
            if (requested == "structural") {
                fidelity = circuit::Fidelity::Structural;
            } else if (requested == "behavioral") {
                fidelity = circuit::Fidelity::Behavioral;
            } else {
                throw std::invalid_argument(
                    "Fidelity for '" + path + "' must be structural or behavioral");
            }
            rules.push_back(circuit::preferFidelity(
                fidelity,
                circuit::ProfileSelector::exactPath(path),
                "visualizer exact-path override"));
        }
        test_->setBuildProfile(circuit::withProfileOverrides(
            std::move(profile), std::move(rules), "visualizer-wasm"));
    }

    test_->setupCircuit();
    root_ = test_->getRoot();
    if (!root_) throw std::runtime_error("C++ test scenario did not produce a root component");
    simulator_ = test_->getSimulator();
    if (!simulator_) throw std::runtime_error("C++ test scenario did not produce a simulator");
    if (!test_->isSimulationPrecomputed()) {
        test_->scheduleInitialEvents(0);
        // setupCircuit() may already have clocked reset and recorded initial
        // pin values. Continue that fresh scenario; runAndRecord would clear
        // its reset history and turn unchanged-until-halt pins into X on seek.
        simulator_->advanceAndRecord(test_->getRunDuration());
    }

    timestamps_ = simulator_->getUniqueTimestamps();
    const auto activity_times = memoryActivityTimestamps(root_);
    timestamps_.insert(timestamps_.end(), activity_times.begin(), activity_times.end());
    for (const auto& checkpoint : test_->getCheckpoints()) {
        timestamps_.push_back(checkpoint.time);
    }
    std::sort(timestamps_.begin(), timestamps_.end());
    timestamps_.erase(std::unique(timestamps_.begin(), timestamps_.end()), timestamps_.end());
    if (timestamps_.empty()) timestamps_.push_back(0);
    simulator_->setCircuitStateAtTime(timestamps_.front());

    session_id_ = "wasm-" + std::to_string(next_session_id.fetch_add(1));
    const auto fingerprint = normalizedProfileFingerprint(root_);
    layout_key_ = fingerprint == "explicit"
        ? scenario_key_
        : scenario_key_ + "@" + fingerprint;
    buildTopology();
}

VisualizerSession::~VisualizerSession() = default;

std::string VisualizerSession::exampleRowsJson() const {
    const auto* test = dynamic_cast<const circuit::test::ComponentScenarioTest*>(test_.get());
    if (!test || test->getRunArtifacts().size() != 1)
        throw std::invalid_argument("Example tables require one recorded component scenario");
    const auto& artifact = test->getRunArtifacts().front();
    const auto& scenarios = test->getSpec().scenarios;
    const auto scenario = std::find_if(scenarios.begin(), scenarios.end(), [&](const auto& item) {
        return item.id == artifact.scenario_id;
    });
    if (!artifact.completed || scenario == scenarios.end())
        throw std::runtime_error("Example table requires a completed scenario run");
    auto root = std::dynamic_pointer_cast<IOComponent>(root_);
    if (!root) throw std::runtime_error("Example table requires an IO component");
    const auto schema = [](const auto& pins) {
        std::vector<std::shared_ptr<PinBase>> ordered;
        for (const auto& [name, pin] : pins) ordered.push_back(pin);
        return arrayJson(ordered, [](const auto& pin) {
            std::string result;
            JsonObject item(result);
            item.string("name", pin->getName());
            item.number("width", pin->getWidth());
            item.end();
            return result;
        });
    };
    const auto namedValues = [](const circuit::test::NamedValues& values) {
        std::string result;
        JsonObject object(result);
        for (const auto& [name, bits] : values) object.string(name, logicVectorValue(bits));
        object.end();
        return result;
    };
    circuit::test::NamedValues inputs;
    std::vector<std::string> rows;
    for (size_t action_index = 0; action_index < scenario->actions.size(); ++action_index) {
        const auto& action = scenario->actions[action_index];
        // Sequential scenarios change only selected inputs. Include preparation
        // actions (e.g. lowering CLK) even when they do not emit a checkpoint.
        for (const auto& [name, value] : action.inputs) inputs[name] = value;
        if (!action.emit_checkpoint) continue;
        const auto index = rows.size();
        const auto& checkpoint = artifact.checkpoints.at(index);
        // ComponentTestRunner drains every action before snapshotting, including
        // transaction/commit-labelled observations. Preserve that label instead
        // of dropping valid status-unit examples or calling them clock edges.
        const char* kind = nullptr;
        switch (action.checkpoint_kind) {
            case circuit::test::CheckpointKind::Settled: kind = "settled"; break;
            case circuit::test::CheckpointKind::AfterEdge: kind = "after-edge"; break;
            case circuit::test::CheckpointKind::TransactionComplete: kind = "transaction-complete"; break;
            case circuit::test::CheckpointKind::InstructionCommit: kind = "instruction-commit"; break;
            default: throw std::runtime_error("Unsupported example checkpoint kind");
        }
        if (action.checkpoint_id != checkpoint.id || action.checkpoint_kind != checkpoint.kind)
            throw std::runtime_error("Example checkpoint does not match its declared action");
        // Retained inputs come from declared drives, never guessed defaults.
        // Unchecked outputs remain absent from the documented expectations.
        if (inputs.size() != root->getAllInputPins().size())
            throw std::runtime_error("Every example input must have been explicitly driven");
        for (const auto& [name, expected] : action.expected_outputs) {
            const auto actual = checkpoint.pins.outputs.find(name);
            if (actual == checkpoint.pins.outputs.end() || actual->second != expected)
                throw std::runtime_error("Cannot export a failing example row: " + checkpoint.id);
        }
        const auto time = std::lower_bound(timestamps_.begin(), timestamps_.end(), checkpoint.actual_time);
        if (time == timestamps_.end() || *time != checkpoint.actual_time)
            throw std::runtime_error("Example checkpoint is missing from the visualizer timeline");
        std::string result;
        JsonObject item(result);
        item.string("checkpoint", checkpoint.id);
        item.number("rowIndex", index);
        item.number("actionIndex", action_index);
        item.string("kind", kind);
        item.string("detail", action.detail);
        item.number("time", checkpoint.actual_time);
        item.number("timelineIndex", static_cast<size_t>(std::distance(timestamps_.begin(), time)));
        item.raw("inputs", namedValues(inputs));
        item.raw("drivenInputs", namedValues(action.inputs));
        item.raw("expectedOutputs", namedValues(action.expected_outputs));
        item.end();
        rows.push_back(std::move(result));
    }
    if (rows.size() != artifact.checkpoints.size())
        throw std::runtime_error("Example table checkpoint count does not match the recorded run");
    std::string result;
    JsonObject object(result);
    object.number("schemaVersion", 1);
    object.string("scenario", scenario_key_);
    object.string("registeredTest", registered_test_name_);
    object.string("rootType", root_->getTypeName());
    object.string("profileFingerprint", normalizedProfileFingerprint(root_));
    object.string("profileName", artifact.profile->name());
    object.string("fidelity", root_->getSelectedFidelity());
    object.raw("inputs", schema(root->getAllInputPins()));
    object.raw("outputs", schema(root->getAllOutputPins()));
    object.raw("rows", arrayJson(rows, [](const auto& row) { return row; }));
    object.end();
    return result;
}

std::string VisualizerSession::visualizableTestsJson() {
    std::vector<const TestRegistryEntry*> entries;
    for (const auto& entry : getTestRegistry()) {
        if (entry.visualizable
            && entry.name != "WireTemplateTest"
            && entry.name != "RewireValidationTest") {
            entries.push_back(&entry);
        }
    }
    return arrayJson(entries, [](const TestRegistryEntry* entry) {
        std::string out;
        JsonObject object(out);
        object.string("name", entry->name);
        object.string("logicalTestId", entry->logical_test_id);
        object.string("kind", toString(entry->kind));
        object.string("scenarioId", entry->scenario_id);
        object.raw("contractIds", stringArray(entry->contract_ids));
        object.raw("labels", stringArray(entry->labels));
        object.boolean("visualizable", entry->visualizable);
        object.end();
        return out;
    });
}

void VisualizerSession::buildTopology() {
    std::function<void(const std::shared_ptr<Component>&, size_t, bool, size_t)> walk;
    walk = [&](const std::shared_ptr<Component>& component,
               size_t parent_index,
               bool has_parent,
               size_t depth) {
        ComponentRecord component_record;
        component_record.handle = component;
        component_record.id = component->getID();
        component_record.name = component->getName();
        component_record.type = component->getTypeName();
        component_record.layout_type = componentLayoutType(component);
        component_record.depth = depth;
        component_record.parent_index = parent_index;
        component_record.has_parent = has_parent;
        const size_t component_index = components_.size();
        component_index_by_id_[component_record.id] = component_index;

        if (const auto io = std::dynamic_pointer_cast<IOComponent>(component)) {
            std::vector<std::pair<std::string, std::shared_ptr<PinBase>>> input_pins(
                io->getAllInputPins().begin(), io->getAllInputPins().end());
            std::vector<std::pair<std::string, std::shared_ptr<PinBase>>> output_pins(
                io->getAllOutputPins().begin(), io->getAllOutputPins().end());
            auto sort_pins = [](const auto& left, const auto& right) {
                return naturalLess(left.first, right.first);
            };
            std::sort(input_pins.begin(), input_pins.end(), sort_pins);
            std::sort(output_pins.begin(), output_pins.end(), sort_pins);
            component_record.input_pin_count = input_pins.size();
            component_record.output_pin_count = output_pins.size();
            auto add_pin = [&](const auto& item, const char* type) {
                PinRecord pin;
                pin.handle = item.second;
                pin.id = item.second->getID();
                pin.component_id = component_record.id;
                pin.name = item.first;
                pin.type = type;
                pin.width = std::max<size_t>(1, item.second->getWidth());
                pin.state_index = pins_.size();
                if (!pin_index_by_component_direction_name_.emplace(
                        std::make_tuple(pin.component_id, pin.type, pin.name), pin.state_index).second) {
                    throw std::runtime_error("Visualizer topology has a duplicate pin: " + pin.id);
                }
                pin_indices_by_component_[pin.component_id].push_back(pin.state_index);
                pins_.push_back(std::move(pin));
            };
            for (const auto& pin : input_pins) add_pin(pin, "input");
            for (const auto& pin : output_pins) add_pin(pin, "output");
        }
        components_.push_back(std::move(component_record));

        for (const auto& wire_handle : component->getAllWires()) {
            WireRecord wire;
            wire.handle = wire_handle;
            wire.id = wire_handle->getID();
            wire.name = wire_handle->getName();
            wire.owner_id = component->getID();
            if (const auto source = wire_handle->getSourcePinBase()) {
                wire.has_source = true;
                wire.source_pin_id = source->getID();
            }
            for (const auto& weak_sink : wire_handle->getSinkPinsBase()) {
                if (const auto sink = weak_sink.lock()) {
                    wire.sink_pin_ids.push_back(sink->getID());
                }
            }
            wire.width = std::max<size_t>(1, wire_handle->getWidth());
            wire.state_index = wires_.size();
            wire_indices_by_owner_[wire.owner_id].push_back(wire.state_index);
            wires_.push_back(std::move(wire));
        }

        for (const auto& child : component->getChildren()) {
            walk(child, component_index, true, depth + 1);
        }
    };
    walk(root_, 0, false, 0);

    std::set<std::string> pin_ids;
    for (const auto& pin : pins_) {
        if (!pin_ids.insert(pin.id).second) {
            throw std::runtime_error("Visualizer topology has a duplicate pin ID: " + pin.id);
        }
    }
    for (const auto& wire : wires_) {
        if (wire.has_source && pin_ids.count(wire.source_pin_id) == 0) {
            throw std::runtime_error("Visualizer topology has an unexposed source pin: " + wire.source_pin_id);
        }
        for (const auto& sink : wire.sink_pin_ids) {
            if (pin_ids.count(sink) == 0) {
                throw std::runtime_error("Visualizer topology has an unexposed sink pin: " + sink);
            }
        }
    }
}

size_t VisualizerSession::clampedIndex(size_t index) const {
    return std::min(index, timestamps_.size() - 1);
}

std::string VisualizerSession::pinValue(const std::shared_ptr<PinBase>& pin) const {
    return pin ? logicVectorValue(pin->getValueAsVector()) : "X";
}

std::string VisualizerSession::topologySliceJson(
    const std::vector<std::string>& owner_ids) const {
    if (owner_ids.empty()) throw std::invalid_argument("Topology scope request requires at least one owner id");
    if (owner_ids.size() > 512) throw std::invalid_argument("Topology scope request may contain at most 512 owners");
    std::vector<std::string> scopes;
    std::set<std::string> seen;
    std::set<std::string> pin_component_ids;
    for (const auto& owner_id : owner_ids) {
        if (!seen.insert(owner_id).second) continue;
        const auto found = component_index_by_id_.find(owner_id);
        if (found == component_index_by_id_.end()) {
            throw std::invalid_argument("Unknown topology scope owner '" + owner_id + "'");
        }
        scopes.push_back(owner_id);
        pin_component_ids.insert(owner_id);
        for (const auto& child : components_[found->second].handle->getChildren()) {
            pin_component_ids.insert(child->getID());
        }
    }

    auto pin_json = [&](const PinRecord& pin) {
        std::string out;
        JsonObject object(out);
        object.string("id", pin.id);
        object.string("componentId", pin.component_id);
        object.string("name", pin.name);
        object.string("type", pin.type);
        object.number("width", pin.width);
        object.number("stateIndex", pin.state_index);
        object.end();
        return out;
    };
    auto wire_json = [&](const WireRecord& wire) {
        std::string out;
        JsonObject object(out);
        object.string("id", wire.id);
        object.string("name", wire.name);
        object.string("ownerId", wire.owner_id);
        if (wire.has_source) object.string("sourcePinId", wire.source_pin_id); else object.null("sourcePinId");
        object.raw("sinkPinIds", stringArray(wire.sink_pin_ids));
        object.number("width", wire.width);
        object.number("stateIndex", wire.state_index);
        object.end();
        return out;
    };

    std::string pins_json = "[";
    bool first = true;
    for (const auto& pin : pins_) {
        if (pin_component_ids.count(pin.component_id) == 0) continue;
        if (!first) pins_json += ',';
        first = false;
        pins_json += pin_json(pin);
    }
    pins_json += ']';
    std::string wires_json = "[";
    first = true;
    for (const auto& wire : wires_) {
        if (seen.count(wire.owner_id) == 0) continue;
        if (!first) wires_json += ',';
        first = false;
        wires_json += wire_json(wire);
    }
    wires_json += ']';

    std::string out;
    JsonObject object(out);
    object.string("sessionId", session_id_);
    object.raw("scopeIds", stringArray(scopes));
    object.raw("pins", pins_json);
    object.raw("wires", wires_json);
    object.end();
    return out;
}

std::string VisualizerSession::stateJson(size_t index) {
    index = clampedIndex(index);
    const auto timestamp = timestamps_[index];
    simulator_->setCircuitStateAtTime(timestamp);
    std::vector<std::string> pin_values;
    std::vector<std::string> wire_values;
    pin_values.reserve(pins_.size());
    wire_values.reserve(wires_.size());
    for (const auto& pin : pins_) pin_values.push_back(pinValue(pin.handle));
    for (const auto& wire : wires_) wire_values.push_back(logicVectorValue(wire.handle->getValueVector()));

    std::string out;
    JsonObject object(out);
    object.string("encoding", "indexed-v1");
    object.number("index", index);
    object.number("time", timestamp);
    object.raw("pins", stringArray(pin_values));
    object.raw("wires", stringArray(wire_values));
    object.end();
    return out;
}

std::shared_ptr<PinBase> VisualizerSession::componentPin(
    const std::string& component_id, const std::string& name,
    const std::string& direction) const {
    if (direction.empty()) {
        const auto input = componentPin(component_id, name, "input");
        const auto output = componentPin(component_id, name, "output");
        if (input && output) {
            throw std::invalid_argument("Ambiguous pin name; specify direction: " + component_id + "." + name);
        }
        return input ? input : output;
    }
    const auto found = pin_index_by_component_direction_name_.find({component_id, direction, name});
    return found == pin_index_by_component_direction_name_.end() ? nullptr : pins_[found->second].handle;
}

std::string VisualizerSession::portsJson(
    const std::string& component_id,
    const std::vector<std::string>& names) const {
    std::string out;
    JsonObject object(out);
    for (const auto& name : names) {
        const auto pin = componentPin(component_id, name);
        if (!pin) continue;
        const auto value = pinValue(pin);
        const auto hex = hexValue(value);
        object.key(name);
        JsonObject port(out);
        port.string("value", value);
        if (hex.empty()) port.null("hex"); else port.string("hex", hex);
        port.end();
    }
    object.end();
    return out;
}

std::string VisualizerSession::componentStateJson(
    size_t index,
    const std::string& component_id, int64_t address, size_t count) {
    const auto found = component_index_by_id_.find(component_id);
    if (found == component_index_by_id_.end()) {
        throw std::invalid_argument("Unknown component id '" + component_id + "'");
    }
    index = clampedIndex(index);
    const auto timestamp = timestamps_[index];
    simulator_->setCircuitStateAtTime(timestamp);
    const auto& component = components_[found->second];

    std::string out;
    JsonObject object(out);
    object.number("index", index);
    object.number("time", timestamp);
    object.string("componentId", component_id);
    object.string("type", component.type);

    if (component.type == "RegisterFile32x32") {
        const auto registers = std::dynamic_pointer_cast<RegisterStateView>(component.handle);
        if (!registers) {
            throw std::runtime_error(
                "RegisterFile32x32 does not expose the register state contract");
        }
        object.boolean("supported", true);
        const auto words = registers->getRegisterStateAtTime(timestamp);
        object.key("registers");
        out += '[';
        for (size_t register_index = 0; register_index < words.size(); ++register_index) {
            if (register_index) out += ',';
            const auto bits = logicVectorValue(words[register_index]);
            const auto hex = hexValue(bits);
            JsonObject item(out);
            item.number("index", register_index);
            item.string("name", "x" + std::to_string(register_index));
            item.string("bits", bits);
            if (hex.empty()) item.null("hex"); else item.string("hex", hex);
            item.boolean("known", !hex.empty());
            item.end();
        }
        out += ']';
        object.raw("ports", portsJson(component_id, {
            "RS1_ADDR", "RS2_ADDR", "RD_ADDR", "WRITE_DATA", "REG_WRITE",
            "CLK", "RST", "RS1_DATA", "RS2_DATA"}));
        object.end();
        return out;
    }

    if (std::dynamic_pointer_cast<MemoryStateView>(component.handle)) {
        auto& inspector = memory_inspectors_[component_id];
        if (!inspector) inspector = std::make_unique<MemoryInspector>(component.handle, *simulator_);
        return inspector->stateJson(index, timestamp, scenario_key_, address, count);
    }

    object.boolean("supported", false);
    object.end();
    return out;
}

std::string VisualizerSession::topologyResponseJson() {
    const auto profile_fingerprint = normalizedProfileFingerprint(root_);
    const auto initial_slice = topologySliceJson({root_->getID()});
    // Extracting the already-built slice members avoids constructing duplicate
    // topology records while preserving the HTTP response's public shape.
    const auto pins_begin = initial_slice.find("\"pins\":");
    const auto wires_begin = initial_slice.find("\"wires\":");
    if (pins_begin == std::string::npos || wires_begin == std::string::npos) {
        throw std::logic_error("Invalid internal topology slice encoding");
    }
    const auto pins_value_begin = pins_begin + 7;
    const auto pins_value_end = wires_begin - 1;
    const auto pins_json = initial_slice.substr(pins_value_begin, pins_value_end - pins_value_begin);
    const auto wires_value_begin = wires_begin + 8;
    const auto wires_value_end = initial_slice.rfind('}');
    const auto wires_json = initial_slice.substr(wires_value_begin, wires_value_end - wires_value_begin);

    std::string components_json = "[";
    for (size_t index = 0; index < components_.size(); ++index) {
        if (index) components_json += ',';
        const auto& record = components_[index];
        const auto& component = record.handle;
        JsonObject item(components_json);
        item.string("id", record.id);
        item.string("name", record.name);
        item.string("type", record.type);
        if (record.has_parent) item.number("parentIndex", record.parent_index); else item.null("parentIndex");
        item.number("depth", record.depth);
        item.decimal("aspectRatio", 0.5);
        item.decimal("minAspectRatio", 0.35);
        item.raw("color", "[61,90,128,190]");
        item.string("implementationId", component->getImplementationId());
        item.string("fidelity", component->getSelectedFidelity());
        item.number("inputPinCount", record.input_pin_count);
        item.number("outputPinCount", record.output_pin_count);
        if (record.layout_type != record.type) item.string("layoutType", record.layout_type);
        const auto available = component->getAvailableFidelities();
        if (!available.empty()) item.raw("availableFidelities", stringArray(available));
        if (component->isProfileSelectable()) item.boolean("profileSelectable", true);
        const auto fingerprint = component->getProfileFingerprint();
        if (!fingerprint.empty() && fingerprint != "explicit") item.string("profileFingerprint", fingerprint);
        const auto constant = constantValueJson(component);
        if (!constant.empty()) item.raw("constantValue", constant);
        const auto rewire = rewireJson(component);
        if (!rewire.empty()) item.raw("rewire", rewire);
        item.end();
    }
    components_json += ']';

    std::vector<SimulationTest::SimulationCheckpoint> checkpoints = test_->getCheckpoints();
    std::sort(checkpoints.begin(), checkpoints.end(), [](const auto& left, const auto& right) {
        if (left.time != right.time) return left.time < right.time;
        if (left.row_index != right.row_index) return left.row_index < right.row_index;
        return left.label < right.label;
    });
    const auto checkpoint_json = arrayJson(checkpoints, [&](const auto& checkpoint) {
        const auto found = std::lower_bound(timestamps_.begin(), timestamps_.end(), checkpoint.time);
        std::string out;
        JsonObject item(out);
        item.number("time", checkpoint.time);
        item.string("label", checkpoint.label);
        item.string("detail", checkpoint.detail);
        item.number("rowIndex", checkpoint.row_index);
        item.number("index", static_cast<size_t>(std::distance(timestamps_.begin(), found)));
        item.end();
        return out;
    });
    const auto metrics = test_->getPerformanceMetrics();
    const auto metrics_json = arrayJson(metrics, [](const auto& metric) {
        std::string out;
        JsonObject item(out);
        item.string("name", metric.name);
        item.decimal("value", metric.value);
        item.string("unit", metric.unit);
        item.string("description", metric.description);
        item.end();
        return out;
    });
    const auto timestamp_json = arrayJson(timestamps_, [](size_t value) {
        return std::to_string(value);
    });
    std::string overrides_json;
    JsonObject overrides(overrides_json);
    for (const auto& [path, fidelity] : exact_overrides_) overrides.string(path, fidelity);
    overrides.end();

    std::string out;
    JsonObject object(out);
    object.string("sessionId", session_id_);
    object.string("scenario", scenario_key_);
    object.string("layoutKey", layout_key_);
    object.string("profileFingerprint", profile_fingerprint);
    object.string("rootId", root_->getID());
    object.string("rootType", root_->getTypeName());
    object.string("topologyEncoding", "progressive-v1");
    object.raw("components", components_json);
    object.raw("pins", pins_json);
    object.raw("wires", wires_json);
    object.raw("loadedScopeIds", stringArray({root_->getID()}));
    object.raw("timestamps", timestamp_json);
    object.raw("checkpoints", checkpoint_json);
    object.raw("performanceMetrics", metrics_json);
    object.raw("state", stateJson(0));
    object.string("stateEncoding", "indexed-v1");
    object.key("profile");
    JsonObject profile(out);
    profile.boolean("editable", profile_editable_);
    profile.number("revision", profile_revision_);
    profile.raw("exactOverrides", overrides_json);
    profile.string("fingerprint", profile_fingerprint);
    profile.end();
    object.key("stats");
    JsonObject stats(out);
    stats.number("componentCount", components_.size());
    stats.number("pinCount", pins_.size());
    stats.number("wireCount", wires_.size());
    stats.number("timestampCount", timestamps_.size());
    stats.end();
    object.end();
    return out;
}

} // namespace circuit::visualizer
