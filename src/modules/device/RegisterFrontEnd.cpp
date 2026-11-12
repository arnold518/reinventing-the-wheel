#include "modules/device/RegisterTable.hpp"
#include "DeviceGroups.hpp"
#include "modules/basic/Decoder.hpp"
#include "modules/memory/MemoryFFController.hpp"
#include "components/ComponentBuilder.tpp"
#include <bit>
#include <set>
#include <stdexcept>

namespace circuit::device {

const char* accessName(RegisterAccess a) {
    switch (a) {
    case RegisterAccess::ReadOnly: return "read-only";
    case RegisterAccess::Setting: return "setting";
    case RegisterAccess::ReadWrite: return "read-write";
    case RegisterAccess::WriteData: return "write-only data";
    case RegisterAccess::Command: return "command";
    }
    return "?";
}

void RegisterTable::validate() const {
    if (registers.empty()) throw std::invalid_argument(device + ": a register table needs at least one register");
    std::set<uint32_t> offsets;
    std::set<std::string> names;
    for (const auto& r : registers) {
        if (r.offset % 4 || r.offset >= 128 || !offsets.insert(r.offset).second)
            throw std::invalid_argument(device + "." + r.name + ": offsets must be unique words below 0x80");
        if (r.name.empty() || !names.insert(r.name).second) throw std::invalid_argument(device + ": register names must be unique");
        if (r.width < 1 || r.width > 32) throw std::invalid_argument(device + "." + r.name + ": width must be 1..32");
        if (r.access == RegisterAccess::Command && r.width != 1) throw std::invalid_argument(device + "." + r.name + ": a command is one bit");
        if (r.width < 32 && (r.value >> r.width)) throw std::invalid_argument(device + "." + r.name + ": value wider than the register");
    }
    if (window_bytes < 128 || !std::has_single_bit(window_bytes)) throw std::invalid_argument(device + ": window must be a power of two of at least 128 bytes");
}

const RegisterEntry* RegisterTable::find(const std::string& name) const {
    for (const auto& r : registers) if (r.name == name) return &r;
    return nullptr;
}

size_t RegisterTable::indexBits() const {
    uint32_t last = 0;
    for (const auto& r : registers) last = std::max(last, r.offset / 4);
    return std::max<size_t>(1, std::bit_width(last));
}

namespace {
using circuit::structural::P;
using circuit::structural::V;
using Minimal = circuit::structural::Logic;

bool readable(RegisterAccess a) { return a == RegisterAccess::ReadOnly || a == RegisterAccess::Setting || a == RegisterAccess::ReadWrite; }
bool writable(RegisterAccess a) { return a == RegisterAccess::ReadWrite || a == RegisterAccess::WriteData || a == RegisterAccess::Command; }

P anyOf(Minimal& l, const V& bits) { return bits.empty() ? l.bit(false) : bits.size() == 1 ? bits.front() : l.any(bits); }

P known(Minimal& l, ComponentBuilder& b, P data) {
    auto p = b.addNewComponent<circuit::memory_detail::RequestKnownBits>(l.id("DATA_KNOWN"), 32);
    l.wire(l.constantWord(3, 0), p->getInputPin<3>("OP"));
    l.wire(l.constantWord(4, 15), p->getInputPin<4>("MASK"));
    l.wire(data, p->getInputPin<32>("ADDR"));
    return p->getOutputPin("FIELDS_KNOWN");
}

// A three-bit 2:1 status choice on word gates (NAME_LOW, NAME_HIGH, NAME);
// the same gates and depth as a scalar mux.
P statusChoice(ComponentBuilder& b, Minimal& l, const std::string& name, P select, P low, P high) {
    return detail::wordChoice(b, l, name + "_", name, select, low, high);
}

// The front end in four groups, as an access flows: DECODE gives one select
// line per register; CHECKS gives the status; WRITES gives the strobes and
// holds the read-write registers; READS chooses the value a read returns.
void buildBody(IOComponent& self, ComponentBuilder& b, const RegisterTable& table, const std::string& suffix) {
    Minimal l(b);
    const auto in = [&](const std::string& n) { return self.getInputPinDynamic(n); };
    const auto out = [&](const std::string& n) { return self.getOutputPinDynamic(n); };
    detail::Fields selects;
    for (const auto& r : table.registers) selects.emplace_back("SELECT_" + r.name, 1);

    // DECODE: the word index on a Decoder, enabled only inside the table's
    // words (no low byte bits, no higher address bits).
    auto decode = detail::group(b, "DECODE", "RegisterDecode" + suffix, {{"ADDRESS", 32}}, selects,
        [table](IOComponent& part, ComponentBuilder& gates) {
            Minimal d(gates);
            const auto n = table.indexBits();
            auto address = d.split(part.getInputPinDynamic("ADDRESS"));
            V outside(address.begin(), address.begin() + 2);
            outside.insert(outside.end(), address.begin() + 2 + n, address.end());
            auto decoder = gates.add(circuit::families::decoder(n), "OFFSET_DECODE");
            d.drive(V(address.begin() + 2, address.begin() + 2 + n), decoder->getInputPinDynamic("ADDR"));
            d.wire(d.inv(d.any(outside)), decoder->getInputPin("ENABLE"));
            for (const auto& r : table.registers)
                d.wire(decoder->getOutputPin("OUT" + std::to_string(r.offset / 4)), part.getOutputPinDynamic("SELECT_" + r.name));
            d.finish();
        });
    l.wire(in("ADDRESS"), decode->getInputPinDynamic("ADDRESS"));

    // CHECKS: a write's payload (known bits, nothing above the width) and the
    // status, in order: not mapped (Range); a read-only register written
    // (ReadOnly); a write-only register read (Unsupported); a write whose
    // payload does not fit (Invalid).
    detail::Fields check_inputs = selects;
    for (const auto& f : detail::Fields{{"IS_WRITE", 1}, {"WRITE_DATA", 32}, {"WRITE_MASK", 4}}) check_inputs.push_back(f);
    auto checks = detail::group(b, "CHECKS", "RegisterChecks" + suffix, check_inputs, {{"STATUS", 3}},
        [table, suffix, selects](IOComponent& part, ComponentBuilder& gates) {
            Minimal c(gates);
            const auto pin = [&](const std::string& n) { return part.getInputPinDynamic(n); };
            // PAYLOAD: does a written value fit its register (known bits,
            // nothing above its width)?
            detail::Fields payload_inputs{{"WRITE_DATA", 32}};
            for (const auto& r : table.registers)
                if (writable(r.access)) payload_inputs.emplace_back("SELECT_" + r.name, 1);
            auto payload = detail::group(gates, "PAYLOAD", "RegisterPayloadCheck" + suffix, payload_inputs, {{"BAD", 1}},
                [table](IOComponent& self, ComponentBuilder& inner) {
                    Minimal p(inner);
                    const auto data_pin = self.getInputPinDynamic("WRITE_DATA");
                    auto data = p.split(data_pin);
                    const auto data_known = known(p, inner, data_pin);
                    V bad;
                    for (const auto& r : table.registers) {
                        if (!writable(r.access)) continue;
                        const auto width = r.access == RegisterAccess::Command ? 1u : r.width;
                        P fits = data_known;
                        if (width < 32) fits = p.land({fits, p.inv(p.any(V(data.begin() + width, data.end())))});
                        bad.push_back(p.land({self.getInputPinDynamic("SELECT_" + r.name), p.inv(fits)}));
                    }
                    p.wire(anyOf(p, bad), self.getOutputPin("BAD"));
                    p.finish();
                });
            // STATUS, in order: not mapped (Range); a read-only register
            // written (ReadOnly); a write-only register read (Unsupported); a
            // write whose payload does not fit (Invalid).
            detail::Fields status_inputs = selects;
            for (const auto& f : detail::Fields{{"IS_WRITE", 1}, {"WRITE_MASK", 4}, {"BAD_PAYLOAD", 1}}) status_inputs.push_back(f);
            auto status_part = detail::group(gates, "STATUS", "RegisterStatus" + suffix, status_inputs, {{"STATUS", 3}},
                [table](IOComponent& self, ComponentBuilder& inner) {
                    Minimal st(inner);
                    const auto in = [&](const std::string& n) { return self.getInputPinDynamic(n); };
                    V read_only, write_only, read_write;
                    for (const auto& r : table.registers)
                        (r.access == RegisterAccess::ReadWrite ? read_write : readable(r.access) ? read_only : write_only).push_back(in("SELECT_" + r.name));
                    V all = read_only;
                    all.insert(all.end(), write_only.begin(), write_only.end());
                    all.insert(all.end(), read_write.begin(), read_write.end());
                    const auto is_write = in("IS_WRITE");
                    // Parts are numbered as they are built, and saved layouts
                    // use those names, so each part is built in its own
                    // statement: C++ leaves the order of a call's arguments to
                    // the compiler, and GCC and Clang differ. This is the
                    // order the native (GCC) build has always used.
                    const auto status_three = st.constantWord(3, 3);
                    const auto status_zero = st.constantWord(3, 0);
                    auto status = statusChoice(inner, st, "READ_ONLY", is_write, status_zero, status_three);
                    const auto unmapped = st.constantWord(3, 2);
                    const auto mapped = anyOf(st, all);
                    status = statusChoice(inner, st, "MAPPED", mapped, unmapped, status);
                    if (!write_only.empty() || !read_write.empty()) {
                        const auto wo = anyOf(st, write_only), rw = anyOf(st, read_write);
                        const auto access_zero = st.constantWord(3, 0);
                        const auto unsupported = st.constantWord(3, 4);
                        const auto access = statusChoice(inner, st, "WRITE_ONLY", is_write, unsupported, access_zero);
                        status = statusChoice(inner, st, "WRITE_ACCESS", wo, status, access);
                        status = statusChoice(inner, st, "READ_WRITE", rw, status, st.constantWord(3, 0));
                        const auto invalid = st.land({in("BAD_PAYLOAD"), st.land({is_write, st.any(st.split(in("WRITE_MASK")))})});
                        status = statusChoice(inner, st, "PAYLOAD", invalid, status, st.constantWord(3, 6));
                    }
                    st.wire(status, self.getOutputPinDynamic("STATUS"));
                    st.finish();
                });
            for (const auto& [name, width] : payload_inputs) c.wire(pin(name), payload->getInputPinDynamic(name));
            for (const auto& [name, width] : status_inputs)
                if (name != "BAD_PAYLOAD") c.wire(pin(name), status_part->getInputPinDynamic(name));
            c.wire(payload->getOutputPin("BAD"), status_part->getInputPin("BAD_PAYLOAD"));
            c.wire(status_part->getOutputPinDynamic("STATUS"), part.getOutputPinDynamic("STATUS"));
            c.finish();
        });
    for (const auto& [name, width] : check_inputs) {
        if (name.starts_with("SELECT_")) l.wire(decode->getOutputPinDynamic(name), checks->getInputPinDynamic(name));
        else l.wire(in(name), checks->getInputPinDynamic(name));
    }
    l.wire(checks->getOutputPinDynamic("STATUS"), out("BACK_STATUS"));
    l.wire(in("READY"), out("BACK_READY"));

    // WRITES: one strobe per writable register (a command acts on bit 0);
    // a read-write register is stored here.
    detail::Fields write_inputs{{"WRITE_STROBE", 1}, {"WRITE_DATA", 32}}, write_outputs;
    bool stores = false;
    for (const auto& r : table.registers) {
        if (!writable(r.access)) continue;
        write_inputs.emplace_back("SELECT_" + r.name, 1);
        write_outputs.emplace_back(r.name + "_WRITE", 1);
        if (r.access == RegisterAccess::ReadWrite) { write_outputs.emplace_back(r.name, r.width); stores = true; }
    }
    if (stores) { write_inputs.emplace_back("CLK", 1); write_inputs.emplace_back("RST", 1); }
    std::shared_ptr<IOComponent> writes;
    if (!write_outputs.empty()) {
        writes = detail::group(b, "WRITES", "RegisterWrites" + suffix, write_inputs, write_outputs,
            [table](IOComponent& part, ComponentBuilder& gates) {
                Minimal w(gates);
                const auto pin = [&](const std::string& n) { return part.getInputPinDynamic(n); };
                auto data = w.split(pin("WRITE_DATA"));
                for (const auto& r : table.registers) {
                    if (!writable(r.access)) continue;
                    auto strobe = w.land({pin("SELECT_" + r.name), pin("WRITE_STROBE")});
                    if (r.access == RegisterAccess::Command) strobe = w.land({strobe, data[0]});
                    w.wire(strobe, part.getOutputPinDynamic(r.name + "_WRITE"));
                    if (r.access == RegisterAccess::ReadWrite) {
                        auto reg = w.reg(r.name + "_STATE", r.width, pin("CLK"), pin("RST"), strobe);
                        w.drive(V(data.begin(), data.begin() + r.width), reg->getInputPinDynamic("D"));
                        w.wire(reg->getOutputPinDynamic("Q"), part.getOutputPinDynamic(r.name));
                    }
                }
                w.finish();
            });
        for (const auto& [name, width] : write_inputs) {
            if (name.starts_with("SELECT_")) l.wire(decode->getOutputPinDynamic(name), writes->getInputPinDynamic(name));
            else l.wire(in(name), writes->getInputPinDynamic(name));
        }
        for (const auto& [name, width] : write_outputs) l.wire(writes->getOutputPinDynamic(name), out(name));
    }

    // READS: zero, then each readable register in table order.
    detail::Fields read_inputs;
    for (const auto& r : table.registers) {
        if (!readable(r.access)) continue;
        read_inputs.emplace_back("SELECT_" + r.name, 1);
        if (r.access == RegisterAccess::ReadOnly) read_inputs.emplace_back(r.name + "_VALUE", r.width);
        if (r.access == RegisterAccess::ReadWrite) read_inputs.emplace_back(r.name, r.width);
    }
    auto reads = detail::group(b, "READS", "RegisterReads" + suffix, read_inputs, {{"DATA", 32}},
        [table](IOComponent& part, ComponentBuilder& gates) {
            Minimal rd(gates);
            const auto pin = [&](const std::string& n) { return part.getInputPinDynamic(n); };
            P value = rd.constantWord(32, 0);
            for (const auto& r : table.registers) {
                if (!readable(r.access)) continue;
                P word;
                if (r.access == RegisterAccess::Setting) word = rd.constantWord(32, r.value);
                else {
                    const auto source = pin(r.access == RegisterAccess::ReadWrite ? r.name : r.name + "_VALUE");
                    if (r.width == 32) word = source;
                    else {
                        auto bits = rd.split(source);
                        while (bits.size() < 32) bits.push_back(rd.bit(false));
                        word = rd.join(bits);
                    }
                }
                auto mux = gates.add(circuit::word::mux(2, 32), "READ_" + r.name);
                rd.wire(pin("SELECT_" + r.name), mux->getInputPin("SEL"));
                rd.wire(value, mux->getInputPinDynamic("A"));
                rd.wire(word, mux->getInputPinDynamic("B"));
                value = mux->getOutputPinDynamic("OUT");
            }
            rd.wire(value, part.getOutputPinDynamic("DATA"));
            rd.finish();
        });
    for (const auto& [name, width] : read_inputs) {
        if (name.starts_with("SELECT_")) l.wire(decode->getOutputPinDynamic(name), reads->getInputPinDynamic(name));
        else if (name.ends_with("_VALUE")) l.wire(in(name), reads->getInputPinDynamic(name));
        else l.wire(writes->getOutputPinDynamic(name), reads->getInputPinDynamic(name));
    }
    l.wire(reads->getOutputPinDynamic("DATA"), out("BACK_READ_DATA"));
    l.finish();
}
} // namespace

std::shared_ptr<IOComponent> buildRegisterFrontEnd(ComponentBuilder& b, const RegisterTable& table) {
    table.validate();
    detail::Fields inputs{{"ADDRESS", 32}, {"IS_WRITE", 1}, {"WRITE_DATA", 32}, {"WRITE_MASK", 4}, {"WRITE_STROBE", 1}, {"READY", 1}};
    detail::Fields outputs{{"BACK_STATUS", 3}, {"BACK_READY", 1}, {"BACK_READ_DATA", 32}};
    bool stored = false;
    std::string type = "RegisterFrontEnd<" + table.device;
    for (const auto& r : table.registers) {
        if (r.access == RegisterAccess::ReadOnly) inputs.emplace_back(r.name + "_VALUE", r.width);
        if (writable(r.access)) outputs.emplace_back(r.name + "_WRITE", 1);
        if (r.access == RegisterAccess::ReadWrite) { outputs.emplace_back(r.name, r.width); stored = true; }
        if (r.access == RegisterAccess::Setting) type += "," + r.name + "=" + std::to_string(r.value);
    }
    if (stored) { inputs.emplace_back("CLK", 1); inputs.emplace_back("RST", 1); }
    const std::string suffix = type.substr(std::string("RegisterFrontEnd").size()) + ">";
    return detail::group(b, "REGISTERS", type + ">", inputs, outputs,
                         [table, suffix](IOComponent& self, ComponentBuilder& gates) { buildBody(self, gates, table, suffix); });
}

} // namespace circuit::device
