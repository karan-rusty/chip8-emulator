/* Tests for the implemented opcode set and the machine invariants that are
 * easy to break silently. Deliberately minimal: only behaviour that a real
 * ROM depends on, plus the specific bugs found during development. */

#include "chip8.h"

#include <stdio.h>
#include <string.h>

static int failures;
static int checks;

#define CHECK(cond, ...)                                                    \
    do {                                                                    \
        checks++;                                                           \
        if (!(cond)) {                                                      \
            failures++;                                                     \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                     \
            printf(__VA_ARGS__);                                            \
            printf("\n");                                                   \
        }                                                                   \
    } while (0)

#define CHECK_EQ(got, want, ...)                                            \
    do {                                                                    \
        checks++;                                                           \
        long g_ = (long)(got), w_ = (long)(want);                           \
        if (g_ != w_) {                                                     \
            failures++;                                                     \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                     \
            printf(__VA_ARGS__);                                            \
            printf(" (got %ld, want %ld)\n", g_, w_);                       \
        }                                                                   \
    } while (0)

/* Load a single opcode at pc and execute it. */
static void step(Chip8 *c, uint16_t opcode)
{
    c->mem[c->pc] = (uint8_t)(opcode >> 8);
    c->mem[(c->pc + 1) & 0x0FFF] = (uint8_t)(opcode & 0xFF);
    chip8_execute(c, opcode);
}

static int lit(const Chip8 *c)
{
    int n = 0;
    for (int i = 0; i < FB_SIZE; i++)
        n += c->fb[i];
    return n;
}

/* Scratch area for sprite data written by tests, clear of the font at
 * 0x000-0x04F and below the program area. */
#define SCRATCH 0x100

static void test_init(void)
{
    Chip8 c;
    chip8_init(&c);

    CHECK_EQ(c.pc, PC_START, "pc should start at PROG_BASE");
    CHECK_EQ(c.sp, 0, "sp should start empty");
    CHECK_EQ(c.i, 0, "I should start at 0");
    CHECK_EQ(c.dt, 0, "delay timer should start at 0");
    CHECK_EQ(c.st, 0, "sound timer should start at 0");
    CHECK_EQ(c.mem[PROG_BASE], 0, "program area should start zeroed");
}

/* The font is the 16 hex digits, 5 bytes each, at 0x000. */
static void test_font(void)
{
    static const uint8_t expected[FONT_BYTES] = {
        0xF0, 0x90, 0x90, 0x90, 0xF0, /* 0 */
        0x20, 0x60, 0x20, 0x20, 0x70, /* 1 */
        0xF0, 0x10, 0xF0, 0x80, 0xF0, /* 2 */
        0xF0, 0x10, 0xF0, 0x10, 0xF0, /* 3 */
        0x90, 0x90, 0xF0, 0x10, 0x10, /* 4 */
        0xF0, 0x80, 0xF0, 0x10, 0xF0, /* 5 */
        0xF0, 0x80, 0xF0, 0x90, 0xF0, /* 6 */
        0xF0, 0x10, 0x20, 0x40, 0x40, /* 7 */
        0xF0, 0x90, 0xF0, 0x90, 0xF0, /* 8 */
        0xF0, 0x90, 0xF0, 0x10, 0xF0, /* 9 */
        0xF0, 0x90, 0xF0, 0x90, 0x90, /* A */
        0xE0, 0x90, 0xE0, 0x90, 0xE0, /* B */
        0xF0, 0x80, 0x80, 0x80, 0xF0, /* C */
        0xE0, 0x90, 0x90, 0x90, 0xE0, /* D */
        0xF0, 0x80, 0xF0, 0x80, 0xF0, /* E */
        0xF0, 0x80, 0xF0, 0x80, 0x80, /* F */
    };

    Chip8 c;
    chip8_init(&c);

    CHECK_EQ(FONT_BASE, 0x000, "font must start at 0x000 for Fx29 arithmetic");
    CHECK_EQ(FONT_BYTES, 80, "font is 16 glyphs of 5 bytes");

    for (int i = 0; i < FONT_BYTES; i++)
        CHECK_EQ(c.mem[FONT_BASE + i], expected[i],
                 "font byte %d (glyph %d row %d)", i, i / FONT_H, i % FONT_H);

    /* Glyphs are 4 pixels wide: the low nibble of every row must be clear. */
    for (int i = 0; i < FONT_BYTES; i++)
        CHECK_EQ(c.mem[FONT_BASE + i] & 0x0F, 0x0,
                 "font byte %d must leave the low nibble clear", i);

    /* Nothing may be written past the font. */
    CHECK_EQ(c.mem[FONT_END], 0x00, "byte after the font must be untouched");
}

/* Fx29 points I at the glyph for the low hex digit of VX. */
static void test_font_pointer(void)
{
    for (unsigned d = 0; d < 16; d++) {
        Chip8 c;
        chip8_init(&c);
        c.regs[1] = (uint16_t)d;
        step(&c, 0xF129); /* Fx29 */
        CHECK_EQ(c.i, FONT_BASE + d * FONT_H,
                 "Fx29 for digit %u should point at glyph %u", d, d);
    }

    /* VX > 0x0F must be masked to the low nibble. The COSMAC manual calls
     * this "LSD" and real ROMs depend on it. */
    static const unsigned cases[] = { 0x10, 0x1F, 0x23, 0xA5, 0xFF };
    for (size_t k = 0; k < sizeof(cases) / sizeof(*cases); k++) {
        Chip8 c;
        chip8_init(&c);
        c.regs[1] = (uint16_t)cases[k];
        step(&c, 0xF129);
        CHECK_EQ(c.i, FONT_BASE + (cases[k] & 0x0F) * FONT_H,
                 "Fx29 must mask VX=0x%02X to its low nibble", cases[k]);
    }

    /* Fx29 uses the x nibble, not a fixed register. */
    Chip8 d;
    chip8_init(&d);
    d.regs[0] = 0x99;
    d.regs[7] = 0x03;
    step(&d, 0xF729); /* x=7 -> V7 */
    CHECK_EQ(d.i, FONT_BASE + 3 * FONT_H, "Fx29 must read Vx from the x nibble");
}

/* Drawing a glyph through Fx29 + DxNN is the real end-to-end path. */
static void test_font_draw(void)
{
    Chip8 c;
    chip8_init(&c);
    c.regs[0] = 10; /* x */
    c.regs[1] = 4;  /* y */
    c.regs[2] = 0;  /* digit 0 */

    step(&c, 0xF229); /* Fx29 with x=2 -> V2 */
    CHECK_EQ(c.i, FONT_BASE, "digit 0 should be at the font base");

    step(&c, 0xD015); /* x=0 -> V0, y=1 -> V1, N=5 rows */
    CHECK_EQ(lit(&c), 20, "glyph '0' should light 20 pixels");

    /* '0' is 1111 / 1001 / 1001 / 1001 / 1111 - verify the exact shape. */
    static const uint8_t rows[5] = { 0xF0, 0x90, 0x90, 0x90, 0xF0 };
    for (int r = 0; r < 5; r++)
        for (int b = 0; b < 8; b++)
            CHECK_EQ(c.fb[(4 + r) * FB_W + 10 + b], (rows[r] >> (7 - b)) & 1,
                     "glyph '0' pixel at row %d bit %d", r, b);

    /* Glyphs are 4 wide, so columns 14+ must stay clear. */
    for (int b = 4; b < 8; b++)
        CHECK(!c.fb[4 * FB_W + 10 + b], "column %d is outside a 4-wide glyph", b);

    /* '1' has a different pixel count, proving I really moved. */
    Chip8 d;
    chip8_init(&d);
    d.regs[0] = 10;
    d.regs[1] = 4;
    d.regs[2] = 1;
    step(&d, 0xF229);
    CHECK_EQ(d.i, FONT_BASE + FONT_H, "digit 1 should be one glyph along");
    step(&d, 0xD015);
    CHECK_EQ(lit(&d), 11, "glyph '1' should light 11 pixels");
}

static void test_load_rom(void)
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

    /* Oversized ROM: truncated, and the caller is told. */
    uint8_t big[PROG_MAX + 100];
    memset(big, 0xAA, sizeof(big));
    chip8_init(&c);
    trunc = false;
    CHECK(chip8_load_rom(&c, big, sizeof(big), &trunc), "oversized ROM loads");
    CHECK(trunc, "oversized ROM must set the truncation flag");
    CHECK_EQ(c.mem[MEM_SIZE - 1], 0xAA, "last byte should be written");

    /* The truncation pointer is optional. */
    chip8_init(&c);
    CHECK(chip8_load_rom(&c, rom, sizeof(rom), NULL),
          "truncated pointer may be NULL");
    CHECK_EQ(c.mem[PROG_BASE], 0x12, "load still happened with NULL flag");
}

static void test_fetch(void)
{
    Chip8 c;
    chip8_init(&c);

    c.pc = PROG_BASE;
    c.mem[PROG_BASE] = 0xAB;
    c.mem[PROG_BASE + 1] = 0xCD;
    CHECK_EQ(chip8_fetch(&c), 0xABCD, "fetch is big-endian");

    /* PC wraps within 4K rather than running off the end. */
    c.pc = 0x0FFF;
    c.mem[0x0FFF] = 0x11;
    c.mem[0x0000] = 0x22;
    CHECK_EQ(chip8_fetch(&c), 0x1122, "fetch must wrap at 0x0FFF");
}

static void test_jump(void)
{
    Chip8 c;
    chip8_init(&c);

    step(&c, 0x1ABC);
    CHECK_EQ(c.pc, 0xABC, "1nnn jumps to nnn");
}

static void test_call_return(void)
{
    Chip8 c;
    chip8_init(&c);

    c.pc = PROG_BASE;
    step(&c, 0x2ABC);
    CHECK_EQ(c.pc, 0xABC, "2nnn jumps to the subroutine");
    CHECK_EQ(c.sp, 1, "2nnn pushes a return address");

    /* The return address must survive without a trailing +2 corruption. */
    step(&c, 0x00EE);
    CHECK_EQ(c.pc, PROG_BASE + 2, "00EE returns to the instruction after the call");
    CHECK_EQ(c.sp, 0, "00EE pops the stack");

    /* Return with an empty stack must not underflow. */
    Chip8 d;
    chip8_init(&d);
    d.pc = 0x300;
    step(&d, 0x00EE);
    CHECK_EQ(d.pc, 0x302, "00EE on an empty stack is a no-op");
    CHECK_EQ(d.sp, 0, "sp must not go negative");
}

static void test_stack_saturates(void)
{
    Chip8 c;
    chip8_init(&c);

    c.pc = PROG_BASE;
    for (int i = 0; i < STACK_SIZE * 3; i++)
        step(&c, 0x2ABC);
    CHECK(c.sp > STACK_SIZE, "sp is allowed past the top (uint8_t)");

    /* Pops past the bottom must be no-ops, not reads below the array. */
    for (int i = 0; i < STACK_SIZE * 3; i++)
        step(&c, 0x00EE);
    CHECK_EQ(c.sp, 0, "sp returns to zero after balanced calls");
}

static void test_load_add(void)
{
    Chip8 c;
    chip8_init(&c);

    step(&c, 0x6010);
    CHECK_EQ(c.regs[0], 0x10, "6xnn sets VX to nn");

    step(&c, 0x7005);
    CHECK_EQ(c.regs[0], 0x15, "7xnn adds to VX");

    step(&c, 0x7005);
    CHECK_EQ(c.regs[0], 0x1A, "7xnn accumulates");

    /* Registers are 8-bit, so 0xFF + 0x01 must wrap to 0x00, not 0x100. */
    Chip8 w;
    chip8_init(&w);
    w.regs[0] = 0xFF;
    step(&w, 0x7001);
    CHECK_EQ(w.regs[0], 0x00, "7xnn must wrap at 8 bits like 6xnn stores");

    /* Register indices are the middle nibble, not the low byte. */
    Chip8 d;
    chip8_init(&d);
    step(&d, 0x6A0F);
    CHECK_EQ(d.regs[0xA], 0x0F, "6xnn targets Vx from the middle nibble");
    CHECK_EQ(d.regs[0], 0, "6xnn must not touch V0");

    /* High byte must not leak into VX: 6xFF must set 0xFF, not 0x6FF. */
    Chip8 e;
    chip8_init(&e);
    step(&e, 0x60FF);
    CHECK_EQ(e.regs[0], 0xFF, "6xnn must discard the high nibble");
}

static void test_draw(void)
{
    Chip8 c;
    chip8_init(&c);

    c.regs[0] = 16; /* x */
    c.regs[1] = 8;  /* y */
    c.i = SCRATCH;
    c.mem[SCRATCH + 0] = 0xFF; /* full row of 8 pixels */

    /* DxNN is four hex digits: D, x, y, N. So 0xD011 is x=0 -> V0,
     * y=1 -> V1, N=1 row. N is one nibble, so never test with D011 vs
     * D101 confusion: those differ in x and y, not in the row count. */
    step(&c, 0xD011);
    CHECK_EQ(lit(&c), 8, "Dxn draws n rows of 8 pixels");
    for (int b = 0; b < 8; b++)
        CHECK(c.fb[8 * FB_W + 16 + b], "drawn pixel %d should be set", b);

    /* Draw is XOR, not OR: the same sprite again erases it. */
    step(&c, 0xD011);
    CHECK_EQ(lit(&c), 0, "drawing the same sprite twice must XOR it away");

    /* n = 0 must draw nothing and still advance pc. */
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

/* Regression: the row count of DxNN is the low nibble, not the low byte and
 * not the 12-bit nnn. Using nnn made D014 draw 20 rows instead of 4, which
 * was invisible only because the old 4-row glyph meant the extra rows read
 * zeroes. Filling 16 rows makes the difference observable. */
static void test_draw_row_count_is_a_nibble(void)
{
    Chip8 c;
    chip8_init(&c);

    c.regs[0] = 0;
    c.regs[1] = 0;
    c.i = SCRATCH;
    for (int r = 0; r < 16; r++)
        c.mem[SCRATCH + r] = 0xFF;

    /* x and y must be non-zero for this to discriminate: with x=y=0 the
     * 12-bit nnn happens to equal the low nibble. V1 and V2 are 0, so the
     * sprite still lands at the origin. */
    step(&c, 0xD114); /* x=1, y=1, N=4 -> exactly 4 rows */
    CHECK_EQ(lit(&c), 4 * 8, "D114 must draw 4 rows, not 276");

    Chip8 d;
    chip8_init(&d);
    d.i = SCRATCH;
    for (int r = 0; r < 16; r++)
        d.mem[SCRATCH + r] = 0xFF;

    step(&d, 0xD11F); /* x=1, y=1, N=15 -> 15 rows */
    CHECK_EQ(lit(&d), 15 * 8, "D11F must draw 15 rows");
    CHECK(!d.fb[15 * FB_W], "row 15 is outside N=15 and must not be drawn");
}

static void test_draw_wraps(void)
{
    Chip8 c;
    chip8_init(&c);

    /* Start one past the bottom-right corner. CHIP-8 has no framebuffer
     * bounds, so the pixel must wrap to the origin. */
    c.regs[0] = FB_W;
    c.regs[1] = FB_H;
    c.i = SCRATCH;
    c.mem[SCRATCH] = 0x80; /* only the leftmost pixel */

    step(&c, 0xD011); /* x=0 -> V0, y=1 -> V1, N=1 */
    CHECK(c.fb[0], "pixel past the bottom-right corner wraps to the origin");
    CHECK_EQ(lit(&c), 1, "wrapping draw should light exactly one pixel");
}

static void test_set_index(void)
{
    Chip8 c;
    chip8_init(&c);

    step(&c, 0xF020);
    CHECK_EQ(c.i, 0x020, "F0nn sets I to nn");

    /* I only has 8 significant bits here, so it can never exceed 0x0FF. */
    Chip8 d;
    chip8_init(&d);
    step(&d, 0xF0FF);
    CHECK_EQ(d.i, 0xFF, "F0FF sets I to 0xFF");
}

/* The 0xF group is ambiguous: F0nn shares an encoding with Fxkk, so F055 is
 * both "I = 0x55" and "store V0 to memory". Reserved low bytes must be
 * treated as the specific opcode and must not clobber I. */
static void test_f_group_reserved(void)
{
    /* 0x29 is deliberately absent: Fx29 is implemented, so it must set I. */
    static const uint16_t reserved[] = {
        0x55, 0x65, 0x15, 0x1A, 0x1E,
        0x33, 0x75, 0x85, 0x0A, 0xB0,
    };
    for (size_t i = 0; i < sizeof(reserved) / sizeof(*reserved); i++) {
        for (unsigned x = 0; x < 16; x++) {
            Chip8 c;
            chip8_init(&c);
            c.i = 0x123;
            chip8_execute(&c, (uint16_t)(0xF000 | (x << 8) | reserved[i]));
            CHECK_EQ(c.i, 0x123, "reserved opcode F%X%02X must not set I",
                     x, reserved[i]);
        }
    }
}

static int is_reserved(unsigned kk)
{
    switch (kk) {
    case 0x55: case 0x65: case 0x15: case 0x1A: case 0x1E:
    case 0x29: case 0x33: case 0x75: case 0x85: case 0x0A: case 0xB0:
        return 1;
    default:
        return 0;
    }
}

static void test_f_group_nonreserved(void)
{
    /* Every non-reserved low byte decodes as "I = nn". 0x00 is included:
     * F000 is an ordinary "I = 0", not a special opcode. */
    int mismatches = 0;
    for (unsigned v = 0; v < 0x100; v++) {
        if (is_reserved(v))
            continue;
        Chip8 c;
        chip8_init(&c);
        chip8_execute(&c, (uint16_t)(0xF000 | v));
        if (c.i != v)
            mismatches++;
    }
    CHECK_EQ(mismatches, 0, "all non-reserved F0nn values must set I");

    /* F000 specifically: regression guard for treating 0x00 as reserved. */
    Chip8 z;
    chip8_init(&z);
    z.i = 0x123;
    chip8_execute(&z, 0xF000);
    CHECK_EQ(z.i, 0x000, "F000 sets I to 0");
}

static void test_unknown_opcode_is_inert(void)
{
    /* An unimplemented opcode must advance pc and touch nothing else. */
    Chip8 c;
    chip8_init(&c);

    c.regs[0] = 0xAB;
    uint16_t pc = c.pc;

    step(&c, 0x9ABC); /* not a real group */
    CHECK_EQ(c.pc, pc + 2, "unknown opcode still advances pc by 2");
    CHECK_EQ(c.regs[0], 0xAB, "unknown opcode must not change registers");
    CHECK_EQ(lit(&c), 0, "unknown opcode must not touch the framebuffer");
    CHECK_EQ(c.sp, 0, "unknown opcode must not touch the stack");
}

static void test_pc_wraps(void)
{
    Chip8 c;
    chip8_init(&c);

    c.pc = 0x0FFE;
    step(&c, 0x0000); /* unimplemented 0nnn, just advances pc */
    CHECK_EQ(c.pc, 0x000, "pc must wrap from 0x0FFF back to 0x000");
}

/* SCHIP's Dx0 and DxCN reuse the DxNN encoding and are not supported by this
 * original-CHIP-8 interpreter. They must not crash, and Dx0 must draw nothing.
 * A ROM that needs them needs SCHIP support, not a decoder fix. */
static void test_schip_draw_forms(void)
{
    Chip8 c;
    chip8_init(&c);
    c.regs[0] = 0;
    c.regs[1] = 0;
    c.i = 0x100;
    for (int r = 0; r < 16; r++)
        c.mem[SCRATCH + r] = 0xFF;

    step(&c, 0xD010); /* x=1,y=1, N=0 -> the 16x16 form, unsupported */
    CHECK_EQ(lit(&c), 0, "Dx0 is an unsupported SCHIP form and draws nothing");

    Chip8 d;
    chip8_init(&d);
    d.regs[0] = 0;
    d.regs[1] = 0;
    d.i = SCRATCH;
    for (int r = 0; r < 16; r++)
        d.mem[SCRATCH + r] = 0xFF;

    /* DxCN should be C rows of 16 pixels wide; unsupported here, so it is
     * drawn as C rows of 8 pixels. This test documents the wrong result on
     * purpose so it cannot drift silently. */
    step(&d, 0xD1C4);
    CHECK_EQ(lit(&d), 4 * 8, "DxC4 is unsupported and drawn 8 pixels wide");
}

int main(void)
{
    test_init();
    test_font();
    test_font_pointer();
    test_font_draw();
    test_load_rom();
    test_fetch();
    test_jump();
    test_call_return();
    test_stack_saturates();
    test_load_add();
    test_draw();
    test_draw_row_count_is_a_nibble();
    test_draw_wraps();
    test_schip_draw_forms();
    test_set_index();
    test_f_group_reserved();
    test_f_group_nonreserved();
    test_unknown_opcode_is_inert();
    test_pc_wraps();

    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
