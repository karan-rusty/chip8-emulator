#ifndef TEST_H
#define TEST_H

#include "chip8.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define SCRATCH 0x100

extern int checks;
extern int failures;

#define CHECK(cond, ...)                                                    \
    do {                                                                    \
        checks++;                                                           \
        if (!(cond)) {                                                      \
            failures++;                                                     \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                     \
            printf(__VA_ARGS__);                                            \
            printf("\n");                                                   \
        }                                                                   \
    } while (0)
#define CHECK_EQ(got, want, ...)                                            \
    do {                                                                    \
        checks++;                                                           \
        long g_ = (long)(got), w_ = (long)(want);                           \
        if (g_ != w_) {                                                     \
            failures++;                                                     \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                     \
            printf(__VA_ARGS__);                                            \
            printf(" (got %ld, want %ld)\n", g_, w_);                       \
        }                                                                   \
    } while (0)
static inline void step(Chip8 *c, uint16_t opcode)
{
    c->mem[c->pc] = (uint8_t)(opcode >> 8);
    c->mem[(c->pc + 1) & 0x0FFF] = (uint8_t)(opcode & 0xFF);
    chip8_execute(c, opcode);
}

static inline int step_pc(Chip8 *c, uint16_t opcode)
{
    uint16_t before = c->pc;
    step(c, opcode);
    return (int)((c->pc - before) & 0x0FFF);
}

static inline int lit(const Chip8 *c)
{
    int n = 0;
    for (int i = 0; i < FB_SIZE; i++)
        n += c->fb[i];
    return n;
}

void test_init(void);
void test_font(void);
void test_font_pointer(void);
void test_load_rom(void);
void test_fetch(void);
void test_unknown_opcode_is_inert(void);
void test_pc_wraps(void);
void test_load_add(void);
void test_alu(void);
void test_alu_flags(void);
void test_logical_clears_vf(void);
void test_alu_vf_order(void);
void test_random(void);
void test_jump(void);
void test_call_return(void);
void test_stack_saturates(void);
void test_jump_offset(void);
void test_sys_is_ignored(void);
void test_skips(void);
void test_font_draw(void);
void test_draw(void);
void test_draw_row_count_is_a_nibble(void);
void test_draw_wraps(void);
void test_draw_clips(void);
void test_clear(void);
void test_draw_sets_collision(void);
void test_schip_draw_forms(void);
void test_load_i(void);
void test_add_to_i(void);
void test_bcd(void);
void test_mem_move(void);
void test_unknown_f_opcode_is_inert(void);
void test_display_wait(void);
void test_key_skips(void);
void test_timers(void);
void test_wait_key(void);

#endif
