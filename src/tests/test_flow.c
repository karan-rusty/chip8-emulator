#include "test.h"

void test_flow(void)
{
    Chip8 c;
    chip8_init(&c);

    step(&c, 0x1ABC);
    CHECK_EQ(c.pc, 0xABC, "1nnn jumps");

    c.pc = PROG_BASE;
    step(&c, 0x2ABC);
    CHECK_EQ(c.pc, 0xABC, "2nnn calls");
    step(&c, 0x00EE);
    CHECK_EQ(c.pc, PROG_BASE + 2, "00EE returns");

    c.pc = PROG_BASE;
    for (int i = 0; i < STACK_SIZE * 25; i++)
        step(&c, 0x2ABC);
    CHECK_EQ(c.sp, UINT8_MAX, "sp saturates, never wraps");
    step(&c, 0x00EE);
    CHECK_EQ(c.sp, STACK_SIZE - 1, "pop collapses phantom frames");

    Chip8 s;
    chip8_init(&s);
    s.regs[3] = 0x42;
    CHECK_EQ(step_pc(&s, 0x3342), 4, "3xkk skips when equal");
    s.regs[2] = 7;
    s.regs[9] = 7;
    CHECK_EQ(step_pc(&s, 0x5290), 4, "5xy0 skips when equal");
    s.regs[9] = 8;
    CHECK_EQ(step_pc(&s, 0x9290), 4, "9xy0 skips when unequal");

    Chip8 j;
    chip8_init(&j);
    j.regs[0] = 0x10;
    step(&j, 0xB200);
    CHECK_EQ(j.pc, 0x210, "Bnnn adds V0 (vip)");

    Chip8 y;
    chip8_init(&y);
    y.i = 0x333;
    uint16_t pc = y.pc;
    step(&y, 0x0ABC);
    CHECK_EQ(y.pc, pc + 2, "0nnn ignored");
}
