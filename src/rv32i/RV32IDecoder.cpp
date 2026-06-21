#include "rv32i/RV32IDecoder.hpp"

#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

namespace rv32i {
namespace {
constexpr uint8_t OPCODE_LOAD = 0x03;
constexpr uint8_t OPCODE_MISC_MEM = 0x0F;
constexpr uint8_t OPCODE_OP_IMM = 0x13;
constexpr uint8_t OPCODE_AUIPC = 0x17;
constexpr uint8_t OPCODE_STORE = 0x23;
constexpr uint8_t OPCODE_OP = 0x33;
constexpr uint8_t OPCODE_LUI = 0x37;
constexpr uint8_t OPCODE_BRANCH = 0x63;
constexpr uint8_t OPCODE_JALR = 0x67;
constexpr uint8_t OPCODE_JAL = 0x6F;
constexpr uint8_t OPCODE_SYSTEM = 0x73;

constexpr uint8_t OPCODE_OP_IMM_32 = 0x1B;
constexpr uint8_t OPCODE_OP_32 = 0x3B;

uint32_t bits(uint32_t value, unsigned high, unsigned low) {
    const uint32_t width = high - low + 1;
    return (value >> low) & ((uint32_t{1} << width) - 1);
}

RV32IDecodedInstruction baseDecode(uint32_t raw) {
    RV32IDecodedInstruction decoded;
    decoded.raw = raw;
    decoded.opcode = RV32IDecoder::opcode(raw);
    decoded.rd = RV32IDecoder::rd(raw);
    decoded.funct3 = RV32IDecoder::funct3(raw);
    decoded.rs1 = RV32IDecoder::rs1(raw);
    decoded.rs2 = RV32IDecoder::rs2(raw);
    decoded.funct7 = RV32IDecoder::funct7(raw);
    decoded.imm12 = RV32IDecoder::imm12(raw);
    decoded.shamt = RV32IDecoder::shamt(raw);
    return decoded;
}

RV32IDecodedInstruction legal(uint32_t raw, RV32IInstruction instruction, RV32IFormat format, int32_t immediate = 0) {
    auto decoded = baseDecode(raw);
    decoded.instruction = instruction;
    decoded.format = format;
    decoded.status = RV32IDecodeStatus::Legal;
    decoded.legal = true;
    decoded.immediate = immediate;
    return decoded;
}

RV32IDecodedInstruction invalid(uint32_t raw, RV32IDecodeStatus status) {
    auto decoded = baseDecode(raw);
    decoded.status = status;
    return decoded;
}

RV32IDecodedInstruction decodeOp(uint32_t raw) {
    switch (RV32IDecoder::funct3(raw)) {
        case 0x0:
            if (RV32IDecoder::funct7(raw) == 0x00) return legal(raw, RV32IInstruction::ADD, RV32IFormat::R);
            if (RV32IDecoder::funct7(raw) == 0x20) return legal(raw, RV32IInstruction::SUB, RV32IFormat::R);
            return invalid(raw, RV32IDecodeStatus::UnsupportedFunct7);
        case 0x1:
            if (RV32IDecoder::funct7(raw) == 0x00) return legal(raw, RV32IInstruction::SLL, RV32IFormat::R);
            return invalid(raw, RV32IDecodeStatus::UnsupportedFunct7);
        case 0x2:
            if (RV32IDecoder::funct7(raw) == 0x00) return legal(raw, RV32IInstruction::SLT, RV32IFormat::R);
            return invalid(raw, RV32IDecodeStatus::UnsupportedFunct7);
        case 0x3:
            if (RV32IDecoder::funct7(raw) == 0x00) return legal(raw, RV32IInstruction::SLTU, RV32IFormat::R);
            return invalid(raw, RV32IDecodeStatus::UnsupportedFunct7);
        case 0x4:
            if (RV32IDecoder::funct7(raw) == 0x00) return legal(raw, RV32IInstruction::XOR, RV32IFormat::R);
            return invalid(raw, RV32IDecodeStatus::UnsupportedFunct7);
        case 0x5:
            if (RV32IDecoder::funct7(raw) == 0x00) return legal(raw, RV32IInstruction::SRL, RV32IFormat::R);
            if (RV32IDecoder::funct7(raw) == 0x20) return legal(raw, RV32IInstruction::SRA, RV32IFormat::R);
            return invalid(raw, RV32IDecodeStatus::UnsupportedFunct7);
        case 0x6:
            if (RV32IDecoder::funct7(raw) == 0x00) return legal(raw, RV32IInstruction::OR, RV32IFormat::R);
            return invalid(raw, RV32IDecodeStatus::UnsupportedFunct7);
        case 0x7:
            if (RV32IDecoder::funct7(raw) == 0x00) return legal(raw, RV32IInstruction::AND, RV32IFormat::R);
            return invalid(raw, RV32IDecodeStatus::UnsupportedFunct7);
        default:
            return invalid(raw, RV32IDecodeStatus::UnsupportedFunct3);
    }
}

RV32IDecodedInstruction decodeOpImm(uint32_t raw) {
    switch (RV32IDecoder::funct3(raw)) {
        case 0x0: return legal(raw, RV32IInstruction::ADDI, RV32IFormat::I, RV32IDecoder::immediateI(raw));
        case 0x2: return legal(raw, RV32IInstruction::SLTI, RV32IFormat::I, RV32IDecoder::immediateI(raw));
        case 0x3: return legal(raw, RV32IInstruction::SLTIU, RV32IFormat::I, RV32IDecoder::immediateI(raw));
        case 0x4: return legal(raw, RV32IInstruction::XORI, RV32IFormat::I, RV32IDecoder::immediateI(raw));
        case 0x6: return legal(raw, RV32IInstruction::ORI, RV32IFormat::I, RV32IDecoder::immediateI(raw));
        case 0x7: return legal(raw, RV32IInstruction::ANDI, RV32IFormat::I, RV32IDecoder::immediateI(raw));
        case 0x1:
            if (RV32IDecoder::funct7(raw) == 0x00) return legal(raw, RV32IInstruction::SLLI, RV32IFormat::I, RV32IDecoder::immediateI(raw));
            return invalid(raw, RV32IDecodeStatus::UnsupportedFunct7);
        case 0x5:
            if (RV32IDecoder::funct7(raw) == 0x00) return legal(raw, RV32IInstruction::SRLI, RV32IFormat::I, RV32IDecoder::immediateI(raw));
            if (RV32IDecoder::funct7(raw) == 0x20) return legal(raw, RV32IInstruction::SRAI, RV32IFormat::I, RV32IDecoder::immediateI(raw));
            return invalid(raw, RV32IDecodeStatus::UnsupportedFunct7);
        default:
            return invalid(raw, RV32IDecodeStatus::UnsupportedFunct3);
    }
}

RV32IDecodedInstruction decodeLoad(uint32_t raw) {
    switch (RV32IDecoder::funct3(raw)) {
        case 0x0: return legal(raw, RV32IInstruction::LB, RV32IFormat::I, RV32IDecoder::immediateI(raw));
        case 0x1: return legal(raw, RV32IInstruction::LH, RV32IFormat::I, RV32IDecoder::immediateI(raw));
        case 0x2: return legal(raw, RV32IInstruction::LW, RV32IFormat::I, RV32IDecoder::immediateI(raw));
        case 0x4: return legal(raw, RV32IInstruction::LBU, RV32IFormat::I, RV32IDecoder::immediateI(raw));
        case 0x5: return legal(raw, RV32IInstruction::LHU, RV32IFormat::I, RV32IDecoder::immediateI(raw));
        default: return invalid(raw, RV32IDecodeStatus::UnsupportedFunct3);
    }
}

RV32IDecodedInstruction decodeStore(uint32_t raw) {
    switch (RV32IDecoder::funct3(raw)) {
        case 0x0: return legal(raw, RV32IInstruction::SB, RV32IFormat::S, RV32IDecoder::immediateS(raw));
        case 0x1: return legal(raw, RV32IInstruction::SH, RV32IFormat::S, RV32IDecoder::immediateS(raw));
        case 0x2: return legal(raw, RV32IInstruction::SW, RV32IFormat::S, RV32IDecoder::immediateS(raw));
        default: return invalid(raw, RV32IDecodeStatus::UnsupportedFunct3);
    }
}

RV32IDecodedInstruction decodeBranch(uint32_t raw) {
    switch (RV32IDecoder::funct3(raw)) {
        case 0x0: return legal(raw, RV32IInstruction::BEQ, RV32IFormat::B, RV32IDecoder::immediateB(raw));
        case 0x1: return legal(raw, RV32IInstruction::BNE, RV32IFormat::B, RV32IDecoder::immediateB(raw));
        case 0x4: return legal(raw, RV32IInstruction::BLT, RV32IFormat::B, RV32IDecoder::immediateB(raw));
        case 0x5: return legal(raw, RV32IInstruction::BGE, RV32IFormat::B, RV32IDecoder::immediateB(raw));
        case 0x6: return legal(raw, RV32IInstruction::BLTU, RV32IFormat::B, RV32IDecoder::immediateB(raw));
        case 0x7: return legal(raw, RV32IInstruction::BGEU, RV32IFormat::B, RV32IDecoder::immediateB(raw));
        default: return invalid(raw, RV32IDecodeStatus::UnsupportedFunct3);
    }
}

RV32IDecodedInstruction decodeSystem(uint32_t raw) {
    if (RV32IDecoder::funct3(raw) != 0x0) {
        return invalid(raw, RV32IDecodeStatus::UnsupportedExtension);
    }
    if (raw == 0x00000073U) {
        return legal(raw, RV32IInstruction::ECALL, RV32IFormat::I, 0);
    }
    if (raw == 0x00100073U) {
        return legal(raw, RV32IInstruction::EBREAK, RV32IFormat::I, 1);
    }
    return invalid(raw, RV32IDecodeStatus::UnsupportedSystem);
}

std::string reg(uint8_t index) {
    return "x" + std::to_string(index);
}

std::string lower(std::string_view value) {
    std::string result(value);
    for (auto& ch : result) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return result;
}

std::string hex32(uint32_t value) {
    std::ostringstream out;
    out << "0x" << std::hex << std::setfill('0') << std::setw(8) << value;
    return out.str();
}

std::string imm(int32_t value) {
    return std::to_string(value);
}
}

RV32IDecodedInstruction RV32IDecoder::decode(uint32_t raw) {
    if ((raw & 0x3U) != 0x3U) {
        return invalid(raw, RV32IDecodeStatus::InvalidInstructionLength);
    }

    switch (opcode(raw)) {
        case OPCODE_LUI: return legal(raw, RV32IInstruction::LUI, RV32IFormat::U, immediateU(raw));
        case OPCODE_AUIPC: return legal(raw, RV32IInstruction::AUIPC, RV32IFormat::U, immediateU(raw));
        case OPCODE_JAL: return legal(raw, RV32IInstruction::JAL, RV32IFormat::J, immediateJ(raw));
        case OPCODE_JALR:
            if (funct3(raw) == 0x0) return legal(raw, RV32IInstruction::JALR, RV32IFormat::I, immediateI(raw));
            return invalid(raw, RV32IDecodeStatus::UnsupportedFunct3);
        case OPCODE_BRANCH: return decodeBranch(raw);
        case OPCODE_LOAD: return decodeLoad(raw);
        case OPCODE_STORE: return decodeStore(raw);
        case OPCODE_OP_IMM: return decodeOpImm(raw);
        case OPCODE_OP: return decodeOp(raw);
        case OPCODE_MISC_MEM:
            if (funct3(raw) == 0x0) return legal(raw, RV32IInstruction::FENCE, RV32IFormat::I, immediateI(raw));
            return invalid(raw, RV32IDecodeStatus::UnsupportedExtension);
        case OPCODE_SYSTEM: return decodeSystem(raw);
        case OPCODE_OP_IMM_32:
        case OPCODE_OP_32:
            return invalid(raw, RV32IDecodeStatus::UnsupportedExtension);
        default:
            return invalid(raw, RV32IDecodeStatus::UnknownOpcode);
    }
}

uint8_t RV32IDecoder::opcode(uint32_t raw) {
    return static_cast<uint8_t>(raw & 0x7FU);
}

uint8_t RV32IDecoder::rd(uint32_t raw) {
    return static_cast<uint8_t>(bits(raw, 11, 7));
}

uint8_t RV32IDecoder::funct3(uint32_t raw) {
    return static_cast<uint8_t>(bits(raw, 14, 12));
}

uint8_t RV32IDecoder::rs1(uint32_t raw) {
    return static_cast<uint8_t>(bits(raw, 19, 15));
}

uint8_t RV32IDecoder::rs2(uint32_t raw) {
    return static_cast<uint8_t>(bits(raw, 24, 20));
}

uint8_t RV32IDecoder::funct7(uint32_t raw) {
    return static_cast<uint8_t>(bits(raw, 31, 25));
}

uint16_t RV32IDecoder::imm12(uint32_t raw) {
    return static_cast<uint16_t>(bits(raw, 31, 20));
}

uint8_t RV32IDecoder::shamt(uint32_t raw) {
    return static_cast<uint8_t>(bits(raw, 24, 20));
}

int32_t RV32IDecoder::signExtend(uint32_t value, unsigned bit_count) {
    if (bit_count == 0 || bit_count >= 32) {
        return static_cast<int32_t>(value);
    }
    const uint32_t sign_bit = uint32_t{1} << (bit_count - 1);
    const uint32_t mask = (uint32_t{1} << bit_count) - 1;
    value &= mask;
    if ((value & sign_bit) == 0) {
        return static_cast<int32_t>(value);
    }
    return static_cast<int32_t>(value | ~mask);
}

int32_t RV32IDecoder::immediateI(uint32_t raw) {
    return signExtend(bits(raw, 31, 20), 12);
}

int32_t RV32IDecoder::immediateS(uint32_t raw) {
    const uint32_t value = (bits(raw, 31, 25) << 5) | bits(raw, 11, 7);
    return signExtend(value, 12);
}

int32_t RV32IDecoder::immediateB(uint32_t raw) {
    const uint32_t value = (bits(raw, 31, 31) << 12)
                         | (bits(raw, 7, 7) << 11)
                         | (bits(raw, 30, 25) << 5)
                         | (bits(raw, 11, 8) << 1);
    return signExtend(value, 13);
}

int32_t RV32IDecoder::immediateU(uint32_t raw) {
    return static_cast<int32_t>(raw & 0xFFFFF000U);
}

int32_t RV32IDecoder::immediateJ(uint32_t raw) {
    const uint32_t value = (bits(raw, 31, 31) << 20)
                         | (bits(raw, 19, 12) << 12)
                         | (bits(raw, 20, 20) << 11)
                         | (bits(raw, 30, 21) << 1);
    return signExtend(value, 21);
}

std::string RV32IDecoder::disassemble(uint32_t raw, uint32_t pc) {
    const auto decoded = decode(raw);
    if (!decoded.legal) {
        return "invalid " + std::string(toString(decoded.status));
    }

    const auto op = lower(toString(decoded.instruction));
    switch (decoded.instruction) {
        case RV32IInstruction::ADD:
        case RV32IInstruction::SUB:
        case RV32IInstruction::SLL:
        case RV32IInstruction::SLT:
        case RV32IInstruction::SLTU:
        case RV32IInstruction::XOR:
        case RV32IInstruction::SRL:
        case RV32IInstruction::SRA:
        case RV32IInstruction::OR:
        case RV32IInstruction::AND:
            return op + " " + reg(decoded.rd) + ", " + reg(decoded.rs1) + ", " + reg(decoded.rs2);

        case RV32IInstruction::SLLI:
        case RV32IInstruction::SRLI:
        case RV32IInstruction::SRAI:
            return op + " " + reg(decoded.rd) + ", " + reg(decoded.rs1) + ", " + std::to_string(decoded.shamt);

        case RV32IInstruction::ADDI:
        case RV32IInstruction::SLTI:
        case RV32IInstruction::SLTIU:
        case RV32IInstruction::XORI:
        case RV32IInstruction::ORI:
        case RV32IInstruction::ANDI:
            return op + " " + reg(decoded.rd) + ", " + reg(decoded.rs1) + ", " + imm(decoded.immediate);

        case RV32IInstruction::LB:
        case RV32IInstruction::LH:
        case RV32IInstruction::LW:
        case RV32IInstruction::LBU:
        case RV32IInstruction::LHU:
            return op + " " + reg(decoded.rd) + ", " + imm(decoded.immediate) + "(" + reg(decoded.rs1) + ")";

        case RV32IInstruction::SB:
        case RV32IInstruction::SH:
        case RV32IInstruction::SW:
            return op + " " + reg(decoded.rs2) + ", " + imm(decoded.immediate) + "(" + reg(decoded.rs1) + ")";

        case RV32IInstruction::BEQ:
        case RV32IInstruction::BNE:
        case RV32IInstruction::BLT:
        case RV32IInstruction::BGE:
        case RV32IInstruction::BLTU:
        case RV32IInstruction::BGEU:
            return op + " " + reg(decoded.rs1) + ", " + reg(decoded.rs2) + ", "
                 + hex32(pc + static_cast<uint32_t>(decoded.immediate));

        case RV32IInstruction::JAL:
            return op + " " + reg(decoded.rd) + ", " + hex32(pc + static_cast<uint32_t>(decoded.immediate));

        case RV32IInstruction::JALR:
            return op + " " + reg(decoded.rd) + ", " + imm(decoded.immediate) + "(" + reg(decoded.rs1) + ")";

        case RV32IInstruction::LUI:
        case RV32IInstruction::AUIPC:
            return op + " " + reg(decoded.rd) + ", " + hex32(static_cast<uint32_t>(decoded.immediate));

        case RV32IInstruction::FENCE:
            return "fence";
        case RV32IInstruction::ECALL:
            return "ecall";
        case RV32IInstruction::EBREAK:
            return "ebreak";
        case RV32IInstruction::INVALID:
            break;
    }
    return "invalid";
}

std::string_view toString(RV32IFormat format) {
    switch (format) {
        case RV32IFormat::R: return "R";
        case RV32IFormat::I: return "I";
        case RV32IFormat::S: return "S";
        case RV32IFormat::B: return "B";
        case RV32IFormat::U: return "U";
        case RV32IFormat::J: return "J";
        case RV32IFormat::Invalid: return "Invalid";
    }
    return "Invalid";
}

std::string_view toString(RV32IInstruction instruction) {
    switch (instruction) {
        case RV32IInstruction::ADD: return "ADD";
        case RV32IInstruction::SUB: return "SUB";
        case RV32IInstruction::SLL: return "SLL";
        case RV32IInstruction::SLT: return "SLT";
        case RV32IInstruction::SLTU: return "SLTU";
        case RV32IInstruction::XOR: return "XOR";
        case RV32IInstruction::SRL: return "SRL";
        case RV32IInstruction::SRA: return "SRA";
        case RV32IInstruction::OR: return "OR";
        case RV32IInstruction::AND: return "AND";
        case RV32IInstruction::ADDI: return "ADDI";
        case RV32IInstruction::SLTI: return "SLTI";
        case RV32IInstruction::SLTIU: return "SLTIU";
        case RV32IInstruction::XORI: return "XORI";
        case RV32IInstruction::ORI: return "ORI";
        case RV32IInstruction::ANDI: return "ANDI";
        case RV32IInstruction::SLLI: return "SLLI";
        case RV32IInstruction::SRLI: return "SRLI";
        case RV32IInstruction::SRAI: return "SRAI";
        case RV32IInstruction::LB: return "LB";
        case RV32IInstruction::LH: return "LH";
        case RV32IInstruction::LW: return "LW";
        case RV32IInstruction::LBU: return "LBU";
        case RV32IInstruction::LHU: return "LHU";
        case RV32IInstruction::SB: return "SB";
        case RV32IInstruction::SH: return "SH";
        case RV32IInstruction::SW: return "SW";
        case RV32IInstruction::BEQ: return "BEQ";
        case RV32IInstruction::BNE: return "BNE";
        case RV32IInstruction::BLT: return "BLT";
        case RV32IInstruction::BGE: return "BGE";
        case RV32IInstruction::BLTU: return "BLTU";
        case RV32IInstruction::BGEU: return "BGEU";
        case RV32IInstruction::JAL: return "JAL";
        case RV32IInstruction::JALR: return "JALR";
        case RV32IInstruction::LUI: return "LUI";
        case RV32IInstruction::AUIPC: return "AUIPC";
        case RV32IInstruction::FENCE: return "FENCE";
        case RV32IInstruction::ECALL: return "ECALL";
        case RV32IInstruction::EBREAK: return "EBREAK";
        case RV32IInstruction::INVALID: return "INVALID";
    }
    return "INVALID";
}

std::string_view toString(RV32IDecodeStatus status) {
    switch (status) {
        case RV32IDecodeStatus::Legal: return "Legal";
        case RV32IDecodeStatus::InvalidInstructionLength: return "InvalidInstructionLength";
        case RV32IDecodeStatus::UnknownOpcode: return "UnknownOpcode";
        case RV32IDecodeStatus::UnsupportedFunct3: return "UnsupportedFunct3";
        case RV32IDecodeStatus::UnsupportedFunct7: return "UnsupportedFunct7";
        case RV32IDecodeStatus::UnsupportedSystem: return "UnsupportedSystem";
        case RV32IDecodeStatus::UnsupportedExtension: return "UnsupportedExtension";
    }
    return "UnknownOpcode";
}

}
