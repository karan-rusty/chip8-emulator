#include "test.h"

void test_io(void)
{
    Chip8 c;
    chip8_init(&c);
    c.i = SCRATCH;
    c.mem[SCRATCH] = 0x80;
    c.mem[0x200] = 0xD0;
    c.mem[0x201] = 0x11;
    chip8_run(&c, 10);
    CHECK(c.vblank, "vip draw raises vblank");

    Chip8 k;
    chip8_init(&k);
    k.regs[1] = 0x0A;
    CHECK_EQ(step_pc(&k, 0xE19E), 2, "Ex9E up: no skip");
    chip8_set_key(&k, 0x0A, true);
    CHECK_EQ(step_pc(&k, 0xE19E), 4, "Ex9E down: skip");
    CHECK_EQ(step_pc(&k, 0xE1A1), 2, "ExA1 down: no skip");

    Chip8 t;
    chip8_init(&t);
    t.regs[3] = 5;
    step(&t, 0xF315);
    CHECK_EQ(t.dt, 5, "Fx15 sets DT");
    step(&t, 0xF318);
    CHECK(chip8_sound(&t), "sound while ST>0");
    chip8_tick_timers(&t);
    CHECK_EQ(t.dt, 4, "timers tick at 60Hz");

    Chip8 w;
    chip8_init(&w);
    w.regs[2] = 0xAA;
    CHECK_EQ(step_pc(&w, 0xF20A), 0, "Fx0A stalls empty");
    chip8_set_key(&w, 0x0B, true);
    CHECK_EQ(step_pc(&w, 0xF20A), 2, "Fx0A takes key");
    CHECK_EQ(w.regs[2], 0x0B, "Fx0A stores key");
}
