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

/* Execute one opcode loaded at pc and report how far pc moved: 2 for a plain
 * instruction, 4 for a taken skip, 0 for Fx0A still waiting. */
static int step_pc(Chip8 *c, uint16_t opcode)
{
    uint16_t before = c->pc;
    step(c, opcode);
    return (int)((c->pc - before) & 0x0FFF);
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

    /* A machine boots with no keys down and a usable random source. */
    for (unsigned k = 0; k < NUM_KEYS; k++)
        CHECK(!c.keys[k], "key %u must start up", k);
    CHECK(c.rng != 0, "the xorshift state must never be all zero");
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
    /* '0' is 4 pixels wide over 5 rows: 4+2+2+2+4 = 14 lit, not 20. The
     * 20 counted every row as full width, which no hex glyph ever is. */
    CHECK_EQ(lit(&c), 14, "glyph '0' should light 14 pixels");

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
    /* '1' is 20,60,20,20,70 -> 1+2+1+1+3 = 8 lit pixels. */
    CHECK_EQ(lit(&d), 8, "glyph '1' should light 8 pixels");
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

    /* sp is uint8_t, so a runaway call chain must saturate instead of
     * wrapping to 0 and starting to overwrite stack[0] from below. */
    Chip8 d;
    chip8_init(&d);
    d.pc = PROG_BASE;
    for (int i = 0; i < STACK_SIZE * 25; i++)
        step(&d, 0x2ABC);
    CHECK_EQ(d.sp, UINT8_MAX, "sp saturates rather than wrapping to 0");

    /* The first pop has to return to the deepest frame that was really
     * stored, without reading stack[sp] out of bounds on the way. */
    step(&d, 0x00EE);
    CHECK_EQ(d.sp, STACK_SIZE - 1, "the pop drops the phantom frames too");
    CHECK_EQ(d.pc, 0x0ABE, "the pop returns to the last stored address");
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

/* The start coordinate wraps into the screen, but a sprite that then runs off
 * the right or bottom edge stops there. The VIP's DRAW walks a contiguous
 * framebuffer and halts at the end of it; wrapping the sprite itself around
 * to the opposite edge is the SCHIP behaviour, and it is what the suite's
 * quirks test reports as "clipping". */
static void test_draw_clips(void)
{
    Chip8 c;
    chip8_init(&c);
    c.regs[0] = FB_W - 1;      /* last column */
    c.regs[1] = FB_H - 1;      /* last row */
    c.i = SCRATCH;
    c.mem[SCRATCH] = 0xFF;

    step(&c, 0xD011);
    CHECK_EQ(lit(&c), 1, "a sprite at the far corner is clipped to one pixel");
    CHECK(c.fb[(FB_H - 1) * FB_W + FB_W - 1], "that pixel lands in place");
    CHECK(!c.fb[FB_W - 1], "nothing wraps back up to the first row");

    Chip8 d;
    chip8_init(&d);
    d.regs[0] = 0;
    d.regs[1] = FB_H - 1;      /* one row of the sprite hangs off the bottom */
    d.i = SCRATCH;
    d.mem[SCRATCH] = 0xFF;
    d.mem[SCRATCH + 1] = 0xFF;
    step(&d, 0xD012);
    CHECK_EQ(lit(&d), 8, "the row past the bottom edge is dropped");
    CHECK(!d.fb[0] && !d.fb[1], "the dropped row does not reappear on top");

    Chip8 e;
    chip8_init(&e);
    e.regs[0] = FB_W - 3;      /* three columns of the sprite fit */
    e.regs[1] = 0;
    e.i = SCRATCH;
    e.mem[SCRATCH] = 0xFF;
    step(&e, 0xD011);
    CHECK_EQ(lit(&e), 3, "the columns past the right edge are dropped");
}

/* Annn is how a ROM sets I, and therefore how any ROM draws anything. It is
 * the instruction that was missing while F0nn was being decoded instead. */
static void test_load_i(void)
{
    Chip8 c;
    chip8_init(&c);

    step(&c, 0xA2BC);
    CHECK_EQ(c.i, 0x2BC, "Annn sets I to nnn");

    /* nnn is all 12 low bits, so x and y are part of the address. */
    step(&c, 0xA123);
    CHECK_EQ(c.i, 0x123, "Annn takes every one of its 12 bits");

    step(&c, 0xAFFF);
    CHECK_EQ(c.i, 0xFFF, "Annn can address the top of memory");

    /* Annn must leave the registers and the timers alone. */
    Chip8 d;
    chip8_init(&d);
    d.regs[3] = 0x77;
    d.dt = 5;
    step(&d, 0xA300);
    CHECK_EQ(d.regs[3], 0x77, "Annn does not touch registers");
    CHECK_EQ(d.dt, 5, "Annn does not touch the timers");
}

static void test_jump_offset(void)
{
    /* Bnnn - jump to nnn + V0. Used by the classic maze-style ROMs. */
    Chip8 c;
    chip8_init(&c);
    c.regs[0] = 0x10;
    step(&c, 0xB200);
    CHECK_EQ(c.pc, 0x210, "Bnnn jumps to nnn + V0");

    /* V0 is 8 bits, and the result still lives in 12. */
    Chip8 d;
    chip8_init(&d);
    d.regs[0] = 0xFF;
    step(&d, 0xBFF0);
    CHECK_EQ(d.pc, 0x0EF, "Bnnn wraps inside 12 bits");
}

static void test_clear(void)
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

static void test_sys_is_ignored(void)
{
    /* 0nnn is the old SYS jump. Cowgod 3.1 says it "is ignored by modern
     * interpreters", so it must neither jump nor set I. */
    Chip8 c;
    chip8_init(&c);
    c.i = 0x333;
    uint16_t pc = c.pc;

    step(&c, 0x0ABC);
    CHECK_EQ(c.pc, pc + 2, "0nnn does not jump");
    CHECK_EQ(c.i, 0x333, "0nnn does not set I");
}

static void test_skips(void)
{
    /* 3xkk - skip if VX == kk */
    Chip8 a;
    chip8_init(&a);
    a.regs[3] = 0x42;
    CHECK_EQ(step_pc(&a, 0x3342), 4, "3xkk skips when equal");
    CHECK_EQ(step_pc(&a, 0x3341), 2, "3xkk does not skip when unequal");

    /* 4xkk - skip if VX != kk */
    Chip8 b;
    chip8_init(&b);
    b.regs[3] = 0x42;
    CHECK_EQ(step_pc(&b, 0x4341), 4, "4xkk skips when unequal");
    CHECK_EQ(step_pc(&b, 0x4342), 2, "4xkk does not skip when equal");

    /* 5xy0 - skip if VX == VY. The low nibble must be 0. */
    Chip8 c;
    chip8_init(&c);
    c.regs[2] = 7;
    c.regs[9] = 7;
    CHECK_EQ(step_pc(&c, 0x5290), 4, "5xy0 skips when equal");
    CHECK_EQ(step_pc(&c, 0x5291), 2, "5xy1 does not exist and never skips");

    /* 9xy0 - skip if VX != VY */
    Chip8 d;
    chip8_init(&d);
    d.regs[2] = 7;
    d.regs[9] = 8;
    CHECK_EQ(step_pc(&d, 0x9290), 4, "9xy0 skips when unequal");
    CHECK_EQ(step_pc(&d, 0x9291), 2, "9xy1 does not exist and never skips");
    d.regs[9] = 7;
    CHECK_EQ(step_pc(&d, 0x9290), 2, "9xy0 does not skip when equal");

    /* A skip must not clobber registers on the way past. */
    Chip8 e;
    chip8_init(&e);
    e.regs[3] = 1;
    step(&e, 0x3301);
    CHECK_EQ(e.regs[3], 1, "a taken skip leaves Vx alone");
    CHECK_EQ(e.regs[0xF], 0, "a skip does not set VF");
}

static void test_alu(void)
{
    Chip8 c;
    chip8_init(&c);

    c.regs[1] = 0xF0;
    c.regs[2] = 0x0F;
    step(&c, 0x8120); /* LD V1, V2 */
    CHECK_EQ(c.regs[1], 0x0F, "8xy0 copies Vy into Vx");
    CHECK_EQ(c.regs[2], 0x0F, "8xy0 leaves Vy alone");

    c.regs[1] = 0x0F;
    c.regs[2] = 0xF0;
    step(&c, 0x8121); /* OR */
    CHECK_EQ(c.regs[1], 0xFF, "8xy1 ORs");

    c.regs[1] = 0x0F;
    c.regs[2] = 0xF0;
    step(&c, 0x8122); /* AND */
    CHECK_EQ(c.regs[1], 0x00, "8xy2 ANDs");

    c.regs[1] = 0xFF;
    c.regs[2] = 0x0F;
    step(&c, 0x8123); /* XOR */
    CHECK_EQ(c.regs[1], 0xF0, "8xy3 XORs");

    /* 8xy8-8xyD are not instructions. */
    Chip8 d;
    chip8_init(&d);
    d.regs[1] = 0x42;
    d.regs[2] = 0x99;
    step(&d, 0x8129);
    CHECK_EQ(d.regs[1], 0x42, "8xy9 does not exist and changes nothing");
    CHECK_EQ(d.regs[0xF], 0, "8xy9 does not set VF");
}

static void test_alu_flags(void)
{
    /* 8xy4 - ADD, VF = carry */
    Chip8 a;
    chip8_init(&a);
    a.regs[1] = 0xF0;
    a.regs[2] = 0x20;
    step(&a, 0x8124);
    CHECK_EQ(a.regs[1], 0x10, "8xy4 keeps the low 8 bits");
    CHECK_EQ(a.regs[0xF], 1, "8xy4 sets VF on carry");

    Chip8 b;
    chip8_init(&b);
    b.regs[1] = 0x10;
    b.regs[2] = 0x20;
    step(&b, 0x8124);
    CHECK_EQ(b.regs[1], 0x30, "8xy4 sums");
    CHECK_EQ(b.regs[0xF], 0, "8xy4 clears VF without a carry");

    /* 8xy5 - SUB, VF = NOT borrow */
    Chip8 c;
    chip8_init(&c);
    c.regs[1] = 0x50;
    c.regs[2] = 0x30;
    step(&c, 0x8125);
    CHECK_EQ(c.regs[1], 0x20, "8xy5 subtracts");
    CHECK_EQ(c.regs[0xF], 1, "8xy5 VF is 1 when there is no borrow");

    Chip8 d;
    chip8_init(&d);
    d.regs[1] = 0x30;
    d.regs[2] = 0x50;
    step(&d, 0x8125);
    CHECK_EQ(d.regs[1], 0xE0, "8xy5 wraps on borrow");
    CHECK_EQ(d.regs[0xF], 0, "8xy5 VF is 0 when it borrows");

    /* 8xy7 - SUBN is Vy - Vx, the other way round */
    Chip8 e;
    chip8_init(&e);
    e.regs[1] = 0x30;
    e.regs[2] = 0x50;
    step(&e, 0x8127);
    CHECK_EQ(e.regs[1], 0x20, "8xy7 does Vy - Vx");
    CHECK_EQ(e.regs[0xF], 1, "8xy7 VF is 1 when there is no borrow");

    Chip8 f;
    chip8_init(&f);
    f.regs[1] = 0x50;
    f.regs[2] = 0x30;
    step(&f, 0x8127);
    CHECK_EQ(f.regs[1], 0xE0, "8xy7 wraps on borrow");
    CHECK_EQ(f.regs[0xF], 0, "8xy7 VF is 0 when it borrows");

    /* 8xy6 - SHR, VF = the bit shifted out. Original CHIP-8 shifts Vy and
     * parks the result in Vx; Chip-48 and later shift Vx in place instead. */
    Chip8 g;
    chip8_init(&g);
    g.regs[1] = 0x03;   /* Vx, which must be overwritten not shifted */
    g.regs[2] = 0xFF;   /* Vy, the value that actually gets shifted */
    step(&g, 0x8126);
    CHECK_EQ(g.regs[1], 0x7F, "8xy6 shifts Vy into Vx, not Vx in place");
    CHECK_EQ(g.regs[0xF], 1, "8xy6 VF takes Vy's old bit 0");

    Chip8 h;
    chip8_init(&h);
    h.regs[1] = 0xFF;
    h.regs[2] = 0x04;
    step(&h, 0x8126);
    CHECK_EQ(h.regs[1], 0x02, "8xy6 shifts Vy right");
    CHECK_EQ(h.regs[0xF], 0, "8xy6 VF is 0 for an even Vy");

    /* 8xyE - SHL, VF = the bit shifted out */
    Chip8 i;
    chip8_init(&i);
    i.regs[1] = 0x01;
    i.regs[2] = 0x81;
    step(&i, 0x812E);
    CHECK_EQ(i.regs[1], 0x02, "8xyE shifts Vy left into Vx");
    CHECK_EQ(i.regs[0xF], 1, "8xyE VF takes Vy's old bit 7");

    Chip8 j;
    chip8_init(&j);
    j.regs[1] = 0x80;
    j.regs[2] = 0x01;
    step(&j, 0x812E);
    CHECK_EQ(j.regs[1], 0x02, "8xyE shifts Vy left");
    CHECK_EQ(j.regs[0xF], 0, "8xyE VF is 0 when Vy's bit 7 was clear");
}

/* On the COSMAC VIP the logical opcodes ran their result through the flag
 * register, so VF came out as zero afterwards. Modern interpreters leave VF
 * alone; the suite reports the difference as the "vF reset" quirk. */
static void test_logical_clears_vf(void)
{
    Chip8 c;
    chip8_init(&c);
    c.regs[1] = 0xF0;
    c.regs[2] = 0x0F;
    c.regs[0xF] = 0x5A;
    step(&c, 0x8121);                       /* V1 |= V2 */
    CHECK_EQ(c.regs[1], 0xFF, "8xy1 still ORs");
    CHECK_EQ(c.regs[0xF], 0, "8xy1 clears VF");

    Chip8 d;
    chip8_init(&d);
    d.regs[1] = 0xFF;
    d.regs[2] = 0x0F;
    d.regs[0xF] = 0x5A;
    step(&d, 0x8122);                       /* V1 &= V2 */
    CHECK_EQ(d.regs[1], 0x0F, "8xy2 still ANDs");
    CHECK_EQ(d.regs[0xF], 0, "8xy2 clears VF");

    Chip8 e;
    chip8_init(&e);
    e.regs[1] = 0xFF;
    e.regs[2] = 0x0F;
    e.regs[0xF] = 0x5A;
    step(&e, 0x8123);                       /* V1 ^= V2 */
    CHECK_EQ(e.regs[1], 0xF0, "8xy3 still XORs");
    CHECK_EQ(e.regs[0xF], 0, "8xy3 clears VF");

    /* 8xy0 is a plain move and must not be caught up in this. */
    Chip8 f;
    chip8_init(&f);
    f.regs[2] = 0x37;
    f.regs[0xF] = 0x5A;
    step(&f, 0x8120);                       /* V1 = V2 */
    CHECK_EQ(f.regs[1], 0x37, "8xy0 still copies");
    CHECK_EQ(f.regs[0xF], 0x5A, "8xy0 leaves VF alone");
}

/* The VIP's DRAW blocked until the next vertical blank, which caps drawing at
 * one sprite per frame. The core signals that with `vblank`; the frontend
 * clears it by ticking the timers, which it already does once per frame. */
static void test_display_wait(void)
{
    Chip8 c;
    chip8_init(&c);
    c.i = SCRATCH;
    c.mem[SCRATCH] = 0x80;
    c.mem[0x200] = 0x60; c.mem[0x201] = 0x01;   /* V0 = 1   */
    c.mem[0x202] = 0xD0; c.mem[0x203] = 0x11;   /* DRW V0,V1,1 */
    c.mem[0x204] = 0x61; c.mem[0x205] = 0x01;   /* V1 = 1   */

    CHECK(!c.vblank, "vblank starts clear");
    chip8_run(&c, 10);
    CHECK(c.vblank, "drawing raises the vblank flag");
    CHECK_EQ(c.pc, 0x204, "the draw itself has finished");
    CHECK_EQ(c.regs[1], 0, "the rest of the frame's cycles are not spent");

    chip8_tick_timers(&c);
    CHECK(!c.vblank, "the frame boundary clears the flag");
    chip8_run(&c, 1);
    CHECK_EQ(c.regs[1], 1, "execution resumes on the following frame");
}

static void test_alu_vf_order(void)
{
    /* When x is VF the result and the flag land in the same register.
     * Cowgod lists the result first and the flag second, so the flag has to
     * win. Writing VF first would make every such instruction wrong. */
    Chip8 a;
    chip8_init(&a);
    a.regs[0xF] = 0xF0;
    a.regs[2] = 0x20;
    step(&a, 0x8F24);
    CHECK_EQ(a.regs[0xF], 1, "8xy4 with x=VF ends holding the carry");

    Chip8 b;
    chip8_init(&b);
    b.regs[0xF] = 0xF0;
    b.regs[2] = 0x20;
    step(&b, 0x8F25);
    CHECK_EQ(b.regs[0xF], 1, "8xy5 with x=VF ends holding NOT borrow");

    /* x == y: read Vx before writing it, or the operand is already lost. */
    Chip8 c;
    chip8_init(&c);
    c.regs[4] = 0x81;
    step(&c, 0x844E);
    CHECK_EQ(c.regs[4], 0x02, "8xyE with x==y shifts the original value");
    CHECK_EQ(c.regs[0xF], 1, "8xyE with x==y still sets VF");
}

static void test_random(void)
{
    /* Cxkk. Seeded explicitly, because a test that depends on wall time is a
     * test that fails on someone else's machine. */
    Chip8 a, b;
    chip8_init(&a);
    chip8_init(&b);
    chip8_seed(&a, 12345);
    chip8_seed(&b, 12345);

    step(&a, 0xC1FF);
    step(&b, 0xC1FF);
    CHECK_EQ(a.regs[1], b.regs[1], "the same seed must give the same value");
    CHECK_EQ(a.regs[1] & 0xFF, a.regs[1], "Cxkk result fits in 8 bits");

    /* The random byte is ANDed with kk, so bits kk does not have never show. */
    chip8_seed(&a, 999);
    step(&a, 0xC20F);
    CHECK_EQ(a.regs[2] & ~0x0F, 0, "Cxkk masks the random byte with kk");

    Chip8 c;
    chip8_init(&c);
    step(&c, 0xC300);
    CHECK_EQ(c.regs[3], 0, "Cxkk with kk=0 always yields 0");

    /* xorshift cannot run from an all-zero state; seeding with 0 must not
     * produce a generator that never moves. */
    Chip8 d;
    chip8_init(&d);
    chip8_seed(&d, 0);
    step(&d, 0xC4FF);
    uint16_t first = d.regs[4];
    step(&d, 0xC4FF);
    CHECK(d.regs[4] != first, "a zero seed still yields a moving stream");
}

static void test_draw_sets_collision(void)
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

    /* Disjoint sprites must not collide even when both are lit. */
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

static void test_key_skips(void)
{
    Chip8 c;
    chip8_init(&c);
    c.regs[1] = 0x0A;

    CHECK_EQ(step_pc(&c, 0xE19E), 2, "Ex9E does not skip while the key is up");
    chip8_set_key(&c, 0x0A, true);
    CHECK_EQ(step_pc(&c, 0xE19E), 4, "Ex9E skips while the key is down");

    CHECK_EQ(step_pc(&c, 0xE1A1), 2, "ExA1 does not skip while the key is down");
    chip8_set_key(&c, 0x0A, false);
    CHECK_EQ(step_pc(&c, 0xE1A1), 4, "ExA1 skips while the key is up");

    /* Vx holds a key value, not a register index: a stale value above 0xF
     * names no key and must read as "up" rather than wrap onto another. */
    Chip8 d;
    chip8_init(&d);
    d.regs[1] = 0x10;
    chip8_set_key(&d, 0x00, true);
    CHECK_EQ(step_pc(&d, 0xE19E), 2, "Vx=0x10 matches no key at all");

    /* Every one of the 16 keys is addressable. */
    for (unsigned k = 0; k < NUM_KEYS; k++) {
        Chip8 e;
        chip8_init(&e);
        e.regs[1] = (uint16_t)k;
        chip8_set_key(&e, k, true);
        CHECK_EQ(step_pc(&e, 0xE19E), 4, "key %u is reachable", k);
    }

    /* Out-of-range keys are dropped rather than written past the array. */
    chip8_set_key(&d, NUM_KEYS, true);
    chip8_set_key(&d, 1000, true);
    int down = 0;
    for (unsigned k = 0; k < NUM_KEYS; k++)
        if (d.keys[k])
            down++;
    CHECK_EQ(down, 1, "out of range keys are ignored, never aliased");
}

static void test_timers(void)
{
    Chip8 c;
    chip8_init(&c);
    CHECK_EQ(c.dt, 0, "DT starts clear");
    CHECK_EQ(c.st, 0, "ST starts clear");
    CHECK(!chip8_sound(&c), "the buzzer is off on a fresh machine");

    c.regs[3] = 5;
    step(&c, 0xF315);
    CHECK_EQ(c.dt, 5, "Fx15 loads the delay timer");

    c.regs[3] = 3;
    step(&c, 0xF318);
    CHECK_EQ(c.st, 3, "Fx18 loads the sound timer");
    CHECK(chip8_sound(&c), "the buzzer sounds while ST is non-zero");

    c.regs[4] = 0xFF;
    step(&c, 0xF407);
    CHECK_EQ(c.regs[4], 5, "Fx07 reads DT back into Vx");

    /* Timers are clocked at 60Hz by chip8_tick_timers, not by execution. */
    chip8_tick_timers(&c);
    CHECK_EQ(c.dt, 4, "DT decrements once per tick");
    CHECK_EQ(c.st, 2, "ST decrements once per tick");
    chip8_run(&c, 50);
    CHECK_EQ(c.dt, 4, "running opcodes does not tick the timers");

    c.dt = 1;
    c.st = 1;
    chip8_tick_timers(&c);
    CHECK_EQ(c.dt, 0, "DT stops at 0");
    CHECK_EQ(c.st, 0, "ST stops at 0");
    CHECK(!chip8_sound(&c), "the buzzer stops when ST reaches 0");

    chip8_tick_timers(&c);
    CHECK_EQ(c.dt, 0, "DT must not wrap to 255");
    CHECK_EQ(c.st, 0, "ST must not wrap to 255");

    /* The two low bytes the old decode table had missed entirely. F007 and
     * F018 used to read as "I = nn", so V0 never saw DT and ST was never
     * loaded from V0. */
    Chip8 f;
    chip8_init(&f);
    f.dt = 9;
    step(&f, 0xF007);
    CHECK_EQ(f.regs[0], 9, "F007 reads DT into V0");
    CHECK_EQ(f.i, 0, "F007 does not set I");

    f.regs[0] = 4;
    step(&f, 0xF018);
    CHECK_EQ(f.st, 4, "F018 sets ST from V0");
    CHECK_EQ(f.i, 0, "F018 does not set I");
}

static void test_wait_key(void)
{
    Chip8 c;
    chip8_init(&c);
    c.regs[2] = 0xAA;

    /* Nothing pressed: pc must not move, so the fetch re-reads the very
     * same instruction next cycle until a key arrives. */
    CHECK_EQ(step_pc(&c, 0xF20A), 0, "Fx0A stalls while no key is down");
    CHECK_EQ(c.regs[2], 0xAA, "Fx0A does not write Vx while it waits");
    CHECK_EQ(step_pc(&c, 0xF20A), 0, "Fx0A keeps stalling across cycles");

    chip8_set_key(&c, 0x0B, true);
    CHECK_EQ(step_pc(&c, 0xF20A), 2, "Fx0A releases once a key is down");
    CHECK_EQ(c.regs[2], 0x0B, "Fx0A stores the key that was pressed");

    /* A different register: Fx0A is not stuck writing V2. */
    Chip8 d;
    chip8_init(&d);
    chip8_set_key(&d, 0x07, true);
    d.regs[9] = 0;
    step(&d, 0xF90A);
    CHECK_EQ(d.regs[9], 0x07, "Fx0A writes the register named by x");
}

static void test_add_to_i(void)
{
    Chip8 c;
    chip8_init(&c);
    c.i = 0x100;
    c.regs[5] = 0x23;
    step(&c, 0xF51E);
    CHECK_EQ(c.i, 0x123, "Fx1E adds Vx to I");

    /* SCHIP defines VF on overflow past 0xFFF; original CHIP-8 does not. */
    Chip8 d;
    chip8_init(&d);
    d.i = 0xFF0;
    d.regs[5] = 0x20;
    step(&d, 0xF51E);
    CHECK_EQ(d.i, 0x1010, "Fx1E does not mask I to 12 bits");
    CHECK_EQ(d.regs[0xF], 0, "Fx1E leaves VF alone on original CHIP-8");
}

static void test_bcd(void)
{
    Chip8 c;
    chip8_init(&c);
    c.i = SCRATCH;
    c.regs[7] = 231;
    step(&c, 0xF733);
    CHECK_EQ(c.mem[SCRATCH + 0], 2, "Fx33 hundreds digit");
    CHECK_EQ(c.mem[SCRATCH + 1], 3, "Fx33 tens digit");
    CHECK_EQ(c.mem[SCRATCH + 2], 1, "Fx33 ones digit");
    CHECK_EQ(c.i, SCRATCH, "Fx33 leaves I alone");

    Chip8 d;
    chip8_init(&d);
    d.i = SCRATCH;
    d.regs[0] = 0;
    step(&d, 0xF033);
    CHECK_EQ(d.mem[SCRATCH + 0], 0, "Fx33 of 0 writes 0 hundreds");
    CHECK_EQ(d.mem[SCRATCH + 2], 0, "Fx33 of 0 writes 0 ones");

    /* Registers are 8 bits, so the largest BCD value is 255. */
    Chip8 e;
    chip8_init(&e);
    e.i = SCRATCH;
    e.regs[0] = 255;
    step(&e, 0xF033);
    CHECK_EQ(e.mem[SCRATCH + 0], 2, "Fx33 of 255 writes 2 hundreds");
    CHECK_EQ(e.mem[SCRATCH + 1], 5, "Fx33 of 255 writes 5 tens");
    CHECK_EQ(e.mem[SCRATCH + 2], 5, "Fx33 of 255 writes 5 ones");
}

static void test_mem_move(void)
{
    Chip8 a;
    chip8_init(&a);
    a.i = SCRATCH;
    a.regs[0] = 0x11;
    a.regs[1] = 0x22;
    a.regs[2] = 0x33;
    step(&a, 0xF255);
    CHECK_EQ(a.mem[SCRATCH + 0], 0x11, "Fx55 stores V0");
    CHECK_EQ(a.mem[SCRATCH + 1], 0x22, "Fx55 stores V1");
    CHECK_EQ(a.mem[SCRATCH + 2], 0x33, "Fx55 stores V2");
    /* The COSMAC VIP rule: I ends just past the last register written. */
    CHECK_EQ(a.i, SCRATCH + 3, "Fx55 advances I past what it wrote");

    Chip8 b;
    chip8_init(&b);
    b.i = SCRATCH;
    b.mem[SCRATCH + 0] = 0xAA;
    b.mem[SCRATCH + 1] = 0xBB;
    b.regs[0] = 0;
    b.regs[1] = 0;
    step(&b, 0xF165);
    CHECK_EQ(b.regs[0], 0xAA, "Fx65 loads V0");
    CHECK_EQ(b.regs[1], 0xBB, "Fx65 loads V1");
    CHECK_EQ(b.i, SCRATCH + 2, "Fx65 advances I as well");

    /* The exact encoding that used to be called ambiguous. F055 must store
     * V0 at I; it is not "I = 0x55" and never was. */
    Chip8 c;
    chip8_init(&c);
    c.i = SCRATCH;
    c.regs[0] = 0x77;
    step(&c, 0xF055);
    CHECK_EQ(c.mem[SCRATCH], 0x77, "F055 stores V0 at I");
    CHECK_EQ(c.i, SCRATCH + 1, "F055 advances I rather than becoming 0x55");

    /* Storing into the font area is legal: the header documents that ROMs
     * may overwrite it. */
    Chip8 d;
    chip8_init(&d);
    d.i = FONT_BASE;
    d.regs[0] = 0xEE;
    step(&d, 0xF055);
    CHECK_EQ(d.mem[FONT_BASE], 0xEE, "a ROM may overwrite the font");
}

/* Only the nine real F-instructions may have any effect. Every other low
 * byte must leave I, the timers and the registers exactly as they were.
 * This replaces the old "F0nn decodes as I = nn" rule, which stole F055,
 * F007 and F018 from every real ROM. */
static void test_unknown_f_opcode_is_inert(void)
{
    static const unsigned real[] = {
        0x07, 0x0A, 0x15, 0x18, 0x1E, 0x29, 0x33, 0x55, 0x65,
    };

    int wrong = 0;
    for (unsigned kk = 0; kk < 0x100; kk++) {
        bool known = false;
        for (size_t r = 0; r < sizeof real / sizeof *real; r++)
            if (real[r] == kk)
                known = true;
        if (known)
            continue;

        Chip8 c;
        chip8_init(&c);
        c.i = 0x123;
        c.dt = 0x44;
        c.st = 0x55;
        c.regs[0] = 0x66;
        step(&c, (uint16_t)(0xF000 | kk));
        if (c.i != 0x123 || c.dt != 0x44 || c.st != 0x55 || c.regs[0] != 0x66)
            wrong++;
    }
    CHECK_EQ(wrong, 0, "every non-instruction F opcode must change nothing");

    /* F000 specifically: not "I = 0", just not an instruction. */
    Chip8 z;
    chip8_init(&z);
    z.i = 0x123;
    step(&z, 0xF000);
    CHECK_EQ(z.i, 0x123, "F000 does not set I");
    CHECK_EQ(z.pc, PC_START + 2, "F000 still advances pc");
}

static void test_unknown_opcode_is_inert(void)
{
    /* 9xy1-9xyD are not instructions: only 9xy0 exists. This must advance
     * pc and touch nothing else. */
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

static void test_pc_wraps(void)
{
    Chip8 c;
    chip8_init(&c);

    c.pc = 0x0FFE;
    step(&c, 0x0000); /* 0nnn SYS, ignored, but still advances pc */
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
    test_skips();
    test_load_i();
    test_jump_offset();
    test_clear();
    test_sys_is_ignored();
    test_alu();
    test_alu_flags();
    test_alu_vf_order();
    test_logical_clears_vf();
    test_display_wait();
    test_random();
    test_draw();
    test_draw_row_count_is_a_nibble();
    test_draw_wraps();
    test_draw_clips();
    test_draw_sets_collision();
    test_schip_draw_forms();
    test_key_skips();
    test_wait_key();
    test_timers();
    test_add_to_i();
    test_bcd();
    test_mem_move();
    test_unknown_f_opcode_is_inert();
    test_unknown_opcode_is_inert();
    test_pc_wraps();

    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
