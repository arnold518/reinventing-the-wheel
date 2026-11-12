#include "modules/device/RegisterTable.hpp"
#include "DeviceLogic.hpp"

namespace circuit::device {
using A = RegisterAccess;

RegisterTable deviceRegisterTable(const std::string& contract, const ParameterMap& p) {
    using detail::parameter;
    RegisterTable t;
    if (contract == "device.console") {
        t.device = "CONSOLE";
        t.registers = {
            {"TX", 0x0, A::WriteData, 8, 0, "Queue one byte for the host (bits 7..0)"},
            {"STATUS", 0x4, A::ReadOnly, 1, 0, "Bit 0: the queue has room", {{"ROOM", 0}}},
            {"COUNT", 0x8, A::ReadOnly, 3, 0, "Bytes waiting in the queue"}};
    } else if (contract == "device.timer") {
        t.device = "TIMER";
        t.registers = {
            {"COUNT", 0x0, A::ReadOnly, 32, 0, "Ticks since reset"},
            {"DIVISOR", 0x4, A::Setting, 32, parameter(p, "divisor", 1), "Clock cycles per tick"},
            {"FREQUENCY_HZ", 0x8, A::Setting, 32, parameter(p, "frequency_hz", 0), "Declared clock frequency (0: not declared)"}};
    } else if (contract == "device.buttons") {
        t.device = "BUTTONS";
        t.registers = {
            {"LEVELS", 0x0, A::ReadOnly, 32, 0, "Bit n: button n is held"},
            {"HEAD", 0x4, A::ReadOnly, 32, 0, "Oldest event: button in bits 4..0, bit 8 set for a press", {{"BUTTON", 0, 5}, {"DOWN", 8}}},
            {"COUNT", 0x8, A::ReadOnly, 3, 0, "Events waiting"},
            {"STATUS", 0xc, A::ReadOnly, 2, 0, "Bit 0: an event waits; bit 1: an event was lost", {{"VALID", 0}, {"OVERFLOW", 1}}},
            {"POP", 0x10, A::Command, 1, 0, "Remove the oldest event"},
            {"CLEAR_OVERFLOW", 0x14, A::Command, 1, 0, "Clear the lost-event flag"}};
    } else if (contract == "device.framebuffer") {
        t.device = "DISPLAY";
        const auto width = parameter(p, "width", 4), height = parameter(p, "height", 4);
        t.registers = {
            {"WIDTH", 0x0, A::Setting, 32, width, "Visible pixels per row"},
            {"HEIGHT", 0x4, A::Setting, 32, height, "Visible rows"},
            {"STRIDE", 0x8, A::Setting, 32, width, "Bytes from one row to the next in video memory"},
            {"FORMAT", 0xc, A::Setting, 32, 1, "Pixel format; 1 is RGB332 (one byte per pixel)"},
            {"FRAME_SEQUENCE", 0x10, A::ReadOnly, 32, 0, "Frames presented since reset"},
            {"PRESENT", 0x14, A::Command, 1, 0, "Show the current video memory as a new frame"}};
    } else if (contract.starts_with("device.matrix-unit.")) {
        // Chapter 6 accelerator; the command arguments are documented in § 6.4.
        t.device = "MATRIX";
        const auto size = static_cast<uint32_t>(std::stoul(contract.substr(contract.rfind('.') + 1)));
        t.registers = {
            {"COMMAND", 0x0, A::WriteData, 4, 0, "Queue a command (1 LOAD, 2 STORE, 3 SHIFT_WEIGHTS, 4 MATMUL, 5 ACTIVATE) with ARG0..ARG3"},
            {"ARG0", 0x4, A::ReadWrite, 32, 0, "First argument of the next command"},
            {"ARG1", 0x8, A::ReadWrite, 32, 0, "Second argument of the next command"},
            {"ARG2", 0xc, A::ReadWrite, 32, 0, "Third argument of the next command"},
            {"ARG3", 0x10, A::ReadWrite, 32, 0, "Fourth argument of the next command"},
            {"STATUS", 0x14, A::ReadOnly, 3, 0, "Bit 0: busy; bit 1: a command failed; bit 2: the queue is full",
             {{"BUSY", 0}, {"ERROR", 1}, {"FULL", 2}}},
            {"DONE", 0x18, A::ReadOnly, 32, 0, "Commands finished since reset"},
            {"CLEAR", 0x1c, A::Command, 1, 0, "Clear the error flag"},
            {"SIZE", 0x20, A::Setting, 32, size, "Array rows and columns (N)"},
            {"ACT_ROWS", 0x24, A::Setting, 32, parameter(p, "act_rows", 256), "Rows of the activation buffer (N bytes each)"},
            {"ACC_ROWS", 0x28, A::Setting, 32, parameter(p, "acc_rows", 64), "Rows of the accumulators (N int32 each)"}};
    } else if (contract == "device.leds") {
        t.device = "LEDS";
        t.registers = {
            {"LEDS", 0x0, A::ReadWrite, 16, 0, "Bit n: LED n is on"},
            {"BLINK", 0x4, A::ReadWrite, 5, 0, "0: steady; k: lit LEDs blink, on and off for 2^k cycles each"}};
    } else {
        return {};
    }
    t.validate();
    return t;
}

} // namespace circuit::device
