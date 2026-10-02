#include "test.h"

void test_quirks(void)
{
    Chip8 c;
    chip8_init(&c);
    CHECK(c.quirks.vf_reset && c.quirks.mem_inc && c.quirks.shift_vy &&
              c.quirks.jump_v0 && c.quirks.clip && c.quirks.display_wait,
          "default is vip: all quirks on");

    QuirkPreset p;
    CHECK(chip8_quirk_parse("modern", &p) && p == QUIRKS_MODERN, "parse modern");
    CHECK(!chip8_quirk_parse("nope", &p), "reject unknown quirk");

    Chip8 s;
    chip8_init(&s);
    chip8_set_quirks(&s, QUIRKS_MODERN);
    s.regs[1] = 0x03;
    s.regs[2] = 0xFF;
    step(&s, 0x8126);
    CHECK_EQ(s.regs[1], 0x01, "modern shifts Vx in place");

    Chip8 j;
    chip8_init(&j);
    chip8_set_quirks(&j, QUIRKS_MODERN);
    j.regs[0] = 0x10;
    j.regs[5] = 0x20;
    step(&j, 0xB500);
    CHECK_EQ(j.pc, 0x520, "modern Bnnn adds Vx");

    Chip8 v;
    chip8_init(&v);
    chip8_set_quirks(&v, QUIRKS_MODERN);
    v.regs[1] = 0xF0;
    v.regs[2] = 0x0F;
    v.regs[0xF] = 0x5A;
    step(&v, 0x8121);
    CHECK_EQ(v.regs[0xF], 0x5A, "modern leaves VF alone");

    Chip8 m;
    chip8_init(&m);
    chip8_set_quirks(&m, QUIRKS_MODERN);
    m.i = SCRATCH;
    m.regs[0] = 0x11;
    step(&m, 0xF055);
    CHECK_EQ(m.i, SCRATCH, "modern Fx55 keeps I");
}
