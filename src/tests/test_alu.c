#include "test.h"

void test_alu(void)
{
    Chip8 c;
    chip8_init(&c);

    step(&c, 0x6010);
    CHECK_EQ(c.regs[0], 0x10, "6xnn sets VX");
    step(&c, 0x7005);
    CHECK_EQ(c.regs[0], 0x15, "7xnn adds");

    c.regs[0] = 0xFF;
    c.pc = PC_START;
    step(&c, 0x7001);
    CHECK_EQ(c.regs[0], 0x00, "7xnn wraps at 8 bits");

    c.regs[1] = 0x0F;
    c.regs[2] = 0xF0;
    step(&c, 0x8121);
    CHECK_EQ(c.regs[1], 0xFF, "8xy1 ORs");
    CHECK_EQ(c.regs[0xF], 0, "8xy1 clears VF (vip)");

    c.regs[1] = 0x0F;
    c.regs[2] = 0xF0;
    step(&c, 0x8122);
    CHECK_EQ(c.regs[1], 0x00, "8xy2 ANDs");

    c.regs[1] = 0xFF;
    c.regs[2] = 0x0F;
    step(&c, 0x8123);
    CHECK_EQ(c.regs[1], 0xF0, "8xy3 XORs");

    c.regs[1] = 0xF0;
    c.regs[2] = 0x20;
    step(&c, 0x8124);
    CHECK_EQ(c.regs[1], 0x10, "8xy4 low bits");
    CHECK_EQ(c.regs[0xF], 1, "8xy4 carry");

    c.regs[1] = 0x30;
    c.regs[2] = 0x50;
    step(&c, 0x8125);
    CHECK_EQ(c.regs[1], 0xE0, "8xy5 wraps on borrow");
    CHECK_EQ(c.regs[0xF], 0, "8xy5 borrow clears VF");

    c.regs[1] = 0x03;
    c.regs[2] = 0xFF;
    step(&c, 0x8126);
    CHECK_EQ(c.regs[1], 0x7F, "8xy6 shifts Vy (vip)");
    CHECK_EQ(c.regs[0xF], 1, "8xy6 VF is Vy bit 0");

    c.regs[1] = 0x01;
    c.regs[2] = 0x81;
    step(&c, 0x812E);
    CHECK_EQ(c.regs[1], 0x02, "8xyE shifts Vy left (vip)");
    CHECK_EQ(c.regs[0xF], 1, "8xyE VF is Vy bit 7");

    chip8_seed(&c, 999);
    step(&c, 0xC20F);
    CHECK_EQ(c.regs[2] & ~0x0F, 0, "Cxkk masks with kk");
}
