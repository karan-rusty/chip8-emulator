#include "test.h"

void test_font_draw(void)
{
    Chip8 c;
    chip8_init(&c);
    c.regs[0] = 10;
    c.regs[1] = 4;
    c.regs[2] = 0;

    step(&c, 0xF229);
    CHECK_EQ(c.i, FONT_BASE, "digit 0 should be at the font base");

    step(&c, 0xD015);

    CHECK_EQ(lit(&c), 14, "glyph '0' should light 14 pixels");

    static const uint8_t rows[5] = { 0xF0, 0x90, 0x90, 0x90, 0xF0 };
    for (int r = 0; r < 5; r++)
        for (int b = 0; b < 8; b++)
            CHECK_EQ(c.fb[(4 + r) * FB_W + 10 + b], (rows[r] >> (7 - b)) & 1,
                     "glyph '0' pixel at row %d bit %d", r, b);

    for (int b = 4; b < 8; b++)
        CHECK(!c.fb[4 * FB_W + 10 + b], "column %d is outside a 4-wide glyph", b);

    Chip8 d;
    chip8_init(&d);
    d.regs[0] = 10;
    d.regs[1] = 4;
    d.regs[2] = 1;
    step(&d, 0xF229);
    CHECK_EQ(d.i, FONT_BASE + FONT_H, "digit 1 should be one glyph along");
    step(&d, 0xD015);

    CHECK_EQ(lit(&d), 8, "glyph '1' should light 8 pixels");
}

void test_draw(void)
{
    Chip8 c;
    chip8_init(&c);

    c.regs[0] = 16;
    c.regs[1] = 8;
    c.i = SCRATCH;
    c.mem[SCRATCH + 0] = 0xFF;

    step(&c, 0xD011);
    CHECK_EQ(lit(&c), 8, "Dxn draws n rows of 8 pixels");
    for (int b = 0; b < 8; b++)
        CHECK(c.fb[8 * FB_W + 16 + b], "drawn pixel %d should be set", b);

    step(&c, 0xD011);
    CHECK_EQ(lit(&c), 0, "drawing the same sprite twice must XOR it away");

    Chip8 d;
    chip8_init(&d);
    d.regs[0] = 0;
    d.regs[1] = 0;
    d.i = SCRATCH;
    d.mem[SCRATCH] = 0xFF;
    uint16_t pc_before = d.pc;
    step(&d, 0xD000);
    CHECK_EQ(lit(&d), 0, "Dx0 draws nothing");
    CHECK_EQ(d.pc, pc_before + 2, "Dx0 still advances pc by 2");
}

void test_draw_row_count_is_a_nibble(void)
{
    Chip8 c;
    chip8_init(&c);

    c.regs[0] = 0;
    c.regs[1] = 0;
    c.i = SCRATCH;
    for (int r = 0; r < 16; r++)
        c.mem[SCRATCH + r] = 0xFF;

    step(&c, 0xD114);
    CHECK_EQ(lit(&c), 4 * 8, "D114 must draw 4 rows, not 276");

    Chip8 d;
    chip8_init(&d);
    d.i = SCRATCH;
    for (int r = 0; r < 16; r++)
        d.mem[SCRATCH + r] = 0xFF;

    step(&d, 0xD11F);
    CHECK_EQ(lit(&d), 15 * 8, "D11F must draw 15 rows");
    CHECK(!d.fb[15 * FB_W], "row 15 is outside N=15 and must not be drawn");
}

void test_draw_wraps(void)
{
    Chip8 c;
    chip8_init(&c);

    c.regs[0] = FB_W;
    c.regs[1] = FB_H;
    c.i = SCRATCH;
    c.mem[SCRATCH] = 0x80;

    step(&c, 0xD011);
    CHECK(c.fb[0], "pixel past the bottom-right corner wraps to the origin");
    CHECK_EQ(lit(&c), 1, "wrapping draw should light exactly one pixel");
}

void test_draw_clips(void)
{
    Chip8 c;
    chip8_init(&c);
    c.regs[0] = FB_W - 1;
    c.regs[1] = FB_H - 1;
    c.i = SCRATCH;
    c.mem[SCRATCH] = 0xFF;

    step(&c, 0xD011);
    CHECK_EQ(lit(&c), 1, "a sprite at the far corner is clipped to one pixel");
    CHECK(c.fb[(FB_H - 1) * FB_W + FB_W - 1], "that pixel lands in place");
    CHECK(!c.fb[FB_W - 1], "nothing wraps back up to the first row");

    Chip8 d;
    chip8_init(&d);
    d.regs[0] = 0;
    d.regs[1] = FB_H - 1;
    d.i = SCRATCH;
    d.mem[SCRATCH] = 0xFF;
    d.mem[SCRATCH + 1] = 0xFF;
    step(&d, 0xD012);
    CHECK_EQ(lit(&d), 8, "the row past the bottom edge is dropped");
    CHECK(!d.fb[0] && !d.fb[1], "the dropped row does not reappear on top");

    Chip8 e;
    chip8_init(&e);
    e.regs[0] = FB_W - 3;
    e.regs[1] = 0;
    e.i = SCRATCH;
    e.mem[SCRATCH] = 0xFF;
    step(&e, 0xD011);
    CHECK_EQ(lit(&e), 3, "the columns past the right edge are dropped");
}

void test_clear(void)
{
    Chip8 c;
    chip8_init(&c);
    c.fb[0] = true;
    c.fb[FB_SIZE - 1] = true;
    c.regs[1] = 0x42;
    uint16_t pc = c.pc;

    step(&c, 0x00E0);
    CHECK_EQ(lit(&c), 0, "00E0 clears every pixel");
    CHECK_EQ(c.pc, pc + 2, "00E0 still advances pc");
    CHECK_EQ(c.regs[1], 0x42, "00E0 does not touch registers");
}

void test_draw_sets_collision(void)
{
    Chip8 c;
    chip8_init(&c);
    c.regs[0] = 0;
    c.regs[1] = 0;
    c.i = SCRATCH;
    c.mem[SCRATCH] = 0xFF;

    step(&c, 0xD011);
    CHECK_EQ(c.regs[0xF], 0, "a draw over empty pixels clears VF");

    step(&c, 0xD011);
    CHECK_EQ(c.regs[0xF], 1, "erasing pixels sets VF");
    CHECK_EQ(lit(&c), 0, "the second draw XORed the sprite away");

    Chip8 d;
    chip8_init(&d);
    d.regs[0] = 0;
    d.regs[1] = 0;
    d.i = SCRATCH;
    d.mem[SCRATCH] = 0xF0;
    step(&d, 0xD011);
    CHECK_EQ(d.regs[0xF], 0, "the first draw finds nothing underneath");

    d.regs[0] = 4;
    step(&d, 0xD011);
    CHECK_EQ(d.regs[0xF], 0, "a sprite four pixels away does not collide");

    d.regs[0] = 2;
    step(&d, 0xD011);
    CHECK_EQ(d.regs[0xF], 1, "a partly overlapping sprite collides");
}

void test_schip_draw_forms(void)
{
    Chip8 c;
    chip8_init(&c);
    c.regs[0] = 0;
    c.regs[1] = 0;
    c.i = 0x100;
    for (int r = 0; r < 16; r++)
        c.mem[SCRATCH + r] = 0xFF;

    step(&c, 0xD010);
    CHECK_EQ(lit(&c), 0, "Dx0 is an unsupported SCHIP form and draws nothing");

    Chip8 d;
    chip8_init(&d);
    d.regs[0] = 0;
    d.regs[1] = 0;
    d.i = SCRATCH;
    for (int r = 0; r < 16; r++)
        d.mem[SCRATCH + r] = 0xFF;

    step(&d, 0xD1C4);
    CHECK_EQ(lit(&d), 4 * 8, "DxC4 is unsupported and drawn 8 pixels wide");
}
