#include "test.h"

void test_load_add(void)
{
    Chip8 c;
    chip8_init(&c);

    step(&c, 0x6010);
    CHECK_EQ(c.regs[0], 0x10, "6xnn sets VX to nn");

    step(&c, 0x7005);
    CHECK_EQ(c.regs[0], 0x15, "7xnn adds to VX");

    step(&c, 0x7005);
    CHECK_EQ(c.regs[0], 0x1A, "7xnn accumulates");

    Chip8 w;
    chip8_init(&w);
    w.regs[0] = 0xFF;
    step(&w, 0x7001);
    CHECK_EQ(w.regs[0], 0x00, "7xnn must wrap at 8 bits like 6xnn stores");

    Chip8 d;
    chip8_init(&d);
    step(&d, 0x6A0F);
    CHECK_EQ(d.regs[0xA], 0x0F, "6xnn targets Vx from the middle nibble");
    CHECK_EQ(d.regs[0], 0, "6xnn must not touch V0");

    Chip8 e;
    chip8_init(&e);
    step(&e, 0x60FF);
    CHECK_EQ(e.regs[0], 0xFF, "6xnn must discard the high nibble");
}

void test_alu(void)
{
    Chip8 c;
    chip8_init(&c);

    c.regs[1] = 0xF0;
    c.regs[2] = 0x0F;
    step(&c, 0x8120);
    CHECK_EQ(c.regs[1], 0x0F, "8xy0 copies Vy into Vx");
    CHECK_EQ(c.regs[2], 0x0F, "8xy0 leaves Vy alone");

    c.regs[1] = 0x0F;
    c.regs[2] = 0xF0;
    step(&c, 0x8121);
    CHECK_EQ(c.regs[1], 0xFF, "8xy1 ORs");

    c.regs[1] = 0x0F;
    c.regs[2] = 0xF0;
    step(&c, 0x8122);
    CHECK_EQ(c.regs[1], 0x00, "8xy2 ANDs");

    c.regs[1] = 0xFF;
    c.regs[2] = 0x0F;
    step(&c, 0x8123);
    CHECK_EQ(c.regs[1], 0xF0, "8xy3 XORs");

    Chip8 d;
    chip8_init(&d);
    d.regs[1] = 0x42;
    d.regs[2] = 0x99;
    step(&d, 0x8129);
    CHECK_EQ(d.regs[1], 0x42, "8xy9 does not exist and changes nothing");
    CHECK_EQ(d.regs[0xF], 0, "8xy9 does not set VF");
}

void test_alu_flags(void)
{
    Chip8 a;
    chip8_init(&a);
    a.regs[1] = 0xF0;
    a.regs[2] = 0x20;
    step(&a, 0x8124);
    CHECK_EQ(a.regs[1], 0x10, "8xy4 keeps the low 8 bits");
    CHECK_EQ(a.regs[0xF], 1, "8xy4 sets VF on carry");

    Chip8 b;
    chip8_init(&b);
    b.regs[1] = 0x10;
    b.regs[2] = 0x20;
    step(&b, 0x8124);
    CHECK_EQ(b.regs[1], 0x30, "8xy4 sums");
    CHECK_EQ(b.regs[0xF], 0, "8xy4 clears VF without a carry");

    Chip8 c;
    chip8_init(&c);
    c.regs[1] = 0x50;
    c.regs[2] = 0x30;
    step(&c, 0x8125);
    CHECK_EQ(c.regs[1], 0x20, "8xy5 subtracts");
    CHECK_EQ(c.regs[0xF], 1, "8xy5 VF is 1 when there is no borrow");

    Chip8 d;
    chip8_init(&d);
    d.regs[1] = 0x30;
    d.regs[2] = 0x50;
    step(&d, 0x8125);
    CHECK_EQ(d.regs[1], 0xE0, "8xy5 wraps on borrow");
    CHECK_EQ(d.regs[0xF], 0, "8xy5 VF is 0 when it borrows");

    Chip8 e;
    chip8_init(&e);
    e.regs[1] = 0x30;
    e.regs[2] = 0x50;
    step(&e, 0x8127);
    CHECK_EQ(e.regs[1], 0x20, "8xy7 does Vy - Vx");
    CHECK_EQ(e.regs[0xF], 1, "8xy7 VF is 1 when there is no borrow");

    Chip8 f;
    chip8_init(&f);
    f.regs[1] = 0x50;
    f.regs[2] = 0x30;
    step(&f, 0x8127);
    CHECK_EQ(f.regs[1], 0xE0, "8xy7 wraps on borrow");
    CHECK_EQ(f.regs[0xF], 0, "8xy7 VF is 0 when it borrows");

    Chip8 g;
    chip8_init(&g);
    g.regs[1] = 0x03;
    g.regs[2] = 0xFF;
    step(&g, 0x8126);
    CHECK_EQ(g.regs[1], 0x7F, "8xy6 shifts Vy into Vx, not Vx in place");
    CHECK_EQ(g.regs[0xF], 1, "8xy6 VF takes Vy's old bit 0");

    Chip8 h;
    chip8_init(&h);
    h.regs[1] = 0xFF;
    h.regs[2] = 0x04;
    step(&h, 0x8126);
    CHECK_EQ(h.regs[1], 0x02, "8xy6 shifts Vy right");
    CHECK_EQ(h.regs[0xF], 0, "8xy6 VF is 0 for an even Vy");

    Chip8 i;
    chip8_init(&i);
    i.regs[1] = 0x01;
    i.regs[2] = 0x81;
    step(&i, 0x812E);
    CHECK_EQ(i.regs[1], 0x02, "8xyE shifts Vy left into Vx");
    CHECK_EQ(i.regs[0xF], 1, "8xyE VF takes Vy's old bit 7");

    Chip8 j;
    chip8_init(&j);
    j.regs[1] = 0x80;
    j.regs[2] = 0x01;
    step(&j, 0x812E);
    CHECK_EQ(j.regs[1], 0x02, "8xyE shifts Vy left");
    CHECK_EQ(j.regs[0xF], 0, "8xyE VF is 0 when Vy's bit 7 was clear");
}

void test_logical_clears_vf(void)
{
    Chip8 c;
    chip8_init(&c);
    c.regs[1] = 0xF0;
    c.regs[2] = 0x0F;
    c.regs[0xF] = 0x5A;
    step(&c, 0x8121);
    CHECK_EQ(c.regs[1], 0xFF, "8xy1 still ORs");
    CHECK_EQ(c.regs[0xF], 0, "8xy1 clears VF");

    Chip8 d;
    chip8_init(&d);
    d.regs[1] = 0xFF;
    d.regs[2] = 0x0F;
    d.regs[0xF] = 0x5A;
    step(&d, 0x8122);
    CHECK_EQ(d.regs[1], 0x0F, "8xy2 still ANDs");
    CHECK_EQ(d.regs[0xF], 0, "8xy2 clears VF");

    Chip8 e;
    chip8_init(&e);
    e.regs[1] = 0xFF;
    e.regs[2] = 0x0F;
    e.regs[0xF] = 0x5A;
    step(&e, 0x8123);
    CHECK_EQ(e.regs[1], 0xF0, "8xy3 still XORs");
    CHECK_EQ(e.regs[0xF], 0, "8xy3 clears VF");

    Chip8 f;
    chip8_init(&f);
    f.regs[2] = 0x37;
    f.regs[0xF] = 0x5A;
    step(&f, 0x8120);
    CHECK_EQ(f.regs[1], 0x37, "8xy0 still copies");
    CHECK_EQ(f.regs[0xF], 0x5A, "8xy0 leaves VF alone");
}

void test_alu_vf_order(void)
{
    Chip8 a;
    chip8_init(&a);
    a.regs[0xF] = 0xF0;
    a.regs[2] = 0x20;
    step(&a, 0x8F24);
    CHECK_EQ(a.regs[0xF], 1, "8xy4 with x=VF ends holding the carry");

    Chip8 b;
    chip8_init(&b);
    b.regs[0xF] = 0xF0;
    b.regs[2] = 0x20;
    step(&b, 0x8F25);
    CHECK_EQ(b.regs[0xF], 1, "8xy5 with x=VF ends holding NOT borrow");

    Chip8 c;
    chip8_init(&c);
    c.regs[4] = 0x81;
    step(&c, 0x844E);
    CHECK_EQ(c.regs[4], 0x02, "8xyE with x==y shifts the original value");
    CHECK_EQ(c.regs[0xF], 1, "8xyE with x==y still sets VF");
}

void test_random(void)
{
    Chip8 a, b;
    chip8_init(&a);
    chip8_init(&b);
    chip8_seed(&a, 12345);
    chip8_seed(&b, 12345);

    step(&a, 0xC1FF);
    step(&b, 0xC1FF);
    CHECK_EQ(a.regs[1], b.regs[1], "the same seed must give the same value");
    CHECK_EQ(a.regs[1] & 0xFF, a.regs[1], "Cxkk result fits in 8 bits");

    chip8_seed(&a, 999);
    step(&a, 0xC20F);
    CHECK_EQ(a.regs[2] & ~0x0F, 0, "Cxkk masks the random byte with kk");

    Chip8 c;
    chip8_init(&c);
    step(&c, 0xC300);
    CHECK_EQ(c.regs[3], 0, "Cxkk with kk=0 always yields 0");

    Chip8 d;
    chip8_init(&d);
    chip8_seed(&d, 0);
    step(&d, 0xC4FF);
    uint16_t first = d.regs[4];
    step(&d, 0xC4FF);
    CHECK(d.regs[4] != first, "a zero seed still yields a moving stream");
}
