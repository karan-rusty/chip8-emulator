#include "test.h"
#include "disasm.h"

static const char *dis(uint16_t op)
{
    static char buf[32];
    Chip8 c;

    chip8_init(&c);
    c.mem[0x200] = (uint8_t)(op >> 8);
    c.mem[0x201] = (uint8_t)(op & 0xFF);
    disasm_line(&c, 0x200, buf, sizeof buf);
    return buf;
}

static void eq(uint16_t op, const char *want)
{
    const char *got = dis(op);

    CHECK(strcmp(got, want) == 0, "disasm %04X: got '%s', want '%s'",
          op, got, want);
}

void test_disasm(void)
{
    eq(0x00E0, "CLS");
    eq(0x1ABC, "JP   ABC");
    eq(0x6A07, "LD   VA, 07");
    eq(0x8124, "ADD  V1, V2");
    eq(0xD015, "DRW  V0, V1, 5");
    eq(0xE19E, "SKP  V1");
    eq(0xF033, "BCD  V0");
    eq(0xF055, "LD   [I], V0");
    eq(0x8129, "DW   8129");
    eq(0xF000, "DW   F000");
}
