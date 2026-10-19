// Executed after the exact transaction-port excerpt of site-docs/memory/transactions.md.
#include <cassert>
#include <iostream>

namespace {
using namespace circuit::memory;

template<std::size_t W>
Request request(Operation op, std::uint32_t address, std::uint32_t value = 0) {
    return {bits(3, unsigned(op)), bits(32, address), bits(W, value), Bits(W / 8, H)};
}

template<std::size_t W>
void check_port() {
    memory_example::Port<W> port;
    typename memory_example::Port<W>::Inputs in;
    in.reset = true;
    auto out = port.step(in);
    assert(!out.request_ready && !out.response_valid && !out.start);
    in.reset = false; in.request_valid = true;
    in.request = request<W>(Operation::Write, 12, 0xa5);
    out = port.step(in);
    assert(!out.request_ready && out.request.address == bits(32, 12)); // Backend not ready.
    in.backend_ready = true;
    out = port.step(in); // Live bypass, no edge or extra setup cycle.
    assert(out.start && out.request_ready && out.space && out.request.data == bits(W, 0xa5));
    in.rising_edge = true; out = port.step(in);
    assert(!out.start && !out.request_ready); // Pending.
    in.rising_edge = false;

    // Completion while the consumer is not ready: REQ_READY follows registered
    // state only, so the next request is still accepted.
    in.request = request<W>(Operation::Read, 0);
    in.done = true; in.result = bits(W, 0x5a); in.status = bits(3, 5); in.response_ready = false;
    out = port.step(in);
    assert(out.response_valid && out.finish && out.read_data == bits(W, 0x5a));
    assert(out.request_ready && out.start && out.space);
    assert(out.request.address == bits(32, 0)); // The sequencer, not the frontend, owns the request.
    in.rising_edge = true; out = port.step(in); // The blocked response is retained.
    in.rising_edge = false; in.done = false; in.request_valid = false;
    in.result.assign(W, X); in.status.assign(3, X);
    for (unsigned i = 0; i < 3; ++i) {
        out = port.step(in);
        assert(out.response_valid && out.read_data == bits(W, 0x5a) && out.status == bits(3, 5));
        assert(!out.request_ready && !out.space); // A response is held.
    }
    in.response_ready = true; in.rising_edge = true; out = port.step(in); // Consumed.
    in.rising_edge = false; out = port.step(in);
    assert(!out.response_valid && out.space);

    // The second request completes; then a burst of two words.
    in.done = true; in.result = bits(W, 0x11); in.status = bits(3, 0);
    out = port.step(in);
    assert(out.response_valid && out.finish && out.request_ready);
    in.request_valid = true; in.request = request<W>(Operation::ReadBurst, 4);
    out = port.step(in); assert(out.start);
    in.rising_edge = true; port.step(in); // First request done, burst accepted.
    in.rising_edge = false; in.request_valid = false;
    in.result = bits(W, 0x21); in.last = false; // First word of the burst.
    out = port.step(in);
    assert(out.response_valid && out.finish && !out.request_ready); // Not the last word.
    in.rising_edge = true; port.step(in);
    in.rising_edge = false; in.result = bits(W, 0x22); in.last = true;
    out = port.step(in);
    assert(out.response_valid && out.read_data == bits(W, 0x22) && out.request_ready);
    in.rising_edge = true; port.step(in);
    in.rising_edge = false; in.done = false; out = port.step(in);
    assert(!out.response_valid && out.request_ready); // Idle again.
    in.reset = true; out = port.step(in);
    assert(!out.request_ready && !out.response_valid && !out.start);

    memory_example::ResponseBuffer<W> buffer;
    typename memory_example::ResponseBuffer<W>::Inputs b;
    b.valid = true; b.ready = true; b.data = bits(W, 0x31);
    auto view = buffer.outputs(b);
    assert(view.ready && view.valid && view.data == b.data);
    b.edge = true; buffer.step(b); // An empty buffer passes the response on without filling.
    b.valid = false; assert(!buffer.outputs(b).valid && !buffer.full());
    b.ready = false; b.valid = true; buffer.step(b);
    assert(buffer.full());
    b.data = bits(W, 0x42); b.ready = true;
    assert(buffer.outputs(b).data == bits(W, 0x31)); // The old result goes first.
    buffer.step(b); b.valid = false; b.ready = false;
    assert(buffer.outputs(b).data == bits(W, 0x42) && buffer.outputs(b).valid);
}
} // namespace

int main() {
    check_port<8>();
    check_port<16>();
    check_port<32>();
    std::cout << "transaction port reference: registered REQ_READY, held responses and bursts at W=8/16/32\n";
}
