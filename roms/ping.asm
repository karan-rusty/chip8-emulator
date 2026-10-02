; PING PONG for CHIP-8 (original instruction set only)
; Run: ./build/chip8 roms/ping.ch8 -s 120 -q modern
;
; Controls (terminal backend):
;   W / S = move left paddle up / down (CHIP-8 keys 5 / 8)
;   Arrow keys work the same: Up -> key 5, Down -> key 8.
;   Right paddle is a simple AI that tracks the ball.
; Any key restarts after a match win. Ctrl-C quits.
;
; Registers:
;   V2 py (left top)  V3 ay (right top)  V4 bx  V5 by
;   V6 dx (2/254)     V7 dy (2/254)      V8 lscore  VC rscore
;   V0/V1 scratch. VF flag only. VC/VB also used as draw pos.
; LSC and RSC are 3-byte BCD buffers; TMP+4 toggles the AI cadence,
; TMP+6/TMP+7 latch the W/S state, sampled at every wait-loop poll;
; INPUT consumes (and clears) them into TMP+2/TMP+3 each iteration.
; Pad tops are clamped to [0, 26] (paddle height 6).

INIT:
  CLS
  LD V0, 0
  LD V1, 0
  LD I, LSC
  LD [I], V1
  LD I, LSC+2
  LD [I], V0
  LD I, RSC
  LD [I], V1
  LD I, RSC+2
  LD [I], V0
  LD V8, 0
  LD VC, 0
  LD I, LSC
  LD B, V8
  LD I, RSC
  LD B, VC
  LD V2, 13
  LD V3, 13
  LD V4, 30
  LD V5, 14
  LD V6, 2
  LD V7, 2
  LD V0, 0
  LD I, TMP+4
  LD [I], V0

