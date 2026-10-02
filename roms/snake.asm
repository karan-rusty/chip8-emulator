; SNAKE for CHIP-8 (original instruction set only)
; Works on both vip (default) and modern quirk profiles.
;
; Controls (terminal backend):
;   W = up     (CHIP-8 key 5)
;   S = down   (CHIP-8 key 8)
;   A = left   (CHIP-8 key 7)
;   D = right  (CHIP-8 key 9)
; Any key restarts after game over. Ctrl-C quits.
;
; Run:
;   ./build/chip8 roms/snake.ch8                 (defaults are fine; polls input
;                                                every frame so taps register)
;   ./build/chip8 roms/snake.ch8 -q modern       (all sprites per frame)
;
; Registers:
;   V3 dir (0=R 1=D 2=L 3=U)  V4 len  V5/V6 food  V7/V8 head
;   V9 score  VC tailIdx  VD loopIdx  VE counter  V2 food-flag
;   V0/V1 transfer+scratch. VA/VB new-head + draw pos. VF flag only.
; Body is a 32-slot circular buffer of (x,y) pairs at BODY. TMP is 2B scratch.

INIT:
  CLS
  LD V3, 0
  LD V4, 4
  LD V9, 0
  LD V2, 0
  LD VC, 0
  LD V7, 32
  LD V8, 16
  LD V0, 26
  LD V1, 16
  LD I, BODY
  LD [I], V1
  LD V0, 28
  LD V1, 16
  LD I, BODY+2
  LD [I], V1
  LD V0, 30
  LD V1, 16
  LD I, BODY+4
  LD [I], V1
  LD V0, 32
  LD V1, 16
  LD I, BODY+6
  LD [I], V1
  CALL SPAWN
  LD VE, 0
DRAWALL:
  LD V0, VE
  LD V1, VE
  ADD V0, V1
  LD I, BODY
  ADD I, V0
  LD V1, [I]
  LD VA, V0
  LD VB, V1
  CALL DRAW
  LD V0, 1
  ADD VE, V0
  SE VE, V4
  JP DRAWALL
  LD VA, V5
  LD VB, V6
  CALL DRAWF

MAIN:
  CALL INPUT
; pace: DT = max(22 - score, 6), faster as you eat
  LD V0, 22
  SUB V0, V9
  SE VF, 0
  JP DLSET
  LD V0, 6
DLSET:
  LD DT, V0
DWAIT:
  CALL INPUT
  LD V0, DT
  SE V0, 0
  JP DWAIT
  LD V2, 0
  CALL INPUT
; --- new head into VA/VB ---
  LD VA, V7
  LD VB, V8
  SE V3, 0
  JP NOTR
  LD V0, 2
  ADD VA, V0
  JP MOVED
NOTR:
  SE V3, 1
  JP NOTD
  LD V0, 2
  ADD VB, V0
  JP MOVED
NOTD:
  SE V3, 2
  JP NOTL
  LD V0, 2
  SUB VA, V0
  JP MOVED
NOTL:
  LD V0, 2
  SUB VB, V0
MOVED:
; --- wall check (unsigned >=) ---
  LD V0, VA
  LD V1, 64
  SUB V0, V1
  SE VF, 1
  JP YCHK
  JP DEAD
YCHK:
  LD V0, VB
  LD V1, 32
  SUB V0, V1
  SE VF, 1
  JP SELFCHK
  JP DEAD

; --- self check vs ring slots (tail+i)&31, i=0..len-1 ---
SELFCHK:
  LD VD, 0
SCHK:
  CALL INPUT
  LD V0, VC
  LD V1, VD
  ADD V0, V1
  LD V1, 31
  AND V0, V1
  LD V1, V0
  ADD V0, V1
  LD I, BODY
  ADD I, V0
  LD V1, [I]
  SNE V1, VA
  JP NEXTSEG
  LD V0, 1
  ADD I, V0
  LD V1, [I]
  SNE V1, VB
  JP DEAD
  JP NEXTSEG
NEXTSEG:
  LD V0, 1
  ADD VD, V0
  SE VD, V4
  JP SCHK

; --- food check: eat iff the new head sits exactly on the 2x2 food ---
; (snake and food are both 2x2 on the same 2px grid, so "on the food" is
;  an exact cell match -- no corner alignment needed)
FOODCHK:
  CALL INPUT
  SNE VA, V5
  JP CHKY
  JP MOVE
CHKY:
  SNE VB, V6
  JP EAT
  JP MOVE

EAT:
  LD V0, 1
  ADD V9, V0
  LD V0, 20
  LD ST, V0
  SNE V4, 32
  JP EATFULL
; grow: slot = (tail+len)%32 using old len
  LD V0, VC
  LD V1, V4
  ADD V0, V1
  LD V1, 31
  AND V0, V1
  LD VD, V0
  LD V0, 1
  ADD V4, V0
  JP EATSTORE
EATFULL:
; full (len==32): no growth, so drop the tail. Erase the eaten 2x2 food
; first (keep the new head in VA/VB for MOVE), then respawn.
  LD V0, VA
  LD V1, VB
  LD VA, V5
  LD VB, V6
  CALL DRAWF
  LD VA, V0
  LD VB, V1
  CALL SPAWN
  LD V2, 1
  JP MOVE
EATSTORE:
  CALL INPUT
; store new head at slot VD
  LD V0, VD
  LD V1, VD
  ADD V0, V1
  LD I, BODY
  ADD I, V0
  LD V0, VA
  LD V1, VB
  LD [I], V1
  LD V7, VA
  LD V8, VB
; erase old food cell, draw head, spawn+draw new food
  LD VA, V5
  LD VB, V6
  CALL DRAWF
  LD VA, V7
  LD VB, V8
  CALL DRAW
  CALL SPAWN
  LD VA, V5
  LD VB, V6
  CALL DRAWF
  JP MAIN

MOVE:
  CALL INPUT
; stash new head in TMP (VA/VB survive F155)
  LD V0, VA
  LD V1, VB
  LD I, TMP
  LD [I], V1
; load tail coords
  LD V0, VC
  LD V1, VC
  ADD V0, V1
  LD I, BODY
  ADD I, V0
  LD V1, [I]
  LD VA, V0
  LD VB, V1
  CALL DRAW
; reload new head
  LD I, TMP
  LD V1, [I]
  LD VA, V0
  LD VB, V1
; store new head at ring slot (tail+len)&31 -- NOT the tail slot, or the
; ring never frees and VC walks off into stale slots after `len` moves
  LD V0, VC
  LD V1, V4
  ADD V0, V1
  LD V1, 31
  AND V0, V1
  LD V1, V0
  ADD V0, V1
  LD I, BODY
  ADD I, V0
  LD V0, VA
  LD V1, VB
  LD [I], V1
  LD V7, VA
  LD V8, VB
; tail = (tail+1)&31
  LD V0, VC
  LD V1, 1
  ADD V0, V1
  LD V1, 31
  AND V0, V1
  LD VC, V0
; draw new head
  LD VA, V7
  LD VB, V8
  CALL DRAW
; EATFULL pending: draw the new food too
  SNE V2, 0
  JP MAIN
  LD VA, V5
  LD VB, V6
  CALL DRAWF
  JP MAIN

; --- WASD into V3 (never reverse) ---
INPUT:
  LD V0, 5
  SKP V0
  JP C_DOWN
  SE V3, 1
  LD V3, 3
C_DOWN:
  LD V0, 8
  SKP V0
  JP C_LEFT
  SE V3, 3
  LD V3, 1
C_LEFT:
  LD V0, 7
  SKP V0
  JP C_RIGHT
  SE V3, 0
  LD V3, 2
C_RIGHT:
  LD V0, 9
  SKP V0
  JP IN_DONE
  SE V3, 2
  LD V3, 0
IN_DONE:
  RET

SPAWN:
  RND V0, 31
  ADD V0, V0
  LD V5, V0
  RND V0, 15
  ADD V0, V0
  LD V6, V0
; reroll while the food sits on the body (else XOR punches a hole, not food)
  LD VD, 0
SPCHK:
  CALL INPUT
  LD V0, VC
  LD V1, VD
  ADD V0, V1
  LD V1, 31
  AND V0, V1
  LD V1, V0
  ADD V0, V1
  LD I, BODY
  ADD I, V0
  LD V1, [I]
  SNE V0, V5
  JP SPYCH
  JP SPNEXT
SPYCH:
  SNE V1, V6
  JP SPAWN
  JP SPNEXT
SPNEXT:
  LD V0, 1
  ADD VD, V0
  SE VD, V4
  JP SPCHK
  RET

DRAW:
  LD I, SPR
  DRW VA, VB, 2
  RET

DRAWF:
  LD I, FOOD
  DRW VA, VB, 2
  RET

DEAD:
  LD V0, 60
  LD ST, V0
  CLS
; border panel x 18..46, y 10..22 (sides skip corners: XOR would erase them)
  LD VB, 10
  LD VA, 18
BTOP:
  CALL DRAW
  LD V0, 2
  ADD VA, V0
  SE VA, 48
  JP BTOP
  LD VB, 22
  LD VA, 18
BBOT:
  CALL DRAW
  LD V0, 2
  ADD VA, V0
  SE VA, 48
  JP BBOT
  LD VA, 18
  LD VB, 12
BSIDE:
  CALL DRAW
  LD VA, 46
  CALL DRAW
  LD VA, 18
  LD V0, 2
  ADD VB, V0
  SE VB, 22
  JP BSIDE
; score digits, blinked twice so the panel reads as game-over
  LD I, SCORE
  LD B, V9
  CALL SHOWSCORE
  CALL BWAIT
  CALL SHOWSCORE
  CALL BWAIT
  CALL SHOWSCORE
; grace: let the death key release before Fx0A or it restarts instantly
  LD V0, 30
  LD DT, V0
GRACE:
  LD V0, DT
  SE V0, 0
  JP GRACE
  LD V0, K
  JP INIT

SHOWSCORE:
  LD I, SCORE
  LD V0, [I]
  LD F, V0
  LD VA, 24
  LD VB, 14
  DRW VA, VB, 5
  LD I, SCORE+1
  LD V0, [I]
  LD F, V0
  LD VA, 29
  LD VB, 14
  DRW VA, VB, 5
  LD I, SCORE+2
  LD V0, [I]
  LD F, V0
  LD VA, 34
  LD VB, 14
  DRW VA, VB, 5
  RET

BWAIT:
  LD V0, 20
  LD DT, V0
BWLOOP:
  LD V0, DT
  SE V0, 0
  JP BWLOOP
  RET

SPR:
  DB 0xC0, 0xC0
FOOD:
  DB 0xC0, 0xC0
BODY:
  DS 64
TMP:
  DS 2
SCORE:
  DS 3
