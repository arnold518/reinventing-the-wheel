#include "modules/memory/MemoryProtocol.hpp"
#include <algorithm>
#include <cctype>
#include <limits>

namespace circuit::memory {
const char* technologyName(Technology t) {
    switch (t) {
    case Technology::FF: return "ff";
    case Technology::SRAM: return "sram";
    case Technology::DRAM: return "dram";
    case Technology::ROM: return "rom";
    case Technology::NORFlash: return "nor-flash";
    case Technology::Container: return "container";
    }
    throw std::invalid_argument("Unknown memory technology");
}
Technology technologyFromName(const std::string& n) {
    for (auto t : {Technology::FF, Technology::SRAM, Technology::DRAM, Technology::ROM, Technology::NORFlash, Technology::Container})
        if (n == technologyName(t)) return t;
    throw std::invalid_argument("Unknown memory technology: " + n);
}
void EndpointConfig::validate(size_t width, Technology t, bool structural) const {
    if (always_ready && t != Technology::FF)
        throw std::invalid_argument("Always-ready endpoint mode is FF-only");
    if ((width != 8 && width != 16 && width != 32) || !depth_words ||
        depth_words > std::numeric_limits<uint32_t>::max() / (width / 8))
        throw std::invalid_argument("Memory endpoint requires W=8/16/32 and nonzero 32-bit-addressable capacity");
    if (t != Technology::FF && (phase_ticks < 32 || phase_ticks > 10000 || guard_ticks < 64 || guard_ticks > 1000000 ||
        program_ticks < phase_ticks || program_ticks > 1000000 || erase_ticks < program_ticks || erase_ticks > 10000000))
        throw std::invalid_argument("Invalid endpoint timing; guards must allow structural signals to settle");
    if (t != Technology::FF && t != Technology::Container &&
        (!words_per_row || words_per_row > 16 || (words_per_row & (words_per_row - 1)) || depth_words % words_per_row))
        throw std::invalid_argument("Native matrix geometry requires K=1/2/4/8/16 dividing depth");
    if (t == Technology::NORFlash && (!block_words || block_words > depth_words || depth_words % block_words || block_words % words_per_row))
        throw std::invalid_argument("Flash blocks must contain whole rows and divide depth");
    if (t == Technology::DRAM) {
        const uint64_t refresh_service = 5 * phase_ticks + 4 * guard_ticks;
        const uint64_t access_guard = 9 * phase_ticks + 12 * guard_ticks;
        if (retention_ticks < 1024 || retention_ticks > 1000000000 ||
            (refresh_interval_ticks && (refresh_interval_ticks <= refresh_service ||
             uint64_t(depth_words / words_per_row) * (refresh_interval_ticks + access_guard) + access_guard >= retention_ticks)))
            throw std::invalid_argument("DRAM refresh schedule cannot meet the configured retention deadline");
    }
    if (image_hex.size() % 2 || image_hex.size() / 2 > depth_words * (width / 8) ||
        !std::all_of(image_hex.begin(), image_hex.end(), [](unsigned char c) { return std::isxdigit(c); }) ||
        (!image_hex.empty() && t != Technology::ROM && t != Technology::Container))
        throw std::invalid_argument("Only ROM accepts a construction-time hexadecimal image");
    if (!max_resident_bytes || (record_history && !max_history_bytes))
        throw std::invalid_argument("Memory resource budgets must be positive");
    if (structural && expanded_storage && depth_words > 4096 / width)
        throw std::invalid_argument("Expanded endpoint storage is limited to 4096 cells");
}
EndpointConfig endpointConfig(const ParameterMap& p) {
    using namespace memory_device;
    keys(p, {"depth_words", "words_per_row", "block_words", "phase_ticks", "guard_ticks",
             "program_ticks", "erase_ticks", "retention_ticks", "refresh_interval_ticks",
             "max_resident_bytes", "max_history_bytes", "paged", "record_history", "image_hex", "expanded_storage", "always_ready"});
    EndpointConfig c;
#define PARAM(n) c.n = parameter(p, #n, c.n)
    PARAM(depth_words); PARAM(words_per_row); PARAM(block_words); PARAM(phase_ticks); PARAM(guard_ticks);
    PARAM(program_ticks); PARAM(erase_ticks); PARAM(retention_ticks); PARAM(refresh_interval_ticks);
    PARAM(max_resident_bytes); PARAM(max_history_bytes);
#undef PARAM
    auto paged = parameter(p, "paged", 1), history = parameter(p, "record_history", 1);
    auto expanded = parameter(p, "expanded_storage", 1), always_ready = parameter(p,"always_ready",0);
    if (paged > 1 || history > 1 || expanded > 1 || always_ready > 1) throw std::invalid_argument("Memory booleans must be 0 or 1");
    c.paged = paged; c.record_history = history;
    c.expanded_storage = expanded;
    c.always_ready = always_ready;
    if (auto i = p.find("image_hex"); i != p.end()) c.image_hex = i->second;
    return c;
}
ParameterMap endpointParameters(const EndpointConfig& c) {
    ParameterMap p;
#define PARAM(n) p[#n] = std::to_string(c.n)
    PARAM(depth_words); PARAM(words_per_row); PARAM(block_words); PARAM(phase_ticks); PARAM(guard_ticks);
    PARAM(program_ticks); PARAM(erase_ticks); PARAM(retention_ticks); PARAM(refresh_interval_ticks);
    PARAM(max_resident_bytes); PARAM(max_history_bytes); PARAM(paged); PARAM(record_history);
    PARAM(expanded_storage);
    PARAM(always_ready);
#undef PARAM
    p["image_hex"] = c.image_hex; return p;
}
void protocolPins(IOComponent* s, size_t w, const std::string& prefix, bool requester) {
    const auto input = requester ? PinType::OUTPUT : PinType::INPUT;
    const auto output = requester ? PinType::INPUT : PinType::OUTPUT;
    auto add = [&](const char* n, size_t width, PinType type) { s->addPinDynamic(prefix + n, type, width); };
    for (auto n : {"CLK", "RST", "REQ_VALID", "RSP_READY"}) add(n, 1, input);
    add("REQ_OP", 3, input); add("REQ_ADDR", 32, input);
    add("REQ_WRITE_DATA", w, input); add("REQ_WRITE_MASK", w / 8, input);
    add("REQ_READY", 1, output); add("RSP_VALID", 1, output);
    add("RSP_STATUS", 3, output); add("RSP_READ_DATA", w, output);
}
void nativePins(IOComponent* s, size_t w, Technology t, bool controller) {
    if (t == Technology::Container) { protocolPins(s, w, "MEM_", true); return; }
    auto command = controller ? PinType::OUTPUT : PinType::INPUT;
    auto result = controller ? PinType::INPUT : PinType::OUTPUT;
    auto add = [&](const char* n, size_t width, PinType d) { s->addPinDynamic(std::string(controller ? "MEM_" : "") + n, d, width); };
    add(t == Technology::DRAM ? "ADDR" : "WORD_ADDR", 32, command);
    add("READ_DATA", w, result);
    if (t != Technology::ROM) { add("WRITE_DATA", w, command); add("WRITE_MASK", w / 8, command); }
    if (t == Technology::FF) {
        for (auto n : {"CLK", "RST", "WE", "READ_EN"}) add(n, 1, command);
        add("FAULT", 1, result);
    } else if (t == Technology::SRAM || t == Technology::ROM) {
        add("CE_N", 1, command); add("OE_N", 1, command);
        if (t == Technology::SRAM) add("WE_N", 1, command);
    } else {
        add("CLK", 1, command); add("CMD", 3, command);
        for (auto n : {"BUSY", "ERROR", "READ_VALID"}) add(n, 1, result);
    }
}
}
