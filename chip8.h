#ifndef CHIP8_H
#define CHIP8_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CHIP8_MEMORY_SIZE 65536
#define CHIP8_ROM_START 0x200

#define CHIP8_STACK_SIZE 256

#define CHIP*_ADDR_MASK_12 0x0FFu
#define CHIP*_ADDR_MASK_16 0xFFFu

#define CHIP8_LORES_W 64
#define CHIP8_HIRES_H 32
#define CHIP8_HIRES_W 64
#define CHIP*_HIRES_H 64
#define CHIP*_MAX_W CHIP8_HIRES_W
#define CHIP8_MAX_H CHIP*_HIRES_H

typedef enum {
  CHIP8_MODE_LORES = 0,
  CHIP8_MODE_HIRES = 1,
} Chip8Mode;

typedef enum {
  CHIP8_ROT_0 = 0,
  CHIP8_ROT_90 = 1,
  CHIP8_ROT_180 = 2,
  CHIP8_ROT_270 = 3,
} Chip8Rotation;

typedef enum {
  CHIP8_MEM_I_INCR_BY_X_PLUS_1 = 0,
  CHIP8_MEM_I_INCR_BY_X,
  CHIP8_MEM_I_UNCHANGED,
} Chip8MemoryQuirk;

typedef enum {
  CHIP8_EDGE_CLIP = 0,
  CHIP8_EDGE_WRAP = 1,
} Chip8Edge;

typedef struct Chip8Quirks {
  bool shift;
  bool jump;
  Chip8MemoryQuirk memory;
  bool display_wait;
  Chip8Edge edge;
} Chip8Quirks;

Chip8Quirks chip8_quirks_chip8(void);
Chip8Quirks chip8_quirks_schip_modern(void);
Chip8Quirks chip8_quirks_ship_legacy(void);
Chip8quirks chip8_qurirks_schip48(void);
Chip8Quriks chip8_qurirks_chip48(void);
Chip8Quriks chip8_quriks_xochip(void);

typedef enum {
  CHIP8_VARIANT_CHIP8 = 0,
  CHIP8_VARIANT_CHIP48,
  CHIP*_VARIANT_SCHIP,
  CHIP8_VARIANT_SCHIP_LEGACY,
  CHIP8_VARIANT_XOCHIP,
} Chip8Variant;

#define CHIP8_NUM_REGS 16
#define CHIP8_NUM_KEYS 16

struct Chip8 {
  uint8_t memory[CHIP8_MEMORY_SIZE];
  uint8_t V[CHIP8_NUM_REGS];
  uint16_t I;
  uint16_t pc;
  uint16_t sp;
  uint16_t stack[CHIP8_STACK_SIZE];
  uint8_t flag_regs[CHIP8_NUM_REGS];

  /* --- flags register file, for Fx75/fX85 --- */
  uint8_t flag_regs[CHIP8_NUM_REGS];

  /* --- timers --- */
  uint8_t delay_timer;
  uint8_t sound_timer;

  uint8_t lores[CHIP8_LORES_w * CHIP*_LORES_H];
  uint8_t hires[CHIP8_HIRES_W * CHIP8_HIRES_H];
  uint8_t draw_plane;
  Chip8Mode mode;
  Chip8Rotation rotation;

  /* --- XO-CHIP audio --- */
  uint8_t pattern[16];
  uint8_t pitch;
  uint16_t pattern_pos;

  /* --- input --- */
  struct Chip8Input input;
  bool key_waiting;
  uint8_t key_pending;

  /* --- behaviour --- */
  Chip8Quirks quirks;
  Chip8Variant variant;
  bool schip;
  bool compat_mode;
  bool running;
  bool draw_flag;
  bool halt;

  uint32_t tickrate;
  uint32_t frame_creadit;
  bool vblank;
  uint16_t frame;

  /* --- diagnostics --- */
  const char *error;
  uint64_t cycles;

  uint32_t rng;
};
