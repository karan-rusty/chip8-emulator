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

void test_core(void);
void test_alu(void);
void test_flow(void);
void test_draw(void);
void test_mem(void);
void test_io(void);
void test_quirks(void);
void test_disasm(void);
void test_state(void);

#endif
