#include "modules/accelerator/MatrixUnit.hpp"
#include "modules/accelerator/MatrixParts.hpp"
#include "modules/device/DeviceComponents.hpp"
#include "modules/device/RegisterTable.hpp"
#include "modules/memory/MemoryProtocol.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/selection/ComponentCatalog.hpp"
#include "components/selection/FamilyRegistration.hpp"
#include "modules/device/DeviceGroups.hpp"
#include <bit>
#include <stdexcept>

using namespace circuit::device::detail;

namespace {
// Register-transfer helpers over the structural Logic builder: every call
// builds gates, adders, muxes or registers; nothing here evaluates. Parts are
// numbered in the order they are built, and C++ leaves the order of a call's
// arguments to the compiler, so a call never gets two arguments that build
// parts: they are built into named values first, so GCC and Clang (native and
// WebAssembly) name every part alike.
struct Rtl {
    Logic& l;
    P clk, rst;
    struct Reg { std::shared_ptr<IOComponent> part; V q; };
    V bits(P p) { return l.split(p); }
    V slice(const V& v, size_t first, size_t count) { return V(v.begin() + first, v.begin() + first + count); }
    V zext(V v, size_t width) { while (v.size() < width) v.push_back(l.bit(false)); v.resize(width); return v; }
    V constant(size_t width, uint64_t value) { return l.constant(width, value); }
    V add(const V& a, const V& b) { return l.add({a, b, l.bit(false)}).first; }
    V inc(const V& a) { return l.add({a, constant(a.size(), 0), l.bit(true)}).first; }
    V mux(P select, const V& low, const V& high) { return l.mux({select, low, high}); }
    P pick(P select, P low, P high) { return l.choose({select, low, high}); }
    P eq(const V& a, const V& b) { return l.equal({a, b}); }
    P eqConst(const V& a, uint64_t value) { return l.equal({a, constant(a.size(), value)}); }
    P zero(const V& a) { return l.inv(l.any(a)); }
    P land(P a, P b) { return l.land({a, b}); }
    P lor(P a, P b) { return l.lor({a, b}); }
    P inv(P a) { return l.inv(a); }
    Reg reg(const std::string& name, size_t width, P enable) {
        auto r = l.reg(name, width, clk, rst, enable);
        auto q = r->getOutputPinDynamic("Q");
        return {r, width == 1 ? V{q} : l.split(q)};
    }
    void d(const Reg& r, const V& v) { l.drive(v, r.part->getInputPinDynamic("D")); }
    void d(const Reg& r, P bit) { l.wire(bit, r.part->getInputPinDynamic("D")); }
};

// A catalog part by contract ID; a compact device requires compact children.
std::shared_ptr<IOComponent> contractPart(IOComponent& root, ComponentBuilder& b, const std::string& contract,
                                          const std::string& name, circuit::ParameterMap parameters) {
    circuit::ComponentBuildRequest request{contract, name, std::move(parameters)};
    if (root.getSelectedFidelity() == "behavioral") request.required_fidelity = circuit::Fidelity::Behavioral;
    return b.addByContract(std::move(request));
}

void unitPins(IOComponent* p) {
    circuit::memory::protocolPins(p, 32);
    circuit::memory::protocolPins(p, 32, "DMA_", true);
}

// The command sequencer: takes a command from the queue when idle, starts its
// engine, waits for that engine's DONE, counts finished commands and keeps
// the error flag.
void sequencer(IOComponent& self, ComponentBuilder& gates) {
    Logic l(gates);
    Rtl r{l, input(self, "CLK"), input(self, "RST")};
    const auto one = l.bit(true);
    auto copy = r.reg("COPY_ACTIVE", 1, one), shift = r.reg("SHIFT_ACTIVE", 1, one);
    auto mm = r.reg("MATMUL_ACTIVE", 1, one), act = r.reg("ACTIVATE_ACTIVE", 1, one);
    const auto idle = l.inv(l.any({copy.q[0], shift.q[0], mm.q[0], act.q[0]}));
    const auto take = r.land(idle, input(self, "Q_VALID"));
    const auto op = r.slice(r.bits(input(self, "Q_OP")), 0, 8);
    const auto is_load = r.eqConst(op, 1), is_store = r.eqConst(op, 2), is_shift = r.eqConst(op, 3);
    const auto is_mm = r.eqConst(op, 4), is_act = r.eqConst(op, 5);
    const auto known = l.any({is_load, is_store, is_shift, is_mm, is_act});
    const auto start_copy = r.land(take, r.lor(is_load, is_store));
    const auto start_shift = r.land(take, is_shift), start_mm = r.land(take, is_mm), start_act = r.land(take, is_act);
    const auto invalid = r.land(take, r.inv(known));
    const auto copy_done = r.land(copy.q[0], input(self, "COPY_DONE"));
    const auto shift_done = r.land(shift.q[0], input(self, "SHIFT_DONE"));
    const auto mm_done = r.land(mm.q[0], input(self, "MATMUL_DONE"));
    const auto act_done = r.land(act.q[0], input(self, "ACTIVATE_DONE"));
    r.d(copy, r.lor(start_copy, r.land(copy.q[0], r.inv(copy_done))));
    r.d(shift, r.lor(start_shift, r.land(shift.q[0], r.inv(shift_done))));
    r.d(mm, r.lor(start_mm, r.land(mm.q[0], r.inv(mm_done))));
    r.d(act, r.lor(start_act, r.land(act.q[0], r.inv(act_done))));
    const auto finished = l.any({copy_done, shift_done, mm_done, act_done, invalid});
    auto count = r.reg("DONE_COUNT", 32, finished);
    r.d(count, r.inc(count.q));
    const auto failed = l.any({invalid, r.land(copy_done, input(self, "COPY_FAILED"))});
    auto error = r.reg("ERROR", 1, one);
    r.d(error, r.lor(failed, r.land(error.q[0], r.inv(input(self, "CLEAR")))));
    for (auto [name, pin] : std::vector<std::pair<const char*, P>>{
             {"POP", take}, {"START_COPY", start_copy}, {"STORE_OP", is_store}, {"START_SHIFT", start_shift},
             {"START_MATMUL", start_mm}, {"START_ACTIVATE", start_act}, {"COPY_ACTIVE", copy.q[0]},
             {"SHIFT_ACTIVE", shift.q[0]}, {"MATMUL_ACTIVE", mm.q[0]}, {"ACTIVATE_ACTIVE", act.q[0]},
})
        l.wire(pin, output(self, name));
    // STATUS: bit 0 busy, bit 1 error, bit 2 queue full.
    l.drive({r.lor(r.inv(idle), input(self, "Q_VALID")), error.q[0], r.inv(input(self, "Q_READY"))}, output(self, "STATUS"));
    l.drive(count.q, output(self, "DONE_COUNT"));
    l.finish();
}

// LOAD and STORE: a byte copy between main memory (through the DMA master
// port) and the device's buffers, over P patches × R rows × B bytes:
//   memory address = ARG0 + p·T + r·S + c
//   local byte     = ARG1[15:0]·N + p·DR·N + r·B + c
// ARG1 = P << 16 | row; ARG2 = R << 24 | B << 16 | S; ARG3 = target << 30 | DR << 16 | T.
// A LOAD reads each word once while consecutive bytes stay in it.
void copyEngine(IOComponent& self, ComponentBuilder& gates, size_t lg) {
    Logic l(gates);
    Rtl r{l, input(self, "CLK"), input(self, "RST")};
    const auto one = l.bit(true);
    const auto start = input(self, "START");
    const auto a0 = r.bits(input(self, "ARG0")), a1 = r.bits(input(self, "ARG1"));
    const auto a2 = r.bits(input(self, "ARG2")), a3 = r.bits(input(self, "ARG3"));
    auto store = r.reg("STORE", 1, start); r.d(store, input(self, "STORE_OP"));
    auto target = r.reg("TARGET", 2, start); r.d(target, r.slice(a3, 30, 2));
    auto patches = r.reg("PATCHES", 16, start); r.d(patches, r.slice(a1, 16, 16));
    auto rows = r.reg("ROWS", 8, start); r.d(rows, r.slice(a2, 24, 8));
    auto bytes = r.reg("BYTES", 8, start); r.d(bytes, r.slice(a2, 16, 8));
    auto row_stride = r.reg("ROW_STRIDE", 16, start); r.d(row_stride, r.slice(a2, 0, 16));
    auto patch_stride = r.reg("PATCH_STRIDE", 16, start); r.d(patch_stride, r.slice(a3, 0, 16));
    auto patch_rows = r.reg("PATCH_ROWS", 8, start); r.d(patch_rows, r.slice(a3, 16, 8));
    auto empty = r.reg("EMPTY", 1, start);
    r.d(empty, l.any({r.zero(r.slice(a1, 16, 16)), r.zero(r.slice(a2, 24, 8)), r.zero(r.slice(a2, 16, 8))}));
    auto active = r.reg("ACTIVE", 1, one);
    auto wait = r.reg("WAIT", 1, one);
    const bool unused = false; (void)unused;

    // Per-step state.
    const auto store_q = store.q[0], load_q = r.inv(store.q[0]);
    const auto running = r.land(active.q[0], r.inv(empty.q[0]));
    auto ram = r.reg("ADDRESS", 32, one), ram_row = r.reg("ROW_ADDRESS", 32, one), ram_patch = r.reg("PATCH_ADDRESS", 32, one);
    auto local = r.reg("LOCAL", 16, one), local_patch = r.reg("LOCAL_PATCH", 16, one);
    auto c = r.reg("BYTE_COUNT", 8, one), rr = r.reg("ROW_COUNT", 8, one), p = r.reg("PATCH_COUNT", 16, one);
    auto cache_valid = r.reg("CACHE_VALID", 1, one);
    // The cached word: filled from a read answer.
    const auto rsp_valid = input(self, "RSP_VALID");
    const auto ok = r.zero(r.bits(input(self, "RSP_STATUS")));
    const auto answer = r.land(wait.q[0], rsp_valid);
    const auto fill = l.all({answer, ok, load_q});
    auto cache_tag = r.reg("CACHE_TAG", 32, fill), cache_data = r.reg("CACHE_DATA", 32, fill);
    r.d(cache_tag, r.zext(r.slice(ram.q, 2, 30), 32));
    r.d(cache_data, r.bits(input(self, "RSP_DATA")));
    const auto hit = r.land(cache_valid.q[0], r.eq(r.slice(cache_tag.q, 0, 30), r.slice(ram.q, 2, 30)));
    const auto failed = r.land(answer, r.inv(ok));
    const auto load_step = r.land(load_q, hit);
    const auto store_answer = r.land(answer, ok);
    const auto store_step = r.land(store_q, store_answer);
    const auto step = r.land(running, r.lor(load_step, store_step));
    const auto request = l.all({running, r.inv(wait.q[0]), r.lor(store_q, r.inv(hit))});
    const auto accepted = r.land(request, input(self, "REQ_READY"));
    const auto not_start = r.inv(start);
    const auto no_answer = r.inv(rsp_valid);
    const auto still_waiting = r.land(wait.q[0], no_answer);
    const auto waiting = r.lor(accepted, still_waiting);
    r.d(wait, r.land(not_start, waiting));
    const auto filled = r.lor(fill, cache_valid.q[0]);
    r.d(cache_valid, r.land(not_start, filled));

    // Address and counter updates.
    const auto c1 = r.inc(c.q), r1 = r.inc(rr.q), p1 = r.inc(p.q);
    const auto c_last = r.eq(c1, bytes.q), r_last = r.eq(r1, rows.q), p_last = r.eq(p1, patches.q);
    const auto last = l.all({c_last, r_last, p_last});
    const auto next_patch = r.land(c_last, r_last);
    const auto row_plus = r.add(ram_row.q, r.zext(row_stride.q, 32));
    const auto patch_plus = r.add(ram_patch.q, r.zext(patch_stride.q, 32));
    V shifted_rows(lg, l.bit(false));
    shifted_rows.insert(shifted_rows.end(), patch_rows.q.begin(), patch_rows.q.end());
    const auto local_patch_plus = r.add(local_patch.q, r.zext(shifted_rows, 16));
    const auto ram_plus = r.inc(ram.q);
    const auto ram_jump = r.mux(r_last, row_plus, patch_plus);
    const auto ram_next = r.mux(c_last, ram_plus, ram_jump);
    const auto ram_row_next = r.mux(c_last, ram_row.q, r.mux(r_last, row_plus, patch_plus));
    const auto ram_patch_next = r.mux(next_patch, ram_patch.q, patch_plus);
    const auto local_next = r.mux(next_patch, r.inc(local.q), local_patch_plus);
    const auto local_patch_next = r.mux(next_patch, local_patch.q, local_patch_plus);
    const auto c_next = r.mux(c_last, c1, r.constant(8, 0));
    const auto r_next = r.mux(c_last, rr.q, r.mux(r_last, r1, r.constant(8, 0)));
    const auto p_next = r.mux(next_patch, p.q, p1);
    // START loads the first addresses; a step loads the next ones.
    V row_byte(lg, l.bit(false));
    const auto first_row = r.slice(a1, 0, 16);
    row_byte.insert(row_byte.end(), first_row.begin(), first_row.end());
    const auto local_first = r.zext(row_byte, 16);
    const auto hold = [&](const Rtl::Reg& reg, const V& next, const V& first) {
        r.d(reg, r.mux(start, r.mux(step, reg.q, next), first));
    };
    hold(ram, ram_next, a0); hold(ram_row, ram_row_next, a0); hold(ram_patch, ram_patch_next, a0);
    hold(local, local_next, local_first); hold(local_patch, local_patch_next, local_first);
    hold(c, c_next, r.constant(8, 0)); hold(rr, r_next, r.constant(8, 0)); hold(p, p_next, r.constant(16, 0));
    const auto finish = r.land(active.q[0], l.any({empty.q[0], r.land(step, last), failed}));
    r.d(active, r.lor(start, r.land(active.q[0], r.inv(finish))));

    // The bus request: a word read, or a byte write with its lane's mask.
    const auto lane = r.slice(ram.q, 0, 2);
    const auto byte = r.bits(input(self, "LOCAL_READ"));
    V data;
    for (int k = 0; k < 4; ++k) data.insert(data.end(), byte.begin(), byte.end());
    V mask;
    for (uint64_t k = 0; k < 4; ++k) mask.push_back(r.eqConst(lane, k));
    V word_address = ram.q;
    word_address[0] = word_address[1] = l.bit(false);
    const auto cached = cache_data.q;
    const auto low = r.mux(lane[0], r.slice(cached, 0, 8), r.slice(cached, 8, 8));
    const auto high = r.mux(lane[0], r.slice(cached, 16, 8), r.slice(cached, 24, 8));
    const auto loaded = r.mux(lane[1], low, high);
    l.wire(request, output(self, "REQ_VALID"));
    l.wire(store_q, output(self, "REQ_WRITE"));
    l.drive(word_address, output(self, "REQ_ADDR"));
    l.drive(data, output(self, "REQ_DATA"));
    l.drive(mask, output(self, "REQ_MASK"));
    l.wire(wait.q[0], output(self, "RSP_READY"));
    l.drive(local.q, output(self, "LOCAL_ADDR"));
    l.wire(l.all({running, load_q, hit}), output(self, "LOCAL_WRITE"));
    l.drive(loaded, output(self, "LOCAL_DATA"));
    l.drive(target.q, output(self, "TARGET"));
    l.wire(store_q, output(self, "STORING"));
    l.wire(finish, output(self, "DONE"));
    l.wire(failed, output(self, "FAILED"));
    l.finish();
}

// SHIFT_WEIGHTS: N edges with the array's SHIFT high; at the k-th edge the
// top row receives weight-buffer row N-1-k.
void shiftEngine(IOComponent& self, ComponentBuilder& gates, size_t lg) {
    Logic l(gates);
    Rtl r{l, input(self, "CLK"), input(self, "RST")};
    const auto start = input(self, "START");
    auto active = r.reg("ACTIVE", 1, l.bit(true));
    auto k = r.reg("STEP", 8, r.lor(start, active.q[0]));
    const auto k_next = r.inc(k.q);
    const auto k_first = r.constant(8, 0);
    r.d(k, r.mux(start, k_next, k_first));
    const auto done = r.land(active.q[0], r.eqConst(r.slice(k.q, 0, lg), (uint64_t{1} << lg) - 1));
    r.d(active, r.lor(start, r.land(active.q[0], r.inv(done))));
    l.wire(active.q[0], output(self, "SHIFT"));
    l.drive(r.zext(l.inverted(r.slice(k.q, 0, lg)), 16), output(self, "ROW"));
    l.wire(done, output(self, "DONE"));
    l.finish();
}

// MATMUL: P activation rows, ARG0 = P << 16 | first row, ARG1 = accumulator
// row << 16 | row stride, ARG2 bit 0 = add to the accumulators (else start
// from the bias). One row enters per edge; its result is written 2N-1 edges later.
void matmulEngine(IOComponent& self, ComponentBuilder& gates, size_t n) {
    Logic l(gates);
    Rtl r{l, input(self, "CLK"), input(self, "RST")};
    const auto one = l.bit(true);
    const auto start = input(self, "START");
    const auto a0 = r.bits(input(self, "ARG0")), a1 = r.bits(input(self, "ARG1")), a2 = r.bits(input(self, "ARG2"));
    auto total = r.reg("ROWS", 16, start); r.d(total, r.slice(a0, 16, 16));
    auto stride = r.reg("STRIDE", 16, start); r.d(stride, r.slice(a1, 0, 16));
    auto accumulate = r.reg("ACCUMULATE", 1, start); r.d(accumulate, a2[0]);
    auto issuing = r.reg("ISSUING", 1, one);
    auto count = r.reg("ISSUED", 16, r.lor(start, issuing.q[0]));
    auto row = r.reg("ROW", 16, r.lor(start, issuing.q[0]));
    const auto last = r.eq(r.inc(count.q), total.q);
    const auto not_last = r.inv(last);
    const auto continuing = r.land(issuing.q[0], not_last);
    const auto any_rows = r.inv(r.zero(r.slice(a0, 16, 16)));
    r.d(issuing, r.pick(start, continuing, any_rows));
    const auto count_next = r.inc(count.q);
    const auto count_first = r.constant(16, 0);
    r.d(count, r.mux(start, count_next, count_first));
    r.d(row, r.mux(start, r.add(row.q, stride.q), r.slice(a0, 0, 16)));
    // One valid bit per edge of the array's latency, plus the write edge.
    const size_t depth = circuit::accelerator::systolicLatency(n) + 1;
    V valid;
    P previous = issuing.q[0];
    for (size_t k = 0; k < depth; ++k) {
        auto v = r.reg("VALID_" + std::to_string(k), 1, one);
        r.d(v, previous);
        previous = v.q[0];
        valid.push_back(previous);
    }
    const auto write = valid.back();
    auto wb = r.reg("WRITE_ROW", 16, r.lor(start, write));
    r.d(wb, r.mux(start, r.inc(wb.q), r.slice(a1, 16, 16)));
    l.wire(issuing.q[0], output(self, "ISSUE"));
    l.drive(row.q, output(self, "ROW"));
    l.wire(write, output(self, "WRITE"));
    l.drive(wb.q, output(self, "WRITE_ROW"));
    l.wire(accumulate.q[0], output(self, "ACCUMULATE"));
    const auto not_issuing = r.inv(issuing.q[0]);
    const auto none_in_flight = r.inv(l.any(valid));
    l.wire(r.land(not_issuing, none_in_flight), output(self, "DONE"));
    l.finish();
}

// ACTIVATE: P rows; ARG0 = P << 16 | accumulator row, ARG1 = activation row,
// ARG2 = RELU << 24 | SHIFT << 16 | SCALE. One row per edge.
void activateEngine(IOComponent& self, ComponentBuilder& gates) {
    Logic l(gates);
    Rtl r{l, input(self, "CLK"), input(self, "RST")};
    const auto one = l.bit(true);
    const auto start = input(self, "START");
    const auto a0 = r.bits(input(self, "ARG0")), a1 = r.bits(input(self, "ARG1")), a2 = r.bits(input(self, "ARG2"));
    auto total = r.reg("ROWS", 16, start); r.d(total, r.slice(a0, 16, 16));
    auto scale = r.reg("SCALE", 16, start); r.d(scale, r.slice(a2, 0, 16));
    auto shift = r.reg("SHIFT", 5, start); r.d(shift, r.slice(a2, 16, 5));
    auto relu = r.reg("RELU", 1, start); r.d(relu, a2[24]);
    auto running = r.reg("RUNNING", 1, one);
    const auto advance = r.lor(start, running.q[0]);
    auto count = r.reg("COUNT", 16, advance), src = r.reg("ACC_ROW", 16, advance), dst = r.reg("ACT_ROW", 16, advance);
    const auto last = r.eq(r.inc(count.q), total.q);
    const auto not_last = r.inv(last);
    const auto continuing = r.land(running.q[0], not_last);
    const auto any_rows = r.inv(r.zero(r.slice(a0, 16, 16)));
    r.d(running, r.pick(start, continuing, any_rows));
    const auto count_next = r.inc(count.q);
    const auto count_first = r.constant(16, 0);
    r.d(count, r.mux(start, count_next, count_first));
    r.d(src, r.mux(start, r.inc(src.q), r.slice(a0, 0, 16)));
    r.d(dst, r.mux(start, r.inc(dst.q), r.slice(a1, 0, 16)));
    l.wire(running.q[0], output(self, "RUN"));
    l.drive(src.q, output(self, "ACC_ROW"));
    l.drive(dst.q, output(self, "ACT_ROW"));
    l.drive(scale.q, output(self, "SCALE"));
    l.drive(shift.q, output(self, "SHIFT"));
    l.wire(relu.q[0], output(self, "RELU"));
    l.wire(r.inv(running.q[0]), output(self, "DONE"));
    l.finish();
}

// Which engine owns each buffer's address and write enables this cycle.
void bufferPorts(IOComponent& self, ComponentBuilder& gates, size_t n, size_t lg) {
    Logic l(gates);
    Rtl r{l, nullptr, nullptr};
    const auto local = r.bits(input(self, "LOCAL_ADDR"));
    const auto bank = r.slice(local, 0, lg);
    const auto row = r.zext(r.slice(local, lg, 16 - lg), 16);
    const auto target = r.bits(input(self, "TARGET"));
    const auto loading = l.all({input(self, "COPY_ACTIVE"), r.inv(input(self, "STORING")), input(self, "LOCAL_WRITE")});
    const auto to_act = r.land(loading, r.eqConst(target, 0));
    const auto to_weight = r.land(loading, r.eqConst(target, 1));
    const auto to_bias = r.land(loading, r.eqConst(target, 2));
    V act_we, weight_we, bias_we;
    for (size_t j = 0; j < n; ++j) {
        const auto here = r.eqConst(bank, j);
        act_we.push_back(r.lor(r.land(to_act, here), input(self, "ACTIVATE_RUN")));
        weight_we.push_back(r.land(to_weight, here));
    }
    const auto column = r.slice(local, 2, lg), byte_lane = r.slice(local, 0, 2);
    for (size_t c = 0; c < n; ++c)
        for (uint64_t k = 0; k < 4; ++k)
            bias_we.push_back(l.all({to_bias, r.eqConst(column, c), r.eqConst(byte_lane, k)}));
    const auto activate_act_row = r.bits(input(self, "ACTIVATE_ACT_ROW"));
    const auto matmul_row = r.bits(input(self, "MATMUL_ROW"));
    const auto engine_act_row = r.mux(input(self, "MATMUL_ACTIVE"), activate_act_row, matmul_row);
    const auto act_addr = r.mux(input(self, "COPY_ACTIVE"), engine_act_row, row);
    const auto activate_acc_row = r.bits(input(self, "ACTIVATE_ACC_ROW"));
    const auto matmul_write_row = r.bits(input(self, "MATMUL_WRITE_ROW"));
    const auto acc_addr = r.mux(input(self, "MATMUL_ACTIVE"), activate_acc_row, matmul_write_row);
    const auto weight_addr = r.mux(input(self, "SHIFT_ACTIVE"), row, r.bits(input(self, "SHIFT_ROW")));
    l.drive(r.zext(act_addr, 32), output(self, "ACT_ADDR"));
    l.drive(r.zext(acc_addr, 32), output(self, "ACC_ADDR"));
    l.drive(r.zext(weight_addr, 32), output(self, "WEIGHT_ADDR"));
    l.drive(act_we, output(self, "ACT_WE"));
    l.drive(weight_we, output(self, "WEIGHT_WE"));
    l.drive(bias_we, output(self, "BIAS_WE"));
    l.finish();
}

// One column: the array input from activation bank j, the accumulator update
// for array output j, and the value written to activation bank j.
void lane(IOComponent& self, ComponentBuilder& gates) {
    Logic l(gates);
    Rtl r{l, nullptr, nullptr};
    l.drive(l.mask(r.bits(input(self, "ACT_READ")), input(self, "ISSUE")), output(self, "X"));
    const auto bias = r.bits(input(self, "BIAS"));
    const auto old = r.bits(input(self, "ACC_READ"));
    const auto base = r.mux(input(self, "ACCUMULATE"), bias, old);
    const auto y = r.bits(input(self, "Y"));
    l.drive(r.add(base, y), output(self, "ACC_WRITE"));
    const auto copy_byte = r.bits(input(self, "COPY_BYTE"));
    const auto requantized = r.bits(input(self, "REQUANTIZED"));
    l.drive(r.mux(input(self, "ACTIVATE_RUN"), copy_byte, requantized), output(self, "ACT_WRITE"));
    l.finish();
}
} // namespace

namespace circuit::accelerator {

MatrixUnit::MatrixUnit(std::string name, size_t size)
    : IOComponent(std::move(name), unitPins), size_(size), type_("MatrixUnit<" + std::to_string(size) + ">") {
    if (size != 4 && size != 8 && size != 16) throw std::invalid_argument("MatrixUnit size must be 4, 8 or 16");
}

void MatrixUnit::buildInternals(ComponentBuilder& b) {
    Logic l(b);
    const size_t n = size_, lg = std::countr_zero(n);
    const auto context = b.getBuildContext();
    const uint32_t act_rows = parameter(context, "act_rows", 256), acc_rows = parameter(context, "acc_rows", 64);
    const uint32_t queue_depth = parameter(context, "queue_depth", 4);
    const auto clk = getInputPin("CLK"), rst = getInputPin("RST");
    const auto contract = "device.matrix-unit." + std::to_string(n);
    circuit::ParameterMap table_parameters{{"act_rows", std::to_string(act_rows)}, {"acc_rows", std::to_string(acc_rows)}};

    // Register front end and command queue.
    auto port = devicePort(*this, b, l);
    auto registers = circuit::device::buildRegisterFrontEnd(b, circuit::device::deviceRegisterTable(contract, table_parameters));
    frontEnd(l, port, registers);
    inputs(l, *this, registers, {"CLK", "RST"});
    const circuit::ParameterMap fifo{{"depth", std::to_string(queue_depth)}};
    auto queue_op = devicePart(*this, b, circuit::families::synchronousFifo(8), "QUEUE_OP", fifo);
    std::vector<std::shared_ptr<IOComponent>> queue_args;
    for (int k = 0; k < 4; ++k) queue_args.push_back(devicePart(*this, b, circuit::families::synchronousFifo(32), "QUEUE_ARG" + std::to_string(k), fifo));
    auto seq = group(b, "SEQUENCER", "MatrixSequencer",
        {{"CLK", 1}, {"RST", 1}, {"Q_VALID", 1}, {"Q_OP", 8}, {"CLEAR", 1}, {"COPY_DONE", 1}, {"COPY_FAILED", 1},
         {"SHIFT_DONE", 1}, {"MATMUL_DONE", 1}, {"ACTIVATE_DONE", 1}, {"Q_READY", 1}},
        {{"POP", 1}, {"START_COPY", 1}, {"STORE_OP", 1}, {"START_SHIFT", 1}, {"START_MATMUL", 1}, {"START_ACTIVATE", 1},
         {"COPY_ACTIVE", 1}, {"SHIFT_ACTIVE", 1}, {"MATMUL_ACTIVE", 1}, {"ACTIVATE_ACTIVE", 1}, {"STATUS", 3},
         {"DONE_COUNT", 32}},
        sequencer);
    const auto port_data = l.split(port->getOutputPin<32>("WRITE_DATA"));
    for (auto& q : std::vector<std::shared_ptr<IOComponent>>{queue_op, queue_args[0], queue_args[1], queue_args[2], queue_args[3]}) {
        inputs(l, *this, q, {"CLK", "RST"});
        l.wire(registers->getOutputPin("COMMAND_WRITE"), q->getInputPin("PUSH"));
        l.wire(seq->getOutputPin("POP"), q->getInputPin("POP"));
        l.wire(l.bit(false), q->getInputPin("CLEAR_OVERFLOW"));
    }
    l.drive(V(port_data.begin(), port_data.begin() + 8), queue_op->getInputPin<8>("PUSH_DATA"));
    for (int k = 0; k < 4; ++k)
        l.wire(registers->getOutputPin<32>("ARG" + std::to_string(k)), queue_args[k]->getInputPin<32>("PUSH_DATA"));
    l.wire(queue_op->getOutputPin("PUSH_READY"), registers->getInputPin("READY"));
    l.wire(queue_op->getOutputPin("PUSH_READY"), seq->getInputPin("Q_READY"));
    l.wire(seq->getOutputPin<3>("STATUS"), registers->getInputPin<3>("STATUS_VALUE"));
    l.wire(seq->getOutputPin<32>("DONE_COUNT"), registers->getInputPin<32>("DONE_VALUE"));
    inputs(l, *this, seq, {"CLK", "RST"});
    l.wire(queue_op->getOutputPin("VALID"), seq->getInputPin("Q_VALID"));
    l.wire(queue_op->getOutputPin<8>("DATA"), seq->getInputPin<8>("Q_OP"));
    l.wire(registers->getOutputPin("CLEAR_WRITE"), seq->getInputPin("CLEAR"));
    const auto arg = [&](int k) { return queue_args[k]->getOutputPin<32>("DATA"); };

    // Engines.
    auto copy = group(b, "COPY_ENGINE", "MatrixCopyEngine<" + std::to_string(n) + ">",
        {{"CLK", 1}, {"RST", 1}, {"START", 1}, {"STORE_OP", 1}, {"ARG0", 32}, {"ARG1", 32}, {"ARG2", 32}, {"ARG3", 32},
         {"REQ_READY", 1}, {"RSP_VALID", 1}, {"RSP_DATA", 32}, {"RSP_STATUS", 3}, {"LOCAL_READ", 8}},
        {{"REQ_VALID", 1}, {"REQ_WRITE", 1}, {"REQ_ADDR", 32}, {"REQ_DATA", 32}, {"REQ_MASK", 4}, {"RSP_READY", 1},
         {"LOCAL_ADDR", 16}, {"LOCAL_WRITE", 1}, {"LOCAL_DATA", 8}, {"TARGET", 2}, {"STORING", 1}, {"DONE", 1}, {"FAILED", 1}},
        [lg](IOComponent& self, ComponentBuilder& gates) { copyEngine(self, gates, lg); });
    auto shift = group(b, "SHIFT_ENGINE", "MatrixShiftEngine<" + std::to_string(n) + ">",
        {{"CLK", 1}, {"RST", 1}, {"START", 1}}, {{"SHIFT", 1}, {"ROW", 16}, {"DONE", 1}},
        [lg](IOComponent& self, ComponentBuilder& gates) { shiftEngine(self, gates, lg); });
    auto mm = group(b, "MATMUL_ENGINE", "MatrixMultiplyEngine<" + std::to_string(n) + ">",
        {{"CLK", 1}, {"RST", 1}, {"START", 1}, {"ARG0", 32}, {"ARG1", 32}, {"ARG2", 32}},
        {{"ISSUE", 1}, {"ROW", 16}, {"WRITE", 1}, {"WRITE_ROW", 16}, {"ACCUMULATE", 1}, {"DONE", 1}},
        [n](IOComponent& self, ComponentBuilder& gates) { matmulEngine(self, gates, n); });
    auto act = group(b, "ACTIVATE_ENGINE", "MatrixActivateEngine",
        {{"CLK", 1}, {"RST", 1}, {"START", 1}, {"ARG0", 32}, {"ARG1", 32}, {"ARG2", 32}},
        {{"RUN", 1}, {"ACC_ROW", 16}, {"ACT_ROW", 16}, {"SCALE", 16}, {"SHIFT", 5}, {"RELU", 1}, {"DONE", 1}},
        activateEngine);
    for (auto& e : std::vector<std::shared_ptr<IOComponent>>{copy, shift, mm, act}) inputs(l, *this, e, {"CLK", "RST"});
    l.wire(seq->getOutputPin("START_COPY"), copy->getInputPin("START"));
    l.wire(seq->getOutputPin("STORE_OP"), copy->getInputPin("STORE_OP"));
    l.wire(seq->getOutputPin("START_SHIFT"), shift->getInputPin("START"));
    l.wire(seq->getOutputPin("START_MATMUL"), mm->getInputPin("START"));
    l.wire(seq->getOutputPin("START_ACTIVATE"), act->getInputPin("START"));
    for (int k = 0; k < 4; ++k) l.wire(arg(k), copy->getInputPin<32>("ARG" + std::to_string(k)));
    for (int k = 0; k < 3; ++k) {
        l.wire(arg(k), mm->getInputPin<32>("ARG" + std::to_string(k)));
        l.wire(arg(k), act->getInputPin<32>("ARG" + std::to_string(k)));
    }
    l.wire(copy->getOutputPin("DONE"), seq->getInputPin("COPY_DONE"));
    l.wire(copy->getOutputPin("FAILED"), seq->getInputPin("COPY_FAILED"));
    l.wire(shift->getOutputPin("DONE"), seq->getInputPin("SHIFT_DONE"));
    l.wire(mm->getOutputPin("DONE"), seq->getInputPin("MATMUL_DONE"));
    l.wire(act->getOutputPin("DONE"), seq->getInputPin("ACTIVATE_DONE"));
    // The DMA master port.
    l.wire(clk, getOutputPin("DMA_CLK"));
    l.wire(rst, getOutputPin("DMA_RST"));
    l.wire(copy->getOutputPin("REQ_VALID"), getOutputPin("DMA_REQ_VALID"));
    l.drive({copy->getOutputPin("REQ_WRITE"), l.bit(false), l.bit(false)}, getOutputPin<3>("DMA_REQ_OP"));
    l.wire(copy->getOutputPin<32>("REQ_ADDR"), getOutputPin<32>("DMA_REQ_ADDR"));
    l.wire(copy->getOutputPin<32>("REQ_DATA"), getOutputPin<32>("DMA_REQ_WRITE_DATA"));
    l.wire(copy->getOutputPin<4>("REQ_MASK"), getOutputPin<4>("DMA_REQ_WRITE_MASK"));
    l.wire(copy->getOutputPin("RSP_READY"), getOutputPin("DMA_RSP_READY"));
    l.wire(getInputPin("DMA_REQ_READY"), copy->getInputPin("REQ_READY"));
    l.wire(getInputPin("DMA_RSP_VALID"), copy->getInputPin("RSP_VALID"));
    l.wire(getInputPin<32>("DMA_RSP_READ_DATA"), copy->getInputPin<32>("RSP_DATA"));
    l.wire(getInputPin<3>("DMA_RSP_STATUS"), copy->getInputPin<3>("RSP_STATUS"));

    // Buffers: one memory per column.
    auto ports = group(b, "BUFFER_PORTS", "MatrixBufferPorts<" + std::to_string(n) + ">",
        {{"COPY_ACTIVE", 1}, {"STORING", 1}, {"TARGET", 2}, {"LOCAL_ADDR", 16}, {"LOCAL_WRITE", 1},
         {"MATMUL_ACTIVE", 1}, {"MATMUL_ROW", 16}, {"MATMUL_WRITE_ROW", 16}, {"ACTIVATE_RUN", 1},
         {"ACTIVATE_ACC_ROW", 16}, {"ACTIVATE_ACT_ROW", 16}, {"SHIFT_ACTIVE", 1}, {"SHIFT_ROW", 16}},
        {{"ACT_ADDR", 32}, {"ACC_ADDR", 32}, {"WEIGHT_ADDR", 32}, {"ACT_WE", n}, {"WEIGHT_WE", n}, {"BIAS_WE", 4 * n}},
        [n, lg](IOComponent& self, ComponentBuilder& gates) { bufferPorts(self, gates, n, lg); });
    l.wire(seq->getOutputPin("COPY_ACTIVE"), ports->getInputPin("COPY_ACTIVE"));
    l.wire(copy->getOutputPin("STORING"), ports->getInputPin("STORING"));
    l.wire(copy->getOutputPin<2>("TARGET"), ports->getInputPin<2>("TARGET"));
    l.wire(copy->getOutputPin<16>("LOCAL_ADDR"), ports->getInputPin<16>("LOCAL_ADDR"));
    l.wire(copy->getOutputPin("LOCAL_WRITE"), ports->getInputPin("LOCAL_WRITE"));
    l.wire(seq->getOutputPin("MATMUL_ACTIVE"), ports->getInputPin("MATMUL_ACTIVE"));
    l.wire(mm->getOutputPin<16>("ROW"), ports->getInputPin<16>("MATMUL_ROW"));
    l.wire(mm->getOutputPin<16>("WRITE_ROW"), ports->getInputPin<16>("MATMUL_WRITE_ROW"));
    l.wire(act->getOutputPin("RUN"), ports->getInputPin("ACTIVATE_RUN"));
    l.wire(act->getOutputPin<16>("ACC_ROW"), ports->getInputPin<16>("ACTIVATE_ACC_ROW"));
    l.wire(act->getOutputPin<16>("ACT_ROW"), ports->getInputPin<16>("ACTIVATE_ACT_ROW"));
    l.wire(shift->getOutputPin("SHIFT"), ports->getInputPin("SHIFT_ACTIVE"));
    l.wire(shift->getOutputPin<16>("ROW"), ports->getInputPin<16>("SHIFT_ROW"));
    const auto act_we = l.split(ports->getOutputPinDynamic("ACT_WE"));
    const auto weight_we = l.split(ports->getOutputPinDynamic("WEIGHT_WE"));

    auto bias = group(b, "BIAS", "MatrixBias<" + std::to_string(n) + ">",
        [n] { Fields f{{"CLK", 1}, {"RST", 1}, {"BYTE", 8}, {"WE", 4 * n}}; return f; }(),
        [n] { Fields f; for (size_t j = 0; j < n; ++j) f.emplace_back("BIAS_" + std::to_string(j), 32); return f; }(),
        [n](IOComponent& self, ComponentBuilder& gates) {
            Logic c(gates);
            const auto we = c.split(input(self, "WE"));
            for (size_t j = 0; j < n; ++j) {
                V word;
                for (size_t k = 0; k < 4; ++k) {
                    auto reg = c.reg("BIAS_" + std::to_string(j) + "_BYTE_" + std::to_string(k), 8,
                                     input(self, "CLK"), input(self, "RST"), we[4 * j + k]);
                    c.wire(input(self, "BYTE"), reg->getInputPinDynamic("D"));
                    const auto q = c.split(reg->getOutputPinDynamic("Q"));
                    word.insert(word.end(), q.begin(), q.end());
                }
                c.drive(word, output(self, "BIAS_" + std::to_string(j)));
            }
            c.finish();
        });
    inputs(l, *this, bias, {"CLK", "RST"});
    l.wire(copy->getOutputPin<8>("LOCAL_DATA"), bias->getInputPin<8>("BYTE"));
    l.wire(ports->getOutputPinDynamic("BIAS_WE"), bias->getInputPinDynamic("WE"));

    auto array = devicePart(*this, b, circuit::families::systolicArray(n), "ARRAY");
    inputs(l, *this, array, {"CLK", "RST"});
    l.wire(shift->getOutputPin("SHIFT"), array->getInputPin("SHIFT"));
    const circuit::ParameterMap act_bank{{"depth_words", std::to_string(act_rows)}, {"initial_value", "unknown"}};
    const circuit::ParameterMap acc_bank{{"depth_words", std::to_string(acc_rows)}, {"initial_value", "unknown"}};
    const circuit::ParameterMap weight_bank{{"depth_words", std::to_string(n)}, {"initial_value", "unknown"}};
    std::vector<P> act_read;
    for (size_t j = 0; j < n; ++j) {
        const auto index = std::to_string(j);
        auto act_mem = contractPart(*this, b, "memory.ff.device.8", "ACT_BANK_" + index, act_bank);
        auto acc_mem = contractPart(*this, b, "memory.ff.device.32", "ACC_BANK_" + index, acc_bank);
        auto weight_mem = contractPart(*this, b, "memory.ff.device.8", "WEIGHT_BANK_" + index, weight_bank);
        for (auto& m : {act_mem, acc_mem, weight_mem}) {
            inputs(l, *this, m, {"CLK", "RST"});
            l.wire(l.bit(true), m->getInputPin("READ_EN"));
        }
        l.wire(ports->getOutputPin<32>("ACT_ADDR"), act_mem->getInputPin<32>("WORD_ADDR"));
        l.wire(ports->getOutputPin<32>("ACC_ADDR"), acc_mem->getInputPin<32>("WORD_ADDR"));
        l.wire(ports->getOutputPin<32>("WEIGHT_ADDR"), weight_mem->getInputPin<32>("WORD_ADDR"));
        l.wire(act_we[j], act_mem->getInputPin("WE"));
        l.wire(mm->getOutputPin("WRITE"), acc_mem->getInputPin("WE"));
        l.wire(weight_we[j], weight_mem->getInputPin("WE"));
        l.wire(l.bit(true), act_mem->getInputPin<1>("WRITE_MASK"));
        l.wire(l.bit(true), weight_mem->getInputPin<1>("WRITE_MASK"));
        l.wire(l.constantWord(4, 15), acc_mem->getInputPin<4>("WRITE_MASK"));
        l.wire(copy->getOutputPin<8>("LOCAL_DATA"), weight_mem->getInputPin<8>("WRITE_DATA"));
        l.wire(weight_mem->getOutputPin<8>("READ_DATA"), array->getInputPin<8>("W_IN_" + index));

        auto requantize = devicePart(*this, b, circuit::families::Requantizer, "REQUANTIZE_" + index);
        l.wire(acc_mem->getOutputPin<32>("READ_DATA"), requantize->getInputPin<32>("ACC"));
        l.wire(act->getOutputPin<16>("SCALE"), requantize->getInputPin<16>("SCALE"));
        l.wire(act->getOutputPin<5>("SHIFT"), requantize->getInputPin<5>("SHIFT"));
        l.wire(act->getOutputPin("RELU"), requantize->getInputPin("RELU"));

        auto column = group(b, "LANE_" + index, "MatrixLane",
            {{"ACT_READ", 8}, {"ISSUE", 1}, {"ACC_READ", 32}, {"BIAS", 32}, {"Y", 32}, {"ACCUMULATE", 1},
             {"REQUANTIZED", 8}, {"COPY_BYTE", 8}, {"ACTIVATE_RUN", 1}},
            {{"X", 8}, {"ACC_WRITE", 32}, {"ACT_WRITE", 8}}, lane);
        l.wire(act_mem->getOutputPin<8>("READ_DATA"), column->getInputPin<8>("ACT_READ"));
        l.wire(mm->getOutputPin("ISSUE"), column->getInputPin("ISSUE"));
        l.wire(acc_mem->getOutputPin<32>("READ_DATA"), column->getInputPin<32>("ACC_READ"));
        l.wire(bias->getOutputPin<32>("BIAS_" + index), column->getInputPin<32>("BIAS"));
        l.wire(array->getOutputPin<32>("Y_OUT_" + index), column->getInputPin<32>("Y"));
        l.wire(mm->getOutputPin("ACCUMULATE"), column->getInputPin("ACCUMULATE"));
        l.wire(requantize->getOutputPin<8>("OUT"), column->getInputPin<8>("REQUANTIZED"));
        l.wire(copy->getOutputPin<8>("LOCAL_DATA"), column->getInputPin<8>("COPY_BYTE"));
        l.wire(act->getOutputPin("RUN"), column->getInputPin("ACTIVATE_RUN"));
        l.wire(column->getOutputPin<8>("X"), array->getInputPin<8>("X_IN_" + index));
        l.wire(column->getOutputPin<32>("ACC_WRITE"), acc_mem->getInputPin<32>("WRITE_DATA"));
        l.wire(column->getOutputPin<8>("ACT_WRITE"), act_mem->getInputPin<8>("WRITE_DATA"));
        act_read.push_back(act_mem->getOutputPin<8>("READ_DATA"));
    }
    // STORE reads one byte: the bank its local address selects.
    auto pick = group(b, "STORE_SELECT", "MatrixStoreSelect<" + std::to_string(n) + ">",
        [n] { Fields f{{"LOCAL_ADDR", 16}}; for (size_t j = 0; j < n; ++j) f.emplace_back("BANK_" + std::to_string(j), 8); return f; }(),
        {{"BYTE", 8}},
        [n, lg](IOComponent& self, ComponentBuilder& gates) {
            Logic c(gates);
            const auto address = c.split(input(self, "LOCAL_ADDR"));
            std::vector<V> words;
            for (size_t j = 0; j < n; ++j) words.push_back(c.split(input(self, "BANK_" + std::to_string(j))));
            for (size_t s = 0; s < lg; ++s) {
                std::vector<V> next;
                for (size_t j = 0; j + 1 < words.size(); j += 2) next.push_back(c.mux({address[s], words[j], words[j + 1]}));
                words = std::move(next);
            }
            c.drive(words.front(), output(self, "BYTE"));
            c.finish();
        });
    l.wire(copy->getOutputPin<16>("LOCAL_ADDR"), pick->getInputPin<16>("LOCAL_ADDR"));
    for (size_t j = 0; j < n; ++j) l.wire(act_read[j], pick->getInputPin<8>("BANK_" + std::to_string(j)));
    l.wire(pick->getOutputPin<8>("BYTE"), copy->getInputPin<8>("LOCAL_READ"));
    l.finish();
}

bool supportsMatrixUnit(const ParameterMap& p) {
    if (!supportsParameters(p, {"act_rows", "acc_rows", "queue_depth"})) return false;
    const auto act_rows = parameter(p, "act_rows", 256), acc_rows = parameter(p, "acc_rows", 64);
    return std::has_single_bit(act_rows) && act_rows >= 4 && act_rows <= 4096 &&
           std::has_single_bit(acc_rows) && acc_rows >= 4 && acc_rows <= 4096 &&
           circuit::device::validFifoDepth(parameter(p, "queue_depth", 4));
}

void registerMatrixUnits(ComponentCatalog& c) {
    for (size_t n : {4u, 8u, 16u}) {
        circuit::registration::registerStructural(c, circuit::families::matrixUnit(n), "MatrixUnitTest", "clock-boundary", supportsMatrixUnit);
        circuit::registration::registerBehavioral(c, circuit::families::matrixUnit(n), "MatrixUnitTest", "MatrixUnitScenarioTest",
                                                  supportsMatrixUnit, "clock-boundary");
    }
}
} // namespace circuit::accelerator

namespace circuit::families {
namespace {
template<size_t N> const ComponentFamily& unitFamily() {
    static const std::string id = "device.matrix-unit." + std::to_string(N);
    static const std::string type = "MatrixUnit<" + std::to_string(N) + ">";
    static const ComponentFamily f{id, type, unitPins,
        [](const std::string& n, const std::shared_ptr<BuildContext>& c) { return Component::createWithContext<accelerator::MatrixUnitOf<N>>(c, n); },
        [](const std::string& n, const std::shared_ptr<BuildContext>& c) {
            return Component::createWithContext<DeviceBehavioral<accelerator::MatrixUnitOf<N>>>(c, n); }};
    return f;
}
}
const ComponentFamily& matrixUnit(size_t n) {
    switch (n) {
    case 4: return unitFamily<4>();
    case 8: return unitFamily<8>();
    case 16: return unitFamily<16>();
    }
    throw std::invalid_argument("Invalid matrix unit size");
}
}
