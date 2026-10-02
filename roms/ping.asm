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

MAIN:
  CLS
  CALL COURT
  CALL DRAWSCORE
  LD VC, 2
  LD VB, V2
  LD I, PAD
  DRW VC, VB, 6
  LD VC, 60
  LD VB, V3
  DRW VC, VB, 6
  LD I, BALL
  LD VC, V4
  LD VB, V5
  DRW VC, VB, 2
  CALL INPUT
  CALL AI
  CALL PHYS
  LD V0, 4
  LD DT, V0
DL_POLL:
  CALL KEYSAVE
DLEEP:
  LD V0, DT
  SE V0, 0
  JP DL_POLL
  JP MAIN

; --- left paddle on W/S (or up/down arrows), from the saved flags ---
INPUT:
  LD I, TMP+6
  LD V1, [I]      ; F165 x=1: V0 = TMP+6, V1 = TMP+7
  LD I, TMP+2
  LD [I], V1      ; F155 x=1: stash V0,V1 to TMP+2,TMP+3
  LD V0, 0
  LD V1, 0
  LD I, TMP+6
  LD [I], V1      ; F155: clear both flags
  LD I, TMP+2
  LD V1, [I]      ; F165 x=1: V0 = up-flag, V1 = down-flag
  LD VD, V1       ; preserve down-flag across V1 scratch
  SE V0, 1
  JP NO_UP
  LD V0, V2
  LD V1, 2
  SUB V0, V1
  SE VF, 1
  LD V0, 0
  LD V2, V0
NO_UP:
  SE VD, 1
  JP NO_DN
  LD V0, V2
  LD V1, 2
  ADD V0, V1
  LD V1, 26
  SUB V1, V0
  SE VF, 1
  LD V0, 26
  LD V2, V0
NO_DN:
  RET

; --- record the current W/S key state for this iteration ---
; key 5 = W/Up; key 8 = S/Down
KEYSAVE:
  LD V0, 5
  SKP V0
  JP DNCHK
  LD V0, 1
  LD I, TMP+6
  LD [I], V0
DNCHK:
  LD V0, 8
  SKP V0
  JP H_DONE
  LD V0, 1
  LD I, TMP+7
  LD [I], V0
H_DONE:
  RET

; --- AI moves every other rightward poll so it can be beaten ---
AI:
  SE V6, 2
  JP AIRET
  LD I, TMP+4
  LD V0, [I]
  SE V0, 0
  JP AI_SKIP
  LD V0, 1
  LD I, TMP+4
  LD [I], V0
  JP AI_MOVE
AI_SKIP:
  LD V0, 0
  LD I, TMP+4
  LD [I], V0
AIRET:
  RET
AI_MOVE:
  ; move up iff by+1 < ay+3 AND gap >= 4; i.e. up when (ay+3) - (by+1) >= 4
  LD V0, V3
  LD V1, 3
  ADD V0, V1
  LD V1, V5
  ADD V1, 1
  SUB V0, V1
  SE VF, 1
  JP AI_DOWN
  LD V1, 4
  SUB V0, V1
  SE VF, 1
  JP AIRET
  LD V0, V3
  LD V1, 2
  SUB V0, V1
  SE VF, 1
  LD V0, 0
  LD V3, V0
  RET
AI_DOWN:
  ; move down iff (by+1) - (ay+3) >= 4
  LD V1, V5
  ADD V1, 1
  LD V0, V3
  ADD V0, 3
  SUB V1, V0
  LD V0, 4
  SUB V1, V0
  SE VF, 1
  JP AIRET
  LD V0, V3
  LD V1, 2
  ADD V0, V1
  LD V1, 26
  SUB V1, V0
  SE VF, 1
  LD V0, 26
  LD V3, V0
  RET

; --- ball motion, walls, paddles, scoring ---
PHYS:
  ADD V4, V6
  SE V6, 254
  JP XPOS
  SE VF, 1
  JP LOST_L
XPOS:
  LD V0, V4
  LD V1, 61
  SUB V1, V0
  SE VF, 1
  JP LOST_R
  ADD V5, V7
  SE V7, 254
  JP VDOWN
  SE VF, 0
  JP VDOWN
  LD V5, 0
  LD V7, 2
VDOWN:
  LD V0, V5
  LD V1, 30
  SUB V1, V0
  SE VF, 0
  JP PADCHK
  LD V5, 30
  LD V7, 254
PADCHK:
  ; left paddle collision requires moving left and bx in [0, 5]
  SE V6, 254
  JP RPAD
  LD V0, V4
  LD V1, 5
  SUB V1, V0
  SE VF, 1
  JP RPAD
  LD V1, V5
  LD V0, V2
  SUB V1, V0
  SE VF, 1
  JP LCHK2
  LD V0, 5
  SUB V0, V1
  SE VF, 1
  JP RPAD
  JP L_HIT
LCHK2:
  LD V0, V2
  SUB V0, V5
  LD V1, 1
  SUB V1, V0
  SE VF, 1
  JP RPAD
  JP L_HIT
L_HIT:
  LD V4, 4
  LD V6, 2
  LD V0, 5
  LD ST, V0
  RET
RPAD:
; right paddle: needs V6==2 and bx >= 58
  SE V6, 2
  JP P_DONE
  LD V0, V4
  LD V1, 58
  SUB V0, V1
  SE VF, 1
  JP P_DONE
  LD V1, V5
  LD V0, V3
  SUB V1, V0
  SE VF, 1
  JP RCHK2
  LD V0, 5
  SUB V0, V1
  SE VF, 1
  JP P_DONE
  JP R_HIT
RCHK2:
  LD V0, V3
  SUB V0, V5
  LD V1, 1
  SUB V1, V0
  SE VF, 1
  JP P_DONE
  JP R_HIT
R_HIT:
  LD V4, 58
  LD V6, 254
  LD V0, 5
  LD ST, V0
  RET
P_DONE:
  RET
VBOK:
  RET
LOST_L:
  LD V0, 1
  ADD VC, V0
  LD I, RSC
  LD B, VC
  LD V6, 254
  CALL POINT
  RET
LOST_R:
  LD V0, 1
  ADD V8, V0
  LD I, LSC
  LD B, V8
  LD V6, 2
  CALL POINT
  RET

POINT:
; optional win check first
  SE V8, 5
  JP NOWINL
  ; left reached 5: win screen
  CLS
  LD V0, 60
  LD ST, V0
  CALL SHOWSCORE
  CALL BWAIT
  CALL SHOWSCORE
  CALL BWAIT
  CALL SHOWSCORE
  LD V0, K
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
  RET
NOWINL:
  SE VC, 5
  JP NOWINR
  CLS
  LD V0, 60
  LD ST, V0
  CALL SHOWSCORE
  CALL BWAIT
  CALL SHOWSCORE
  CALL BWAIT
  CALL SHOWSCORE
  LD V0, K
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
  RET
NOWINR:
  LD V0, 30
  LD ST, V0
  CLS
  LD VB, 10
  LD VC, 18
BTOP:
  CALL BDRAW
  LD V0, 2
  ADD VC, V0
  SE VC, 48
  JP BTOP
  LD VB, 22
  LD VC, 18
BBOT:
  CALL BDRAW
  LD V0, 2
  ADD VC, V0
  SE VC, 48
  JP BBOT
  LD VC, 18
  LD VB, 12
BSIDE:
  CALL BDRAW
  LD VC, 46
  CALL BDRAW
  LD VC, 18
  LD V0, 2
  ADD VB, V0
  SE VB, 22
  JP BSIDE
  CALL SHOWSCORE
  CALL BWAIT
  CALL SHOWSCORE
  CALL BWAIT
  CALL SHOWSCORE
  LD V0, 30
  LD DT, V0
RESET:
  LD V0, DT
  SE V0, 0
  JP RESET
  LD V2, 13
  LD V3, 13
  LD V4, 30
  LD V5, 14
  RET

BDRAW:
  LD I, RSQ
  DRW VC, VB, 2
  RET

SHOWSCORE:
  LD I, LSC+2
  LD V0, [I]
  LD F, V0
  LD VC, 24
  LD VB, 14
  DRW VC, VB, 5
  LD I, RSC+2
  LD V0, [I]
  LD F, V0
  LD VC, 34
  LD VB, 14
  DRW VC, VB, 5
  RET

BWAIT:
  LD V0, 20
  LD DT, V0
BWLOOP:
  LD V0, DT
  SE V0, 0
  JP BWLOOP
  RET

DRAWSCORE:
  LD I, LSC+2
  LD V0, [I]
  LD F, V0
  LD VC, 22
  LD VB, 2
  DRW VC, VB, 5
  LD I, RSC+2
  LD V0, [I]
  LD F, V0
  LD VC, 42
  LD VB, 2
  DRW VC, VB, 5
  RET

COURT:
  LD VC, 31
  LD VB, 0
CLINE:
  LD I, CDOT
  DRW VC, VB, 2
  LD V0, 4
  ADD VB, V0
  SE VB, 32
  JP CLINE
  RET

PAD:
  DB 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0
BALL:
  DB 0xC0, 0xC0
RSQ:
  DB 0xC0, 0xC0
CDOT:
  DB 0x80, 0x80
LSC:
  DS 3
RSC:
  DS 3
TMP:
  DS 8
