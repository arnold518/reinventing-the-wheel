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

    // Stage durations: settle 32, precharge 32, decode 4, bitline 64, output 8.
    SRAMControlReference control{32, 32, 4, 64, 8};
    assert(!control.at(10, false, true, false).precharge);
    auto stage = control.at(40, false, false, false);
    assert(stage.precharge && !stage.write_enable);
    stage = control.at(66, false, false, false);
    assert(stage.write_enable && !stage.row_enable && !stage.ready);
    stage = control.at(200, false, true, false);
    assert(stage.sense_enable && stage.output_enable && stage.ready && !stage.write_enable);
    assert(!control.at(200, false, true, true).output_enable);
    assert(!control.at(200, true, true, false).ready);

    SRAMReference<32> device(2);
    SRAMReference<32>::Inputs in{0, 0x12345678, 15, false, false, false};
    device.update(control, 66, in); // Interrupted before the row opened: no write.
    assert(device.high_z && word_text(device.words[0]) == std::string(32, 'X'));
    device.update(control, 1000, in);
    assert(device.ready && device.high_z);
    assert(word_text(device.words[0]) == "00010010001101000101011001111000");
    in.data = 0xaabbccdd; in.mask = 5; device.update(control, 1000, in);
    in.we_n = true; device.update(control, 1000, in);
    assert(!device.high_z && word_text(device.read_data) == "00010010101110110101011011011101");
    in.data = 0; in.mask = 0; device.update(control, 1000, in); // Read-mode payload is ignored.
    assert(!device.high_z && word_text(device.read_data) == "00010010101110110101011011011101");
    in.oe_n = true; device.update(control, 1000, in); assert(device.high_z);
    in.oe_n = false; in.address = 2; device.update(control, 1000, in);
    assert(!device.high_z && word_text(device.read_data) == std::string(32, 'X'));
    device.update(control, 100, in); // Still accessing: no data, not ready.
    assert(!device.ready && device.high_z);
}
