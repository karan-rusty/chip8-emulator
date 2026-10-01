#include "test.h"

void test_display_wait(void)
{
    Chip8 c;
    chip8_init(&c);
    c.i = SCRATCH;
    c.mem[SCRATCH] = 0x80;
    c.mem[0x200] = 0x60; c.mem[0x201] = 0x01;
    c.mem[0x202] = 0xD0; c.mem[0x203] = 0x11;
    c.mem[0x204] = 0x61; c.mem[0x205] = 0x01;

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

void test_key_skips(void)
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

    Chip8 d;
    chip8_init(&d);
    d.regs[1] = 0x10;
    chip8_set_key(&d, 0x00, true);
    CHECK_EQ(step_pc(&d, 0xE19E), 2, "Vx=0x10 matches no key at all");

    for (unsigned k = 0; k < NUM_KEYS; k++) {
        Chip8 e;
        chip8_init(&e);
        e.regs[1] = (uint16_t)k;
        chip8_set_key(&e, k, true);
        CHECK_EQ(step_pc(&e, 0xE19E), 4, "key %u is reachable", k);
    }

    chip8_set_key(&d, NUM_KEYS, true);
    chip8_set_key(&d, 1000, true);
    int down = 0;
    for (unsigned k = 0; k < NUM_KEYS; k++)
        if (d.keys[k])
            down++;
    CHECK_EQ(down, 1, "out of range keys are ignored, never aliased");
}

void test_timers(void)
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

void test_wait_key(void)
{
    Chip8 c;
    chip8_init(&c);
    c.regs[2] = 0xAA;

    CHECK_EQ(step_pc(&c, 0xF20A), 0, "Fx0A stalls while no key is down");
    CHECK_EQ(c.regs[2], 0xAA, "Fx0A does not write Vx while it waits");
    CHECK_EQ(step_pc(&c, 0xF20A), 0, "Fx0A keeps stalling across cycles");

    chip8_set_key(&c, 0x0B, true);
    CHECK_EQ(step_pc(&c, 0xF20A), 2, "Fx0A releases once a key is down");
    CHECK_EQ(c.regs[2], 0x0B, "Fx0A stores the key that was pressed");

    Chip8 d;
    chip8_init(&d);
    chip8_set_key(&d, 0x07, true);
    d.regs[9] = 0;
    step(&d, 0xF90A);
    CHECK_EQ(d.regs[9], 0x07, "Fx0A writes the register named by x");
}
