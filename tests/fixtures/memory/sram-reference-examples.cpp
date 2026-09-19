// Compiled after the exact twelve SRAM documentation snippets.
#include <cassert>
#include <string>

std::string word_text(const std::vector<std::optional<bool>>& bits, bool high_z = false) {
    if (high_z) return std::string(bits.size(), 'Z');
    std::string out;
    for (auto it = bits.rbegin(); it != bits.rend(); ++it)
        out += !it->has_value() ? 'X' : **it ? '1' : '0';
    return out;
}

void check_sram_references() {
    SRAMCellReference cell;
    assert(!cell.q && !cell.q_bar());
    cell.update(true, true, true, false); assert(cell.q == true && cell.q_bar() == false);
    cell.update(false, true, false, true); assert(cell.q == true);
    cell.update(true, false, false, true); assert(cell.q == true);
    cell.update(true, true, true, true); assert(!cell.q);
    cell.update(true, true, false, true); assert(cell.q == false);
    SRAMRowReference row(2);
    row.update(true, {true, false}, {true, true}, {false, false});
    assert(row.q(0) == true && !row.q(1));
    SRAMBitlineReference pair;
    pair.update(true, false, false, true, {false, false}, {false, true});
    assert(pair.bl == true && pair.bar == true && !pair.valid && !pair.cell_drive);
    pair.update(false, false, false, true, {true, false}, {false, true});
    assert(pair.bl == false && pair.bar == true && pair.valid);
    pair.update(false, false, false, true, {false, false}, {false, true});
    pair.update(false, false, false, true, {false, true}, {false, true});
    assert(!pair.bl && !pair.valid); // A used read pair requires precharge.
    for (unsigned precharge = 0; precharge < 2; ++precharge)
        for (unsigned drive = 0; drive < 2; ++drive)
            for (unsigned rows = 0; rows < 4; ++rows) {
                pair.update(precharge, drive, true, false,
                            {bool(rows & 1), bool(rows & 2)}, {false, true});
                if (rows == 3 || (precharge && (drive || rows)))
                    assert(!pair.bl && !pair.bar && !pair.cell_drive && !pair.valid);
                else if (drive) assert(pair.cell_drive && pair.cell_bl == true && pair.cell_bar == false);
            }
    SRAMBankReference bank(2, 2);
    bank.update({false, true}, {false, false}, {true, false}, {true, true}, {false, false});
    assert(bank.rows[1].q(0) == true && !bank.rows[0].q(0) && !bank.rows[1].q(1));
    bank.update({false, false}, {true, true}, {false, false}, {false, false}, {true, true});
    bank.update({false, true}, {false, false}, {false, false}, {false, false}, {true, true});
    assert(bank.columns[0].bl == true && bank.columns[0].valid && !bank.columns[1].valid);
    assert(sram_rows_ref(3, true, 6, 2) == std::vector<bool>({false, true, false}));
    assert(sram_precharge_ref(true, 2) == std::vector<bool>({true, true}));
    auto driver = sram_drive_ref(false, true, true);
    assert(!driver.bl && driver.bar && driver.drive);
    assert(sram_sense_ref(true, false, true, true) == true);
    assert(!sram_sense_ref(true, true, false, true));
    std::vector<SRAMBitlineReference> columns(4);
    columns[2].bl = true; columns[2].bar = false; columns[2].valid = true;
    auto selected = sram_select_ref(1, 2, 2, columns, {false, true}, {true, false}, {true, false});
    assert(selected.read_bl[0] == true && selected.read_bar[0] == false && selected.read_valid[0]);
    assert(selected.drive == std::vector<bool>({false, false, true, false}));
    assert(selected.drive_bl[0] == false && selected.drive_bl[2] == false);
    assert(!sram_output_ref(5, false) && sram_output_ref(5, true) == 5);

    SRAMReference<32> device(2);
    SRAMControlReference::Inputs in{0, 0x12345678, 15, false, false, false};
    device.update(0, 32, in);
    device.update(32, 32, in); assert(device.control.precharge && device.high_z);
    device.update(64, 32, in); assert(device.control.write_enable && !device.control.row_enable);
    in.ce_n = true; device.update(95, 32, in);
    device.update(96, 32, in); assert(word_text(device.words[0]) == std::string(32, 'X'));
    in.ce_n = false; device.update(100, 32, in);
    for (auto t : {132, 164, 196, 228}) device.update(t, 32, in);
    assert(word_text(device.words[0]) == "00010010001101000101011001111000");
    in.data = 0xaabbccdd; in.mask = 5; device.update(229, 32, in);
    for (auto t : {261, 293, 325, 357}) device.update(t, 32, in);
    in.we_n = true; device.update(358, 32, in);
    for (auto t : {390, 422, 454, 486}) device.update(t, 32, in);
    assert(!device.high_z && word_text(device.read_data) == "00010010101110110101011011011101");
    in.data = 0; in.mask = 0; device.update(487, 32, in);
    assert(!device.control.due && !device.high_z); // Read-mode payload is ignored.
    in.oe_n = true; device.update(488, 32, in); assert(device.high_z && !device.control.due);
    in.oe_n = false; device.update(489, 32, in); assert(!device.high_z && !device.control.due);
    in.address = 2; device.update(490, 32, in);
    for (auto t : {522, 554, 586, 618}) device.update(t, 32, in);
    assert(!device.high_z && word_text(device.read_data) == std::string(32, 'X'));
    // Input change at the deadline wins over the old timer.
    in.address = 0; device.update(620, 32, in);
    in.address = 1; device.update(652, 32, in);
    assert(device.control.phase == SRAMControlReference::Phase::Quiet && device.control.due == 684);
}
