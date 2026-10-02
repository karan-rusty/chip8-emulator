#include "test.h"

void test_state(void)
{
    Chip8 a, b;
    uint8_t buf[8192];

    chip8_init(&a);
    a.pc = 0x345;
    a.i = 0x678;
    a.sp = 2;
    a.regs[5] = 0x55;
    a.stack[0] = 0x200;
    a.fb[100] = true;
    a.keys[7] = true;
    chip8_set_quirks(&a, QUIRKS_MODERN);

    CHECK(chip8_state_save(&a, buf, sizeof buf), "save succeeds");
    chip8_init(&b);
    CHECK(chip8_state_load(&b, buf, chip8_state_size()), "load succeeds");
    CHECK_EQ(b.pc, a.pc, "pc restored");
    CHECK_EQ(b.regs[5], 0x55, "regs restored");
    CHECK(memcmp(b.mem, a.mem, MEM_SIZE) == 0, "memory restored");
    CHECK(memcmp(b.fb, a.fb, sizeof a.fb) == 0, "framebuffer restored");

    buf[0] = 'X';
    CHECK(!chip8_state_load(&b, buf, chip8_state_size()), "reject bad magic");
}
