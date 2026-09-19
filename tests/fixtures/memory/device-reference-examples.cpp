// Compiled after exact helper, DRAM, ROM, and NOR documentation snippets.
#include <cassert>
#include <string>

std::string word_text(const std::vector<std::optional<bool>>& bits, bool z = false) {
    if (z) return std::string(bits.size(), 'Z');
    std::string out;
    for (auto it = bits.rbegin(); it != bits.rend(); ++it)
        out += !it->has_value() ? 'X' : **it ? '1' : '0';
    return out;
}
std::string binary_word(unsigned width, std::uint32_t word) {
    std::string out;
    while (width) out += (word >> --width) & 1u ? '1' : '0';
    return out;
}
template<class Device, class Inputs>
void drain(Device& device, const Inputs& in, std::size_t& time) {
    for (unsigned step = 0; device.control.due; ++step) {
        assert(step < 8 && device.control.due > time);
        time = device.control.due; device.update(time, in);
    }
}
template<unsigned W>
void check_native_devices() {
    std::size_t time = 0;
    DRAMReference<W> dram(4, 2, 32, 1024);
    typename DRAMControlReference<W>::Inputs di;
    auto dram_command = [&](unsigned op, unsigned address = 0, std::uint32_t data = 0, unsigned mask = 15) {
        di = {false, op, address, data, mask}; dram.update(++time, di);
        di.clock = true; dram.update(++time, di); drain(dram, di, time);
    };
    dram_command(2); assert(dram.control.error && !dram.control.open && dram.high_z);
    dram_command(0); assert(dram.control.error);
    dram_command(1); assert(dram.control.open && !dram.control.error);
    dram_command(3, 0, 0x12345678); dram_command(2);
    assert(word_text(dram.read_data) == binary_word(W, 0x12345678));
    dram_command(3, 0, 0xffffffff, 0); dram_command(2);
    assert(word_text(dram.read_data) == binary_word(W, 0x12345678));
    dram_command(3, 0, 0xaabbccdd, 1); dram_command(2);
    assert(word_text(dram.read_data) == binary_word(W, 0x123456dd));
    // Attempt another command during a launched WRITE: latch and schedule survive.
    di = {false, 3, 1, 0x81, 15}; dram.update(++time, di);
    di.clock = true; dram.update(++time, di);
    auto due = dram.control.due;
    di.clock = false; dram.update(++time, di);
    di = {true, 1, 1, 0, 0}; dram.update(++time, di);
    assert(dram.control.error && dram.control.busy && dram.control.due == due && dram.control.col == 1);
    drain(dram, di, time);
    dram_command(2, 1); assert(word_text(dram.read_data) == binary_word(W, 0x81));
    dram_command(7); assert(dram.control.error && dram.high_z);
    dram_command(2, 2); assert(dram.control.error);
    dram_command(5); assert(dram.control.error); // Refresh requires closed rows.
    dram_command(4); assert(!dram.control.open);
    dram_command(5); assert(!dram.control.open && dram.control.refresh_row == 1);
    dram_command(5); assert(dram.control.refresh_row == 0);
    dram_command(1); dram_command(2, 1);
    assert(word_text(dram.read_data) == binary_word(W, 0x81)); // Refresh restored charge.
    dram_command(4); time += 1024;
    dram_command(1); dram_command(2, 1);
    assert(word_text(dram.read_data) == std::string(W, 'X')); // Expired while closed.

    FlashReference<W> flash(4, 2, 2, 32, 128, 256);
    typename FlashControlReference<W>::Inputs fi;
    auto flash_command = [&](unsigned op, unsigned address = 0, std::uint32_t data = 0, unsigned mask = 15) {
        fi = {false, op, address, data, mask}; flash.update(++time, fi);
        fi.clock = true; flash.update(++time, fi); drain(flash, fi, time);
    };
    flash_command(1); assert(word_text(flash.read_data) == std::string(W, '1'));
    flash_command(2); assert(flash.control.error && flash.high_z);
    flash_command(4, 0xffffffff); assert(flash.control.wel && !flash.control.error);
    flash_command(7); assert(flash.control.error && flash.control.wel);
    flash_command(2, 0, 0, 0); assert(!flash.control.wel); // Zero mask still consumes WEL.
    flash_command(1); assert(word_text(flash.read_data) == std::string(W, '1'));
    flash_command(4); flash_command(2, 0, 0x12345678);
    flash_command(4); flash_command(2, 0, 0xffffffff);
    flash_command(1); assert(word_text(flash.read_data) == binary_word(W, 0x12345678));
    flash_command(4); flash_command(2, 0, 0, 1);
    flash_command(1); assert(word_text(flash.read_data) == binary_word(W, 0x12345600));
    flash_command(4); flash_command(2, 2, 0x81);
    flash_command(4); flash_command(3, 1); // Native ERASE accepts an interior address.
    flash_command(1, 0); assert(word_text(flash.read_data) == std::string(W, '1'));
    flash_command(1, 1); assert(word_text(flash.read_data) == std::string(W, '1'));
    flash_command(1, 2); assert(word_text(flash.read_data) == binary_word(W, 0x81));
    flash_command(4); flash_command(5); assert(!flash.control.wel && !flash.control.valid);
    flash_command(1, 4); assert(flash.control.error);
    flash_command(0); assert(flash.control.error);
    flash_command(4);
    fi = {false, 2, 0, 0x5a, 15}; flash.update(++time, fi);
    fi.clock = true; flash.update(++time, fi);
    due = flash.control.due;
    assert(flash.control.busy && !flash.control.wel && !flash.control.program);
    assert(flash.bank.rows[0][0].q == true); // Mutation is not immediate.
    fi.clock = false; flash.update(++time, fi);
    fi = {true, 4, 0, 0, 0}; flash.update(++time, fi);
    assert(flash.control.error && !flash.control.wel && flash.control.due == due);
    time = due; flash.update(time, fi);
    assert(flash.control.program && flash.bank.rows[0][0].q == false);
    drain(flash, fi, time);
    flash_command(1); assert(word_text(flash.read_data) == binary_word(W, 0x5a));
}

void check_device_references() {
    assert(matrix_decode(3, 2, true) == std::vector<bool>({false, false, true}));
    assert(matrix_decode(3, 0xffffffffu, true) == std::vector<bool>(3, false));
    std::vector<bool> columns(16, false); columns[8] = true;
    assert(matrix_mux(8, 1, columns) == 1 && matrix_mux(8, 2, columns) == 0);
    assert((matrix_write_column(16, 24, 1, true, 0x100, 2) == std::array<bool,2>{true,true}));
    assert(!matrix_write_column(16, 24, 1, true, 0x100, 1)[1]);
    assert(static_read_column({false,true}, {false,true}) == true);
    assert(!static_read_column({true,true}, {false,true}));
    assert(!memory_output(0xA5, false) && memory_output(0xA5, true) == 0xA5);
    // Initial high is not an edge. Completion precedes a simultaneous new edge.
    DRAMControlReference<8> dc(2, 2, 32);
    DRAMControlReference<8>::Inputs di{true, 1, 0, 0, 1};
    dc.update(0, di); assert(!dc.busy);
    di.clock = false; dc.update(1, di);
    di.clock = true; dc.update(2, di); assert(dc.busy && dc.due == 34);
    dc.update(34, di); dc.update(66, di);
    di.clock = false; dc.update(97, di);
    di.clock = true; di.cmd = 2; dc.update(98, di);
    assert(dc.open && dc.busy && !dc.error && dc.command == 2 && dc.due == 130);
    dc.update(130, di); assert(dc.output && !dc.busy);
    FlashControlReference<8> fc(4, 2, 2, 32, 128, 256);
    FlashControlReference<8>::Inputs fi{true, 4, 0, 0, 1};
    fc.update(0, fi); assert(!fc.wel);
    fi.clock = false; fc.update(1, fi);
    fi.clock = true; fc.update(2, fi); assert(fc.wel);
    fi.clock = false; fi.cmd = 2; fc.update(3, fi);
    fi.clock = true; fc.update(4, fi); assert(fc.due == 132);
    fc.update(132, fi); assert(fc.program && fc.busy);
    fc.update(164, fi); assert(!fc.program && fc.busy);
    fi.clock = false; fi.cmd = 1; fc.update(195, fi);
    fi.clock = true; fc.update(196, fi);
    assert(fc.busy && !fc.error && fc.command == 1 && fc.due == 228);
    fc.update(228, fi); assert(fc.valid && !fc.busy);
    DRAMCellReference cell;
    cell.update(10, 100, true, true, true); cell.update(20, 100, false, false, false);
    cell.update(30, 100, true, false, false); assert(cell.sample == true && !cell.q);
    cell.update(40, 100, false, false, false); cell.update(50, 100, true, false, false);
    assert(!cell.sample);
    cell.update(60, 100, true, true, true); cell.update(70, 100, false, false, false);
    cell.update(170, 100, true, false, false); assert(!cell.sample);
    std::array<DRAMCellReference, 2> cells;
    dram_row_update(cells, 10, 100, true, std::array<bool,2>{true,true},
                    std::array<std::optional<bool>,2>{true,false});
    assert(cells[0].q == true && cells[1].q == false);
    DRAMSenseReference sense;
    sense.update(false, true, {true, false}, {true, false}, false, false); assert(sense.q == true);
    sense.update(false, true, {true, false}, {std::nullopt, false}, false, false); assert(sense.q == true);
    sense.update(false, false, {false, false}, {}, false, false); assert(sense.q == true && !sense.restore);
    sense.update(false, true, {true, true}, {true, false}, false, false); assert(!sense.q && !sense.restore);
    sense.update(true, false, {true, false}, {}, false, false); assert(!sense.prepared);
    sense.update(true, false, {false, false}, {}, false, false); assert(sense.prepared);
    DRAMBankReference bank(2, 2);
    bank.update(0, 100, true, false, {false,false}, {false,false}, {false,false});
    bank.update(10, 100, false, false, {true,false}, {false,false}, {false,false});
    bank.update(20, 100, false, true, {true,false}, {true,true}, {true,false});
    assert(bank.rows[0][0].q == true && bank.columns[1].q == false);
    bank.update(30, 100, false, false, {false,false}, {false,false}, {false,false});
    bank.update(40, 100, true, false, {false,false}, {false,false}, {false,false});
    bank.update(50, 100, false, false, {true,false}, {false,false}, {false,false});
    bank.update(60, 100, false, true, {true,false}, {false,false}, {false,false});
    assert(bank.columns[0].q == true && bank.columns[1].q == false);
    assert(ROMCellReference{true}.read());
    assert(rom_row("1010") == std::vector<bool>({true,false,true,false}));
    assert(word_text(rom_bank(8, 2, {0xa5,0x3c,0x81,0xff}, {true,false})) == binary_word(16, 0x3ca5));
    assert(word_text(rom_bank(8, 2, {}, {true,true})) == std::string(16, 'X'));
    assert(word_text(rom_bank(8, 2, {0xa5}, {false,true})) == std::string(16, '0'));
    auto a = rom_address(3,4,2,false,false);
    assert(a.row == 1 && a.slot == 1 && a.row_enable && a.output_enable == true);
    assert(!rom_address(4,4,2,false,false).output_enable);
    assert(rom_address(4,4,2,true,false).output_enable == false);
    assert(rom_read<32>({0x78,0x56,0x34,0x12},2,0,false,false).data == 0x12345678);
    assert(rom_read<16>({0x78},2,0,false,false).data == 0x78);
    assert(rom_read({},2,1,false,false).data == 0);
    assert(rom_read({0xA5},1,1,false,false).state == ROMReadReference::State::Unknown);
    assert(rom_read({0xA5},1,9,true,false).state == ROMReadReference::State::HighZ);
    NORFlashCellReference flash;
    flash.update(true,true,false,false); flash.update(true,true,false,true); assert(flash.q == false);
    flash.update(false,false,true,false); assert(flash.q == true);
    flash.update(true,true,true,false); assert(!flash.q); // Conflicting program + erase.
    std::array<NORFlashCellReference,2> fcells;
    flash_row_update(fcells,true,false,std::array<bool,2>{true,false},
                     std::array<std::optional<bool>,2>{false,false});
    assert(fcells[0].q == false && fcells[1].q == true);
    FlashBankReference fb(2, 1);
    fb.update({false,true}, {false,false}, {true}, {false});
    assert(fb.columns[0] == false && fb.rows[0][0].q == true);
    fb.update({true,true}, {false,false}, {false}, {false}); assert(!fb.columns[0]);
    fb.update({false,true}, {false,true}, {false}, {false}); assert(fb.columns[0] == true);
    check_native_devices<8>(); check_native_devices<16>(); check_native_devices<32>();
}
