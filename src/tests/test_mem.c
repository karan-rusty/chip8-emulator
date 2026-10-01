#include "test.h"

void test_load_i(void)
{
    Chip8 c;
    chip8_init(&c);

    step(&c, 0xA2BC);
    CHECK_EQ(c.i, 0x2BC, "Annn sets I to nnn");

    step(&c, 0xA123);
    CHECK_EQ(c.i, 0x123, "Annn takes every one of its 12 bits");

    step(&c, 0xAFFF);
    CHECK_EQ(c.i, 0xFFF, "Annn can address the top of memory");

    Chip8 d;
    chip8_init(&d);
    d.regs[3] = 0x77;
    d.dt = 5;
    step(&d, 0xA300);
    CHECK_EQ(d.regs[3], 0x77, "Annn does not touch registers");
    CHECK_EQ(d.dt, 5, "Annn does not touch the timers");
}

void test_add_to_i(void)
{
    Chip8 c;
    chip8_init(&c);
    c.i = 0x100;
    c.regs[5] = 0x23;
    step(&c, 0xF51E);
    CHECK_EQ(c.i, 0x123, "Fx1E adds Vx to I");

    Chip8 d;
    chip8_init(&d);
    d.i = 0xFF0;
    d.regs[5] = 0x20;
    step(&d, 0xF51E);
    CHECK_EQ(d.i, 0x1010, "Fx1E does not mask I to 12 bits");
    CHECK_EQ(d.regs[0xF], 0, "Fx1E leaves VF alone on original CHIP-8");
}

void test_bcd(void)
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

    Chip8 e;
    chip8_init(&e);
    e.i = SCRATCH;
    e.regs[0] = 255;
    step(&e, 0xF033);
    CHECK_EQ(e.mem[SCRATCH + 0], 2, "Fx33 of 255 writes 2 hundreds");
    CHECK_EQ(e.mem[SCRATCH + 1], 5, "Fx33 of 255 writes 5 tens");
    CHECK_EQ(e.mem[SCRATCH + 2], 5, "Fx33 of 255 writes 5 ones");
}

void test_mem_move(void)
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

    Chip8 c;
    chip8_init(&c);
    c.i = SCRATCH;
    c.regs[0] = 0x77;
    step(&c, 0xF055);
    CHECK_EQ(c.mem[SCRATCH], 0x77, "F055 stores V0 at I");
    CHECK_EQ(c.i, SCRATCH + 1, "F055 advances I rather than becoming 0x55");

    Chip8 d;
    chip8_init(&d);
    d.i = FONT_BASE;
    d.regs[0] = 0xEE;
    step(&d, 0xF055);
    CHECK_EQ(d.mem[FONT_BASE], 0xEE, "a ROM may overwrite the font");
}

void test_unknown_f_opcode_is_inert(void)
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

    Chip8 z;
    chip8_init(&z);
    z.i = 0x123;
    step(&z, 0xF000);
    CHECK_EQ(z.i, 0x123, "F000 does not set I");
    CHECK_EQ(z.pc, PC_START + 2, "F000 still advances pc");
}
