#include "test.h"

void test_mem(void)
{
    Chip8 c;
    chip8_init(&c);

    step(&c, 0xA2BC);
    CHECK_EQ(c.i, 0x2BC, "Annn sets I");

    c.i = 0x100;
    c.regs[5] = 0x23;
    step(&c, 0xF51E);
    CHECK_EQ(c.i, 0x123, "Fx1E adds Vx");

    c.i = SCRATCH;
    c.regs[7] = 231;
    step(&c, 0xF733);
    CHECK_EQ(c.mem[SCRATCH], 2, "Fx33 hundreds");
    CHECK_EQ(c.mem[SCRATCH + 2], 1, "Fx33 ones");

    Chip8 m;
    chip8_init(&m);
    m.i = SCRATCH;
    m.regs[0] = 0x11;
    m.regs[1] = 0x22;
    m.regs[2] = 0x33;
    step(&m, 0xF255);
    CHECK_EQ(m.mem[SCRATCH + 2], 0x33, "Fx55 stores V2");
    CHECK_EQ(m.i, SCRATCH + 3, "Fx55 advances I (vip)");

    m.i = SCRATCH;
    m.regs[0] = m.regs[1] = 0;
    step(&m, 0xF165);
    CHECK_EQ(m.regs[0], 0x11, "Fx65 loads V0");
}
