#include "test.h"

void test_draw(void)
{
    Chip8 c;
    chip8_init(&c);
    c.regs[0] = 16;
    c.regs[1] = 8;
    c.i = SCRATCH;
    c.mem[SCRATCH] = 0xFF;

    step(&c, 0xD011);
    CHECK_EQ(lit(&c), 8, "draw lights 8 px");
    CHECK_EQ(c.regs[0xF], 0, "first draw clears VF");
    step(&c, 0xD011);
    CHECK_EQ(lit(&c), 0, "second draw XORs away");
    CHECK_EQ(c.regs[0xF], 1, "erase sets VF");

    Chip8 k;
    chip8_init(&k);
    k.regs[0] = FB_W - 1;
    k.regs[1] = FB_H - 1;
    k.i = SCRATCH;
    k.mem[SCRATCH] = 0xFF;
    step(&k, 0xD011);
    CHECK_EQ(lit(&k), 1, "vip clips at edge");

    Chip8 n;
    chip8_init(&n);
    n.regs[0] = 0;
    n.regs[1] = 0;
    n.i = SCRATCH;
    for (int r = 0; r < 16; r++)
        n.mem[SCRATCH + r] = 0xFF;
    step(&n, 0xD114);
    CHECK_EQ(lit(&n), 4 * 8, "row count is low nibble");

    Chip8 e;
    chip8_init(&e);
    e.fb[0] = true;
    step(&e, 0x00E0);
    CHECK_EQ(lit(&e), 0, "00E0 clears");
}
