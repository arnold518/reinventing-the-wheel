// Executed after the exact request-check and endpoint excerpts of
// site-docs/memory/controllers.md (and the transaction-port excerpt, which
// brings in MemoryEndpointEngine.hpp for the production request check).
#include <cassert>
#include <iostream>

namespace {
using namespace circuit::memory;

std::size_t compare_with_production() {
    const Tech reference[] = {Tech::FF, Tech::SRAM, Tech::DRAM, Tech::ROM, Tech::NOR};
    const Technology production[] = {Technology::FF, Technology::SRAM, Technology::DRAM,
                                     Technology::ROM, Technology::NORFlash};
    std::size_t compared = 0;
    for (unsigned width : {8u, 16u, 32u})
        for (unsigned t = 0; t < 5; ++t)
            for (unsigned block : {1u, 2u, 4u})
                for (unsigned rows : {1u, 2u, 4u}) {
                    EndpointConfig config;
                    config.depth_words = 16; config.block_words = block; config.words_per_row = rows;
                    for (unsigned op = 0; op < 8; ++op)
                        for (std::uint32_t address = 0; address < 16 * (width / 8) + 8; ++address)
                            for (unsigned known = 0; known < 8; ++known) {
                                const bool op_known = known & 1, address_known = known & 2, mask_known = known & 4;
                                Request r{op_known ? bits(3, op) : Bits(3, X),
                                          address_known ? bits(32, address) : Bits(32, X),
                                          Bits(width, L), mask_known ? Bits(width / 8, H) : Bits(width / 8, X)};
                                const unsigned expected = unsigned(validateRequest(r, width, production[t], config));
                                const unsigned documented = request_status(reference[t], width, 16, block, rows,
                                    op_known, op, address_known, address, mask_known);
                                if (expected != documented) {
                                    std::cerr << "technology " << t << " width " << width << " op " << op
                                              << " address " << address << " known " << known << ": production "
                                              << expected << ", documented " << documented << "\n";
                                    assert(false);
                                }
                                ++compared;
                            }
                }
    return compared;
}

void check_endpoint_reference() {
    std::vector<std::uint8_t> ram(16 * 4); // 16 words of 32 bits.
    auto r = endpoint_responses(Tech::SRAM, 32, 4, 2, ram, 1, 16, 0x12345678, 0xf);
    assert(r.size() == 1 && r[0].status == 0 && r[0].data == 0);
    r = endpoint_responses(Tech::SRAM, 32, 4, 2, ram, 1, 16, 0xaabbccdd, 0x5); // Bytes 0 and 2.
    r = endpoint_responses(Tech::SRAM, 32, 4, 2, ram, 0, 16, 0, 0);
    assert(r.size() == 1 && r[0].data == 0x12bb56dd);
    for (std::uint32_t w = 4; w < 8; ++w)
        endpoint_responses(Tech::SRAM, 32, 4, 2, ram, 1, w * 4, 0x100 + w, 0xf);
    r = endpoint_responses(Tech::SRAM, 32, 4, 2, ram, 4, 6 * 4, 0, 0); // Burst from word 6.
    assert(r.size() == 4 && r[0].data == 0x106 && r[1].data == 0x107 && r[2].data == 0x104 && r[3].data == 0x105);
    assert(endpoint_responses(Tech::SRAM, 32, 4, 2, ram, 0, 64, 0, 0)[0].status == 2); // Range.
    assert(endpoint_responses(Tech::SRAM, 32, 4, 2, ram, 0, 2, 0, 0)[0].status == 1);  // Alignment.
    assert(endpoint_responses(Tech::SRAM, 32, 4, 2, ram, 5, 0, 0, 0)[0].status == 4);  // Reserved.
    assert(endpoint_responses(Tech::FF, 32, 4, 2, ram, 4, 0, 0, 0)[0].status == 4);    // No FF burst.
    assert(endpoint_responses(Tech::DRAM, 32, 4, 2, ram, 4, 0, 0, 0)[0].status == 4);  // Burst longer than a row.
    assert(endpoint_responses(Tech::ROM, 32, 4, 2, ram, 1, 0, 1, 0xf)[0].status == 3); // Read only.

    std::vector<std::uint8_t> flash(16 * 4, 0xff);
    assert(endpoint_responses(Tech::NOR, 32, 2, 2, flash, 1, 0, 0, 0xf)[0].status == 4); // No WRITE.
    endpoint_responses(Tech::NOR, 32, 2, 2, flash, 2, 0, 0x0f0f0f0f, 0xf);
    assert(endpoint_responses(Tech::NOR, 32, 2, 2, flash, 0, 0, 0, 0)[0].data == 0x0f0f0f0f);
    endpoint_responses(Tech::NOR, 32, 2, 2, flash, 2, 0, 0xf0f0f0ff, 0xf); // Clears bits only.
    assert(endpoint_responses(Tech::NOR, 32, 2, 2, flash, 0, 0, 0, 0)[0].data == 0x0000000f);
    endpoint_responses(Tech::NOR, 32, 2, 2, flash, 2, 8, 0x3c, 0x1);
    assert(endpoint_responses(Tech::NOR, 32, 2, 2, flash, 3, 4, 0, 0)[0].status == 1); // Not block-aligned.
    endpoint_responses(Tech::NOR, 32, 2, 2, flash, 3, 0, 0, 0); // Erase words 0 and 1.
    assert(endpoint_responses(Tech::NOR, 32, 2, 2, flash, 0, 0, 0, 0)[0].data == 0xffffffff);
    assert(endpoint_responses(Tech::NOR, 32, 2, 2, flash, 0, 8, 0, 0)[0].data == 0xffffff3c); // Other block kept.
}
} // namespace

int main() {
    const auto compared = compare_with_production();
    check_endpoint_reference();
    std::cout << "request check: " << compared << " cases equal to the production check; endpoint reference passed\n";
}
