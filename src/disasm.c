#include "disasm.h"

static const char *alu_name(unsigned n)
{
    switch (n) {
    case 0x0: return "LD";
    case 0x1: return "OR";
    case 0x2: return "AND";
    case 0x3: return "XOR";
    case 0x4: return "ADD";
    case 0x5: return "SUB";
    case 0x6: return "SHR";
    case 0x7: return "SUBN";
    case 0xE: return "SHL";
    default:  return NULL;
    }
}

/* F-opcode mnemonics are inlined in disasm_line. */

int disasm_line(const Chip8 *c, uint16_t addr, char *buf, size_t len)
{
    uint16_t op = (uint16_t)((c->mem[addr & 0x0FFF] << 8) |
                              c->mem[(addr + 1) & 0x0FFF]);
    unsigned x   = (op >> 8) & 0xF;
    unsigned y   = (op >> 4) & 0xF;
    unsigned n   = op & 0xF;
    unsigned kk  = op & 0xFF;
    unsigned nnn = op & 0xFFF;

    switch (op & 0xF000) {
    case 0x0000:
        if (op == 0x00E0)      snprintf(buf, len, "CLS");
        else if (op == 0x00EE) snprintf(buf, len, "RET");
        else                   snprintf(buf, len, "SYS  %03X", nnn);
        break;
    case 0x1000: snprintf(buf, len, "JP   %03X", nnn); break;
    case 0x2000: snprintf(buf, len, "CALL %03X", nnn); break;
    case 0x3000: snprintf(buf, len, "SE   V%X, %02X", x, kk); break;
    case 0x4000: snprintf(buf, len, "SNE  V%X, %02X", x, kk); break;
    case 0x5000:
        if (n == 0) snprintf(buf, len, "SE   V%X, V%X", x, y);
        else        snprintf(buf, len, "DW   %04X", op);
        break;
    case 0x6000: snprintf(buf, len, "LD   V%X, %02X", x, kk); break;
    case 0x7000: snprintf(buf, len, "ADD  V%X, %02X", x, kk); break;
    case 0x8000: {
        const char *mn = alu_name(n);
        if (mn) snprintf(buf, len, "%-4s V%X, V%X", mn, x, y);
        else    snprintf(buf, len, "DW   %04X", op);
        break;
    }
    case 0x9000:
        if (n == 0) snprintf(buf, len, "SNE  V%X, V%X", x, y);
        else        snprintf(buf, len, "DW   %04X", op);
        break;
    case 0xA000: snprintf(buf, len, "LD   I, %03X", nnn); break;
    case 0xB000: snprintf(buf, len, "JP   V0, %03X", nnn); break;
    case 0xC000: snprintf(buf, len, "RND  V%X, %02X", x, kk); break;
    case 0xD000: snprintf(buf, len, "DRW  V%X, V%X, %X", x, y, n); break;
    case 0xE000:
        if (kk == 0x9E)      snprintf(buf, len, "SKP  V%X", x);
        else if (kk == 0xA1) snprintf(buf, len, "SKNP V%X", x);
        else                 snprintf(buf, len, "DW   %04X", op);
        break;
    case 0xF000:
        switch (kk) {
        case 0x07: snprintf(buf, len, "LD   V%X, DT", x); break;
        case 0x0A: snprintf(buf, len, "LD   V%X, K", x); break;
        case 0x15: snprintf(buf, len, "LD   DT, V%X", x); break;
        case 0x18: snprintf(buf, len, "LD   ST, V%X", x); break;
        case 0x1E: snprintf(buf, len, "ADD  I, V%X", x); break;
        case 0x29: snprintf(buf, len, "LD   F, V%X", x); break;
        case 0x33: snprintf(buf, len, "BCD  V%X", x); break;
        case 0x55: snprintf(buf, len, "LD   [I], V%X", x); break;
        case 0x65: snprintf(buf, len, "LD   V%X, [I]", x); break;
        default:   snprintf(buf, len, "DW   %04X", op); break;
        }
        break;
    default:
        snprintf(buf, len, "DW   %04X", op);
        break;
    }

    return 2;
}

void disasm_rom(const Chip8 *c, uint16_t base, size_t bytes, FILE *out)
{
    for (size_t off = 0; off + 1 < bytes; off += 2) {
        uint16_t addr = (uint16_t)((base + off) & 0x0FFF);
        char line[32];

        disasm_line(c, addr, line, sizeof line);
        fprintf(out, "%03X  %02X %02X  %s\n", addr,
                c->mem[addr & 0x0FFF], c->mem[(addr + 1) & 0x0FFF], line);
    }
}
