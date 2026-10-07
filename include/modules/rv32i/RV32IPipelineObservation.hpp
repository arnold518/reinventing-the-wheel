#pragma once

#include "components/IOComponent.hpp"
#include "components/capabilities/RV32IRetirementView.hpp"
#include "modules/rv32i/RV32IPipelineEncoding.hpp"

namespace rv32i {
inline RV32IMemoryTrace observeMemoryRetirement(
    const RV32IControlSignals& control, uint32_t address,
    uint32_t store_data, uint32_t load_data, bool fault) {
    RV32IMemoryTrace trace;
    if (!control.mem_read && !control.mem_write) return trace;
    trace.kind = control.mem_read ? RV32IMemoryAccessKind::Read : RV32IMemoryAccessKind::Write;
    trace.size = control.mem_size;
    trace.sign_extend = control.mem_read && control.load_sign_extend;
    trace.address = address;
    trace.write_data = control.mem_read ? 0 : store_data;
    trace.read_data = control.mem_read ? load_data : 0;
    trace.fault = fault;
    return trace;
}

// Inspect real packet outputs; do not execute the instruction or infer its
// result from the oracle. Also works with behavioral pipeline registers.
inline RV32IRetirementObservation observeRetirementPacket(const IOComponent& packet, bool terminal) {
    const auto word = [&](const char* field) {
        return static_cast<uint32_t>(packet.getOutputPin<32>(field)->getValueAsUInt64());
    };
    return {word("Q_PC"), word("Q_INSTRUCTION"), observeMemoryRetirement(
        pipeline_encoding::unpackControl(word("Q_CONTROL")), word("Q_ALU_RESULT"),
        word("Q_STORE_DATA"), word("Q_MEMORY_DATA"), (word("Q_MEMORY_STATUS") & 3U) != 0), word("Q_NEXT_PC"), terminal};
}
}
