#include "chip8.h"

#include <string.h>

/* The standard 8x5 hex font, 16 glyphs of 5 bytes. Each byte holds one row,
 * with the glyph occupying the high nibble and the low nibble left clear, so
 * glyphs are 4 pixels wide and 5 tall.
 *
 * These exact values are what Cowgod's technical reference, tonisagrista's
 * spec, and common emulator implementations all agree on, which is what makes
 * them the de-facto standard. The original COSMAC VIP used a different,
 * space-optimised overlapping font with a lookup table instead; this linear
 * 80-byte layout is the modern convention every ROM is written against. */
static const uint8_t font[FONT_BYTES] = {
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

void chip8_init(Chip8 *c)
{
    memset(c, 0, sizeof(*c));
    c->pc = PC_START;
    memcpy(c->mem + FONT_BASE, font, sizeof(font));
}

bool chip8_load_rom(Chip8 *c, const uint8_t *data, size_t len,
                    bool *truncated)
{
    if (len == 0)
        return false;

    if (truncated)
        *truncated = false;
    if (len > PROG_MAX) {
        len = PROG_MAX;
        if (truncated)
            *truncated = true;
    }

    memcpy(c->mem + PROG_BASE, data, len);
    return true;
}

/* Push return address, saturating at STACK_SIZE. The original COSMAC VIP only
 * checked SP on call, so overflowing the stack here is defined behaviour
 * rather than a crash. */
static void push(Chip8 *c, uint16_t addr)
{
    if (c->sp < STACK_SIZE)
        c->stack[c->sp++] = addr;
    else
        c->sp++;   /* past the top; stays >= STACK_SIZE, so pops stay no-ops */
}

uint16_t chip8_fetch(const Chip8 *c)
{
    uint8_t hi = c->mem[c->pc & 0x0FFF];
    uint8_t lo = c->mem[(c->pc + 1) & 0x0FFF];
    return (uint16_t)((hi << 8) | lo);
}

void chip8_execute(Chip8 *c, uint16_t opcode)
{
    uint16_t x = (opcode >> 8) & 0x0F;
    uint16_t y = (opcode >> 4) & 0x0F;
    uint16_t nnn = opcode & 0x0FFF;

    switch (opcode & 0xF000) {
    case 0x0000:
        if (opcode == 0x00EE) { /* 00EE - return */
            if (c->sp > 0) {
                c->pc = c->stack[--c->sp];
                return; /* pc is the return address; do not add 2 */
            }
            /* empty stack: fall through to the pc += 2 below */
        }
        /* 00FB/00FC/00FD/00FE are SCHIP and not implemented. */
        break;
    case 0x1000: /* 1nnn - jump to nnn */
        c->pc = nnn;
        return;
    case 0x2000: /* 2nnn - call nnn */
        push(c, (uint16_t)(c->pc + 2));
        c->pc = nnn;
        return;
    case 0x6000: /* 6xnn - VX = nn */
        c->regs[x] = nnn & 0xFF;
        break;
    case 0x7000: /* 7xnn - VX += nn */
        /* Registers are 8-bit in original CHIP-8; masking to 0x0FFF here
         * would let VX exceed 0xFF and disagree with 6xnn above. SCHIP
         * widened V to 16-bit, which this interpreter does not target. */
        c->regs[x] = (c->regs[x] + (nnn & 0xFF)) & 0xFF;
        break;
    case 0xD000: /* DxNN - draw an N-row, 8-pixel-wide sprite at VX,VY */
    {
        int vx = c->regs[x] & 0xFF;
        int vy = c->regs[y] & 0xFF;
        /* DxNN is four hex digits: D, x, y, N. N is a single nibble, so the
         * height is opcode & 0x0F (0-15 rows) - not the 12-bit nnn, and not
         * the low byte either.
         *
         * Two SCHIP forms reuse this encoding and are NOT implemented here,
         * because this interpreter targets original CHIP-8 only:
         *   Dx0  - 16x16 sprite. N=0, so this draws nothing.
         *   DxCN - C rows of a 16-pixel-wide sprite (8x16 video mode).
         *          Drawn here as C rows of 8 pixels, i.e. half width.
         * A ROM using either will look wrong rather than crash. Treat that as
         * "needs SCHIP", not as a decoder fault. */
        int height = opcode & 0x0F;

        for (int row = 0; row < height; row++) {
            uint8_t bits = c->mem[(c->i + row) & 0x0FFF];
            for (int bit = 0; bit < 8; bit++) {
                if (!(bits & (0x80 >> bit)))
                    continue;
                /* CHIP-8 has no framebuffer bounds; wrap instead of clipping. */
                int px = (vx + bit) % FB_W;
                int py = (vy + row) % FB_H;
                c->fb[py * FB_W + px] = !c->fb[py * FB_W + px];
            }
        }
        break;
    }
    case 0xF000: {
        /* This group is genuinely ambiguous: F0nn ("I = nn") shares an
         * encoding with Fxkk, because F055 is both "I = 0x55" and "store V0
         * to memory". There is no way to tell them apart, so the usual
         * resolution is to treat a reserved low byte as the specific opcode
         * and everything else as "I = nn". That makes "I = 0x55" and the
         * other reserved values unreachable, which is accepted behaviour.
         *
         * Reserved: Fx55/Fx65 memory moves, Fx15 delay timer, Fx1A wait for
         * key, Fx1E sound, Fx33 BCD, Fx75/Fx85 flag registers, Fx0A key scan,
         * FxB0 flag pointer. Note 0x00 is not reserved: F000 is an ordinary
         * "I = 0". Fx29 (font pointer) is implemented below. */
        uint16_t kk = opcode & 0x00FF;
        switch (kk) {
        case 0x29: /* Fx29 - I = address of the glyph for digit VX */
            /* The COSMAC VIP manual calls this "LSD" (Least Significant
             * Digit) and the disassembly masks with 0x0F explicitly. There
             * are ROMs that leave upper bits set in VX and depend on that,
             * so the mask is required rather than defensive. */
            c->i = FONT_BASE + ((c->regs[x] & 0x0F) * FONT_H);
            break;
        case 0x55: case 0x65: case 0x15: case 0x1A:
        case 0x1E: case 0x33: case 0x75: case 0x85:
        case 0x0A: case 0xB0:
            break; /* specific opcode, not implemented yet */
        default:
            c->i = kk; /* F0nn - I = nn. Low byte only, so I tops out at 0x0FF */
            break;
        }
        break;
    }
    default:
        break;
    }

    c->pc = (c->pc + 2) & 0x0FFF;
}

void chip8_run(Chip8 *c, int cycles)
{
    for (int i = 0; i < cycles; i++)
        chip8_execute(c, chip8_fetch(c));
}
