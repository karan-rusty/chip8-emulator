#include "test.h"

void test_init(void)
{
    Chip8 c;
    chip8_init(&c);

    CHECK_EQ(c.pc, PC_START, "pc should start at PROG_BASE");
    CHECK_EQ(c.sp, 0, "sp should start empty");
    CHECK_EQ(c.i, 0, "I should start at 0");
    CHECK_EQ(c.dt, 0, "delay timer should start at 0");
    CHECK_EQ(c.st, 0, "sound timer should start at 0");
    CHECK_EQ(c.mem[PROG_BASE], 0, "program area should start zeroed");

    for (unsigned k = 0; k < NUM_KEYS; k++)
        CHECK(!c.keys[k], "key %u must start up", k);
    CHECK(c.rng != 0, "the xorshift state must never be all zero");
}

void test_font(void)
{
    static const uint8_t expected[FONT_BYTES] = {
        0xF0, 0x90, 0x90, 0x90, 0xF0,
        0x20, 0x60, 0x20, 0x20, 0x70,
        0xF0, 0x10, 0xF0, 0x80, 0xF0,
        0xF0, 0x10, 0xF0, 0x10, 0xF0,
        0x90, 0x90, 0xF0, 0x10, 0x10,
        0xF0, 0x80, 0xF0, 0x10, 0xF0,
        0xF0, 0x80, 0xF0, 0x90, 0xF0,
        0xF0, 0x10, 0x20, 0x40, 0x40,
        0xF0, 0x90, 0xF0, 0x90, 0xF0,
        0xF0, 0x90, 0xF0, 0x10, 0xF0,
        0xF0, 0x90, 0xF0, 0x90, 0x90,
        0xE0, 0x90, 0xE0, 0x90, 0xE0,
        0xF0, 0x80, 0x80, 0x80, 0xF0,
        0xE0, 0x90, 0x90, 0x90, 0xE0,
        0xF0, 0x80, 0xF0, 0x80, 0xF0,
        0xF0, 0x80, 0xF0, 0x80, 0x80,
    };

    Chip8 c;
    chip8_init(&c);

    CHECK_EQ(FONT_BASE, 0x000, "font must start at 0x000 for Fx29 arithmetic");
    CHECK_EQ(FONT_BYTES, 80, "font is 16 glyphs of 5 bytes");

    for (int i = 0; i < FONT_BYTES; i++)
        CHECK_EQ(c.mem[FONT_BASE + i], expected[i],
                 "font byte %d (glyph %d row %d)", i, i / FONT_H, i % FONT_H);

    for (int i = 0; i < FONT_BYTES; i++)
        CHECK_EQ(c.mem[FONT_BASE + i] & 0x0F, 0x0,
                 "font byte %d must leave the low nibble clear", i);

    CHECK_EQ(c.mem[FONT_END], 0x00, "byte after the font must be untouched");
}

void test_font_pointer(void)
{
    for (unsigned d = 0; d < 16; d++) {
        Chip8 c;
        chip8_init(&c);
        c.regs[1] = (uint16_t)d;
        step(&c, 0xF129);
        CHECK_EQ(c.i, FONT_BASE + d * FONT_H,
                 "Fx29 for digit %u should point at glyph %u", d, d);
    }

    static const unsigned cases[] = { 0x10, 0x1F, 0x23, 0xA5, 0xFF };
    for (size_t k = 0; k < sizeof(cases) / sizeof(*cases); k++) {
        Chip8 c;
        chip8_init(&c);
        c.regs[1] = (uint16_t)cases[k];
        step(&c, 0xF129);
        CHECK_EQ(c.i, FONT_BASE + (cases[k] & 0x0F) * FONT_H,
                 "Fx29 must mask VX=0x%02X to its low nibble", cases[k]);
    }

    Chip8 d;
    chip8_init(&d);
    d.regs[0] = 0x99;
    d.regs[7] = 0x03;
    step(&d, 0xF729);
    CHECK_EQ(d.i, FONT_BASE + 3 * FONT_H, "Fx29 must read Vx from the x nibble");
}

void test_load_rom(void)
{
    Chip8 c;
    chip8_init(&c);
    bool trunc = true;

    CHECK(!chip8_load_rom(&c, NULL, 0, &trunc),
          "zero-length ROM must be rejected");

    const uint8_t rom[] = { 0x12, 0x34, 0x56, 0x78 };
    trunc = true;
    CHECK(chip8_load_rom(&c, rom, sizeof(rom), &trunc), "valid ROM loads");
    CHECK(!trunc, "small ROM must not report truncation");
    CHECK_EQ(c.mem[PROG_BASE + 0], 0x12, "ROM byte 0 lands at PROG_BASE");
    CHECK_EQ(c.mem[PROG_BASE + 3], 0x78, "ROM byte 3 lands at PROG_BASE+3");

    uint8_t big[PROG_MAX + 100];
    memset(big, 0xAA, sizeof(big));
    chip8_init(&c);
    trunc = false;
    CHECK(chip8_load_rom(&c, big, sizeof(big), &trunc), "oversized ROM loads");
    CHECK(trunc, "oversized ROM must set the truncation flag");
    CHECK_EQ(c.mem[MEM_SIZE - 1], 0xAA, "last byte should be written");

    chip8_init(&c);
    CHECK(chip8_load_rom(&c, rom, sizeof(rom), NULL),
          "truncated pointer may be NULL");
    CHECK_EQ(c.mem[PROG_BASE], 0x12, "load still happened with NULL flag");
}

void test_fetch(void)
{
    Chip8 c;
    chip8_init(&c);

    c.pc = PROG_BASE;
    c.mem[PROG_BASE] = 0xAB;
    c.mem[PROG_BASE + 1] = 0xCD;
    CHECK_EQ(chip8_fetch(&c), 0xABCD, "fetch is big-endian");

    c.pc = 0x0FFF;
    c.mem[0x0FFF] = 0x11;
    c.mem[0x0000] = 0x22;
    CHECK_EQ(chip8_fetch(&c), 0x1122, "fetch must wrap at 0x0FFF");
}

void test_unknown_opcode_is_inert(void)
{
    Chip8 c;
    chip8_init(&c);

    c.regs[0] = 0xAB;
    uint16_t pc = c.pc;

    step(&c, 0x9ABC);
    CHECK_EQ(c.pc, pc + 2, "unknown opcode still advances pc by 2");
    CHECK_EQ(c.regs[0], 0xAB, "unknown opcode must not change registers");
    CHECK_EQ(lit(&c), 0, "unknown opcode must not touch the framebuffer");
    CHECK_EQ(c.sp, 0, "unknown opcode must not touch the stack");
}

void test_pc_wraps(void)
{
    Chip8 c;
    chip8_init(&c);

    c.pc = 0x0FFE;
    step(&c, 0x0000);
    CHECK_EQ(c.pc, 0x000, "pc must wrap from 0x0FFF back to 0x000");
}
