#include "test.h"

void test_core(void)
{
    Chip8 c;
    chip8_init(&c);

    CHECK_EQ(c.pc, PC_START, "pc starts at PROG_BASE");
    CHECK_EQ(c.sp, 0, "sp starts empty");
    CHECK(c.rng != 0, "rng never zero");

    CHECK_EQ(c.mem[FONT_BASE], 0xF0, "font head");
    CHECK_EQ(c.mem[FONT_BASE + 4], 0xF0, "font glyph 0 tail");

    const uint8_t rom[] = { 0x12, 0x34, 0x56, 0x78 };
    bool trunc = true;
    CHECK(chip8_load_rom(&c, rom, sizeof rom, &trunc), "valid ROM loads");
    CHECK(!trunc, "small ROM not truncated");
    CHECK_EQ(c.mem[PROG_BASE + 3], 0x78, "ROM lands at PROG_BASE");

    uint8_t big[PROG_MAX + 100];
    memset(big, 0xAA, sizeof big);
    CHECK(chip8_load_rom(&c, big, sizeof big, &trunc), "oversize loads");
    CHECK(trunc, "oversize sets truncation");

    c.pc = PROG_BASE;
    c.mem[PROG_BASE] = 0xAB;
    c.mem[PROG_BASE + 1] = 0xCD;
    CHECK_EQ(chip8_fetch(&c), 0xABCD, "fetch big-endian");

    uint16_t pc = c.pc;
    c.regs[0] = 0xAB;
    step(&c, 0x9ABC);
    CHECK_EQ(c.pc, (pc + 2) & 0x0FFF, "unknown opcode advances");
    CHECK_EQ(c.regs[0], 0xAB, "unknown opcode inert");
}
