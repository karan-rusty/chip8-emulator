#include "test.h"

void test_jump(void)
{
    Chip8 c;
    chip8_init(&c);

    step(&c, 0x1ABC);
    CHECK_EQ(c.pc, 0xABC, "1nnn jumps to nnn");
}

void test_call_return(void)
{
    Chip8 c;
    chip8_init(&c);

    c.pc = PROG_BASE;
    step(&c, 0x2ABC);
    CHECK_EQ(c.pc, 0xABC, "2nnn jumps to the subroutine");
    CHECK_EQ(c.sp, 1, "2nnn pushes a return address");

    step(&c, 0x00EE);
    CHECK_EQ(c.pc, PROG_BASE + 2, "00EE returns to the instruction after the call");
    CHECK_EQ(c.sp, 0, "00EE pops the stack");

    Chip8 d;
    chip8_init(&d);
    d.pc = 0x300;
    step(&d, 0x00EE);
    CHECK_EQ(d.pc, 0x302, "00EE on an empty stack is a no-op");
    CHECK_EQ(d.sp, 0, "sp must not go negative");
}

void test_stack_saturates(void)
{
    Chip8 c;
    chip8_init(&c);

    c.pc = PROG_BASE;
    for (int i = 0; i < STACK_SIZE * 3; i++)
        step(&c, 0x2ABC);
    CHECK(c.sp > STACK_SIZE, "sp is allowed past the top (uint8_t)");

    for (int i = 0; i < STACK_SIZE * 3; i++)
        step(&c, 0x00EE);
    CHECK_EQ(c.sp, 0, "sp returns to zero after balanced calls");

    Chip8 d;
    chip8_init(&d);
    d.pc = PROG_BASE;
    for (int i = 0; i < STACK_SIZE * 25; i++)
        step(&d, 0x2ABC);
    CHECK_EQ(d.sp, UINT8_MAX, "sp saturates rather than wrapping to 0");

    step(&d, 0x00EE);
    CHECK_EQ(d.sp, STACK_SIZE - 1, "the pop drops the phantom frames too");
    CHECK_EQ(d.pc, 0x0ABE, "the pop returns to the last stored address");
}

void test_jump_offset(void)
{
    Chip8 c;
    chip8_init(&c);
    c.regs[0] = 0x10;
    step(&c, 0xB200);
    CHECK_EQ(c.pc, 0x210, "Bnnn jumps to nnn + V0");

    Chip8 d;
    chip8_init(&d);
    d.regs[0] = 0xFF;
    step(&d, 0xBFF0);
    CHECK_EQ(d.pc, 0x0EF, "Bnnn wraps inside 12 bits");
}

void test_sys_is_ignored(void)
{
    Chip8 c;
    chip8_init(&c);
    c.i = 0x333;
    uint16_t pc = c.pc;

    step(&c, 0x0ABC);
    CHECK_EQ(c.pc, pc + 2, "0nnn does not jump");
    CHECK_EQ(c.i, 0x333, "0nnn does not set I");
}

void test_skips(void)
{
    Chip8 a;
    chip8_init(&a);
    a.regs[3] = 0x42;
    CHECK_EQ(step_pc(&a, 0x3342), 4, "3xkk skips when equal");
    CHECK_EQ(step_pc(&a, 0x3341), 2, "3xkk does not skip when unequal");

    Chip8 b;
    chip8_init(&b);
    b.regs[3] = 0x42;
    CHECK_EQ(step_pc(&b, 0x4341), 4, "4xkk skips when unequal");
    CHECK_EQ(step_pc(&b, 0x4342), 2, "4xkk does not skip when equal");

    Chip8 c;
    chip8_init(&c);
    c.regs[2] = 7;
    c.regs[9] = 7;
    CHECK_EQ(step_pc(&c, 0x5290), 4, "5xy0 skips when equal");
    CHECK_EQ(step_pc(&c, 0x5291), 2, "5xy1 does not exist and never skips");

    Chip8 d;
    chip8_init(&d);
    d.regs[2] = 7;
    d.regs[9] = 8;
    CHECK_EQ(step_pc(&d, 0x9290), 4, "9xy0 skips when unequal");
    CHECK_EQ(step_pc(&d, 0x9291), 2, "9xy1 does not exist and never skips");
    d.regs[9] = 7;
    CHECK_EQ(step_pc(&d, 0x9290), 2, "9xy0 does not skip when equal");

    Chip8 e;
    chip8_init(&e);
    e.regs[3] = 1;
    step(&e, 0x3301);
    CHECK_EQ(e.regs[3], 1, "a taken skip leaves Vx alone");
    CHECK_EQ(e.regs[0xF], 0, "a skip does not set VF");
}
