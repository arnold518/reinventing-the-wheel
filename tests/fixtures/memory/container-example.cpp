// Appended to the exact C++ block extracted from site-docs/memory/3.1.6.md.
// It deliberately does not contain a second copy of the documented class.
#include <cassert>
#include <iostream>
#include <random>

namespace {
using namespace circuit::memory;

template<std::size_t W, class Model>
struct Rig {
    using Example = memory_example::Container<W, Model>;
    Example memory;
    typename Example::Inputs in;
    typename Example::Outputs out;
    std::size_t time = 0, next_clock = 128;

    explicit Rig(EndpointConfig config = {}) : memory(config) {
        out = memory.step(0, in);
        assert(out.request_ready && !out.response_valid);
    }

    void step_to(std::size_t target, bool clock_running = true) {
        auto event = memory.next_event_time();
        auto next = clock_running ? std::min(target, next_clock) : target;
        if (event) {
            assert(event > time); // No missed native deadline or zero-time loop.
            next = std::min(next, event);
        }
        assert(next > time);
        time = next;
        in.rising_edge = clock_running && time == next_clock;
        if (in.rising_edge) next_clock += 128;
        out = memory.step(time, in);
    }

    void tick() { step_to(next_clock); }

    void advance(std::size_t duration, bool clock_running = true) {
        auto end = time + duration;
        while (time < end) step_to(end, clock_running);
        if (!clock_running) next_clock = time + 128;
    }

    template<class Predicate>
    void until(Predicate predicate) {
        for (unsigned i = 0; !predicate(); ++i) {
            assert(i < 20000); // Finite test even if the excerpt loses a response.
            tick();
        }
    }

    static Request request(Operation op, std::uint32_t address,
                           std::uint32_t data = 0, unsigned mask = (1u << (W / 8)) - 1) {
        return {bits(3, unsigned(op)), bits(32, address), bits(W, data), bits(W / 8, mask)};
    }

    void submit(const Request& request) {
        until([&] { return out.request_ready; });
        in.request = request;
        in.request_valid = true;
        do { tick(); } while (!in.rising_edge);
        assert(!out.request_ready && !out.response_valid);
        in.request_valid = false;
        // Payload changes after acceptance must not change the latched request.
        in.request = Rig::request(Operation::EraseBlock, 0xffffffffu);
    }

    void consume() {
        assert(out.response_valid);
        in.response_ready = true;
        until([&] { return !out.response_valid; });
        in.response_ready = false;
    }

    auto transact(const Request& request) {
        submit(request);
        until([&] { return out.response_valid; });
        auto result = out;
        for (unsigned i = 0; i < 4; ++i) {
            tick();
            assert(out.response_valid && !out.request_ready);
            assert(out.status == result.status && out.read_data == result.read_data);
        }
        consume();
        return result;
    }

    auto read(std::uint32_t address) { return transact(request(Operation::Read, address)); }

    void expect(const Request& request, Status status, const Bits& data = Bits(W, L)) {
        auto result = transact(request);
        if (result.status != status || result.read_data != data)
            std::cerr << technologyName(Model::technology) << '/' << W
                      << " op=" << number(request.op) << " address=" << number(request.address)
                      << " expected status=" << unsigned(status) << " data=" << number(data)
                      << " known=" << known(data) << "; actual status=" << unsigned(result.status)
                      << " data=" << number(result.read_data) << " known=" << known(result.read_data) << '\n';
        assert(result.status == status && result.read_data == data);
    }
};

template<std::size_t W, class Model>
void check_model() {
    EndpointConfig config;
    config.image_hex = Model::technology == Technology::ROM ? "3412" : "";
    Rig<W, Model> rig(config);
    using R = Rig<W, Model>;
    constexpr auto bytes = W / 8;
    constexpr auto mask = (1u << bytes) - 1;
    assert(rig.memory.storage().wordBits() == W);
    assert(rig.memory.storage().capacityWords() == 4);
    assert(rig.memory.storage().capacityBytes() == 4 * bytes);

    auto initial = rig.read(0);
    assert(initial.status == Status::Ok);
    if constexpr (Model::technology == Technology::ROM) {
        assert(initial.read_data == bits(W, 0x1234));
        rig.expect(R::request(Operation::Read, 3 * bytes), Status::Ok);
        for (auto op : {Operation::Write, Operation::Program, Operation::EraseBlock})
            rig.expect(R::request(op, 0), Status::ReadOnly);
        rig.expect(R::request(Operation::Read, 0), Status::Ok, initial.read_data);
    } else if constexpr (Model::technology == Technology::NORFlash) {
        assert(initial.read_data == Bits(W, H));
        rig.expect(R::request(Operation::Write, 0), Status::Unsupported);
        rig.expect(R::request(Operation::Program, 0, 0xf0f0f0f0), Status::Ok);
        rig.expect(R::request(Operation::Program, 0, 0xffffffff), Status::Ok);
        rig.expect(R::request(Operation::Read, 0), Status::Ok, bits(W, 0xf0f0f0f0));
        rig.expect(R::request(Operation::Program, 0, 0, 0), Status::Ok);
        rig.expect(R::request(Operation::Read, 0), Status::Ok, bits(W, 0xf0f0f0f0));
        rig.expect(R::request(Operation::Program, 0, 0, 1), Status::Ok);
        rig.expect(R::request(Operation::Read, 0), Status::Ok, bits(W, 0xf0f0f000));
        rig.expect(R::request(Operation::Program, bytes, 0), Status::Ok);
        rig.expect(R::request(Operation::Program, 3 * bytes, 0x55555555), Status::Ok);
        rig.expect(R::request(Operation::EraseBlock, bytes), Status::Alignment);
        auto erase = R::request(Operation::EraseBlock, 0);
        erase.data.assign(W, X); erase.mask.assign(bytes, X); // Ignored by erase.
        rig.expect(erase, Status::Ok);
        rig.expect(R::request(Operation::Read, 0), Status::Ok, Bits(W, H));
        rig.expect(R::request(Operation::Read, bytes), Status::Ok, Bits(W, H));
        rig.expect(R::request(Operation::Read, 3 * bytes), Status::Ok, bits(W, 0x55555555));
        auto unknown = R::request(Operation::Program, 0);
        unknown.data.assign(W, X);
        rig.expect(unknown, Status::Ok);
        rig.expect(R::request(Operation::Read, 0), Status::Ok, Bits(W, X));
        rig.expect(R::request(Operation::Program, 0, 0xffffffff), Status::Ok);
        rig.expect(R::request(Operation::Read, 0), Status::Ok, Bits(W, X));
        rig.expect(R::request(Operation::Program, 0, 0), Status::Ok);
        rig.expect(R::request(Operation::Read, 0), Status::Ok);
    } else {
        assert(initial.read_data == Bits(W, X));
        rig.expect(R::request(Operation::Write, 3 * bytes, 0x12345678), Status::Ok);
        rig.expect(R::request(Operation::Write, 3 * bytes, 0, 0), Status::Ok);
        rig.expect(R::request(Operation::Read, 3 * bytes), Status::Ok, bits(W, 0x12345678));
        rig.expect(R::request(Operation::Write, 3 * bytes, 0xa5, 1), Status::Ok);
        rig.expect(R::request(Operation::Read, 3 * bytes), Status::Ok, bits(W, 0x123456a5));
        rig.expect(R::request(Operation::Program, 0), Status::Unsupported);
        rig.expect(R::request(Operation::EraseBlock, 0), Status::Unsupported);
        auto unknown = R::request(Operation::Write, 0, 0xffffffff, mask);
        unknown.data[0] = X; unknown.data[1] = Z;
        rig.expect(unknown, Status::Ok);
        auto expected = Bits(W, H); expected[0] = expected[1] = X;
        rig.expect(R::request(Operation::Read, 0), Status::Ok, expected);
    }

    rig.expect(R::request(Operation::Read, 4 * bytes), Status::Range);
    if constexpr (W > 8) rig.expect(R::request(Operation::Read, 1), Status::Alignment);
    rig.expect(R::request(Operation(7), 0), Status::Unsupported);
    auto bad = R::request(Operation::Read, 0); bad.address[0] = X;
    rig.expect(bad, Status::InvalidRequest);
    bad = R::request(Operation::Write, 0); bad.mask[0] = X;
    rig.expect(bad, Status::InvalidRequest);
    auto expected = rig.read(0);
    auto ignored = R::request(Operation::Read, 0);
    ignored.data.assign(W, X); ignored.mask.assign(bytes, Z);
    rig.expect(ignored, expected.status, expected.read_data);

    // Reset clears the interface, not contents, for every technology model.
    rig.in.reset = true; rig.tick();
    assert(!rig.out.request_ready && !rig.out.response_valid);
    rig.in.reset = false; rig.tick();
    rig.expect(R::request(Operation::Read, 0), expected.status, expected.read_data);
}

void check_refresh_and_retention() {
    EndpointConfig config;
    config.retention_ticks = 100000;
    config.refresh_interval_ticks = 10000;
    Rig<32, DRAMModel> rig(config);
    using R = Rig<32, DRAMModel>;
    rig.expect(R::request(Operation::Write, 0, 0x12345678), Status::Ok);
    rig.submit(R::request(Operation::Read, 0));
    rig.until([&] { return rig.out.response_valid; });
    rig.advance(3 * config.retention_ticks); // Maintenance continues under backpressure.
    assert(rig.out.response_valid && rig.out.read_data == bits(32, 0x12345678));
    rig.consume();
    rig.expect(R::request(Operation::Read, 0), Status::Ok, bits(32, 0x12345678));
    rig.read(8); // Close the target row by opening the other row.
    rig.advance(3 * config.retention_ticks, false); // No new clock-triggered refresh.
    rig.expect(R::request(Operation::Read, 0), Status::Ok, Bits(32, X));
}

void check_reset_during_program() {
    EndpointConfig config;
    config.program_ticks = 20000; config.erase_ticks = 30000;
    Rig<8, NORFlashModel> rig(config);
    using R = Rig<8, NORFlashModel>;
    rig.submit(R::request(Operation::Program, 0, 0x5a));
    rig.advance(2048); // WREN and PROGRAM have launched, but mutation is still pending.
    assert(rig.memory.storage().readLogic(0, 1) == Bits(8, H));
    rig.in.reset = true; rig.tick();
    rig.in.reset = false;
    rig.until([&] {
        assert(!rig.out.response_valid); // The aborted response must not reappear.
        return rig.out.request_ready;
    });
    rig.expect(R::request(Operation::Read, 0), Status::Ok, bits(8, 0x5a));
}

void check_configuration_and_sparse_storage() {
    auto example = memory_example::make_sram_example();
    assert(example.storage().capacityBytes() == 16384);
    EndpointConfig config;
    config.depth_words = 1u << 20; config.record_history = false;
    config.max_resident_bytes = 16384;
    Rig<32, SRAMModel> rig(config);
    using R = Rig<32, SRAMModel>;
    assert(rig.memory.storage().capacityBytes() == 4u * (1u << 20));
    assert(rig.memory.storage().allocatedDataBytes() == 0);
    auto last = std::uint32_t((config.depth_words - 1) * 4);
    rig.expect(R::request(Operation::Write, last, 0xabcdef01), Status::Ok);
    rig.expect(R::request(Operation::Read, last), Status::Ok, bits(32, 0xabcdef01));
    assert(rig.memory.storage().allocatedDataBytes() <= config.max_resident_bytes);
    config.depth_words = 0;
    bool rejected = false;
    try { memory_example::Container<32, SRAMModel> invalid(config); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}

template<std::size_t W>
void compare_compact_devices() {
    auto compare = []<class Model>() {
        EndpointConfig config;
        config.depth_words = 8;
        config.refresh_interval_ticks = 1024;
        config.retention_ticks = 100000;
        config.record_history = false;
        config.validate(W, Model::technology);
        memory_example::CompactDevice<W, Model> reference(config);
        auto actual = makeDeviceModel(W, Model::technology, config);
        memory_example::Sequence sequence(W, Model::technology, config);
        std::mt19937 random(518 + W);
        Request payload{bits(3, 0), bits(32, 0), Bits(W, L), Bits(W / 8, H)};
        bool start = false;
        auto check = [&] {
            for (auto pin : {"BUSY", "ERROR", "READ_VALID", "FAULT"})
                assert(reference.read(pin, 1) == actual->read(pin, 1));
            assert(reference.read("READ_DATA", W) == actual->read("READ_DATA", W));
            assert(reference.deadline() == actual->deadline());
            assert(reference.storage.readLogic(0, 8 * W / 8) == actual->storage.readLogic(0, 8 * W / 8));
        };
        for (std::size_t t = 0; t < 24000; t += 8) {
            if (sequence.done) start = false;
            else if (sequence.available && !start) {
                auto op = Model::technology == Technology::ROM ? 0u : random() % 2;
                auto word = random() % 8;
                if constexpr (Model::technology == Technology::NORFlash) {
                    op = random() % 3;
                    if (op) ++op; // READ / PROGRAM / ERASE_BLOCK.
                    if (op == 3) word -= word % config.block_words;
                }
                payload = {bits(3, op), bits(32, word * (W / 8)), bits(W, random()), bits(W / 8, random())};
                if (random() % 5 == 0) payload.data[0] = X;
                start = true;
            }
            reference.update(t, sequence.outputs); actual->update(t, sequence.outputs); check();
            sequence.step(t, t % 128 == 0, t % 4096 == 0, start, payload,
                          [&](const std::string& p, std::size_t w) { return actual->read(p, w); });
            reference.update(t, sequence.outputs); actual->update(t, sequence.outputs); check();
        }
    };
    compare.template operator()<FFModel>(); compare.template operator()<SRAMModel>();
    compare.template operator()<DRAMModel>(); compare.template operator()<ROMModel>();
    compare.template operator()<NORFlashModel>();
}

template<std::size_t W>
void check_width() {
    check_model<W, FFModel>(); check_model<W, SRAMModel>(); check_model<W, DRAMModel>();
    check_model<W, ROMModel>(); check_model<W, NORFlashModel>();
    compare_compact_devices<W>();
}
} // namespace

int main() {
    check_width<8>(); check_width<16>(); check_width<32>();
    check_refresh_and_retention(); check_reset_during_program();
    check_configuration_and_sparse_storage();
    std::cout << "Container excerpt: five models x 8/16/32, timing/storage, and 90000 compact-device comparisons passed\n";
}
