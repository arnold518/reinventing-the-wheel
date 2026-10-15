#pragma once
#include "modules/memory/MemoryProtocol.hpp"
#include "components/IOComponent.hpp"
#include <string>

namespace circuit::memory_detail {

// The request rules of a delayed memory controller as gates: READ/WRITE (and
// for NOR flash PROGRAM/ERASE_BLOCK) checked for unknown fields, operation,
// write mask, alignment, range and technology permission, with the same
// status precedence as validateRequest. Outputs ACCEPT, STATUS[2:0] and the
// decoded operation, plus WORD_ADDR (the byte address shifted to a word).
class MemoryRequestCheck : public IOComponent {
public:
    MemoryRequestCheck(std::string name, size_t width, circuit::memory::Technology, circuit::memory::EndpointConfig);
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder&) override;
private:
    size_t width_;
    circuit::memory::Technology technology_;
    circuit::memory::EndpointConfig config_;
    std::string type_;
};

// The controller sequencer of an asynchronous static device (SRAM, ROM), as
// gates. Same pins as the native sequencer it replaces: the transaction port
// on one side (START, FINISH, request fields; BACK_READY, DONE, RESULT,
// STATUS) and the device's native pins with MEM_ on the other. START loads the
// request registers; a valid request enters ACCESS, which drives CE/WE/OE and
// the held fields until the device's READY, brought into the clock by a
// Synchronizer, loads the result and status and raises DONE. FINISH clears
// DONE. A new request needs the access released and the synchronized READY
// low again. An invalid request completes at once with its status.
class StaticMemorySequencer : public IOComponent {
public:
    StaticMemorySequencer(std::string name, size_t width, circuit::memory::Technology, circuit::memory::EndpointConfig);
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder&) override;
private:
    size_t width_;
    circuit::memory::Technology technology_;
    circuit::memory::EndpointConfig config_;
    std::string type_;
};

void defineSequencerPins(IOComponent* self, size_t width, circuit::memory::Technology technology);

} // namespace circuit::memory_detail
