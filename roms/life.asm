; CONWAY'S GAME OF LIFE for CHIP-8 (original instruction set only)
; Run: ./build/chip8 roms/life.ch8 -q modern -s 16000
;
; The playfield is a 16x16 grid of cells, each drawn as a solid 4x2 pixel
; block, so the grid exactly fills the 64x32 screen (cell x,y lands at
; pixel 4x,2y).  A cell is one byte, 0 or 1, living in one of two
; 256-byte buffers.  Every generation is computed into the "other" buffer
; and the two are swapped by anchor registers instead of being copied:
;
;   V8 = anchor of the buffer holding the *current* pattern (0 or 255)
;   VC = anchor of the buffer being *written*               (255 or 0)
;   VD = anchor RENDER draws: the buffer that was just written
;
; An anchor is an offset from GRID_A: 0 selects GRID_A, 255 selects
; GRID_B, which starts 255 bytes later.  The buffers therefore share one
; byte - A's cell 255 (row 15, col 15) is B's cell 0 (row 0, col 0).
; Both are border cells that every generation writes dead, so that byte
; is always 0 and the aliasing never shows.  Border rows/cols (0 and 15)
; are written dead each generation, which is also how out-of-range
; neighbours are handled: the read offsets simply run into them.
;
; UPDATE's registers (V8/VC are only read, never written):
;   V0 next cell / dead value    V1,V2 loaded triples
;   V3 the cell's own state      V4 offset P+16 (its own row)
;   V5 offset P+32 (next row)    V6 offset P (previous row)
;   V7 write offset, walks 0..255 across the whole grid in row order
;   V9 neighbour count           VA x counter   VB y counter
;
; Every Fx55/Fx65 reloads I from an explicit `LD I` first, so the ROM
; behaves the same under the vip and modern memory quirks (Fx55/Fx65
; advancing I).  Run it under `-q modern` anyway: vip's display-wait
; would draw only one of the ~196 redraw sprites per frame.
;
; Controls (terminal keypad, map in README):
;   W / Up     pause or resume
;   D / Right  step one generation while paused
;   A / Left   reseed with a random pattern
;   S / Down   toggle the pace (8 delay ticks <-> flat out)
;   Ctrl-C     quit

INIT:
  CLS
  LD V8, 0                  ; current buffer = GRID_A
  LD VC, 255                ; first generation writes GRID_B
  LD V0, 8
  LD I, PACING
  LD [I], V0                ; 8 delay ticks between generations
  CALL SEED                 ; random interior in GRID_A (edges stay 0)
  LD VD, V8
  CALL RENDER
  JP MAIN

MAIN:
  LD I, PAUSE
  LD V0, [I]
  SE V0, 0
  JP PAUSED
  CALL DOGEN
WAIT:
  LD I, PACING
  LD V0, [I]
  LD DT, V0
WLOOP:
  CALL KEYS                 ; poll every spin: a tap only lasts 3 frames
  LD V0, DT
  SE V0, 0
  JP WLOOP
  JP MAIN

PAUSED:
  LD I, STEP
  LD V0, [I]
  SNE V0, 0
  JP WAIT                    ; STEP still clear: just keep polling keys
  LD V0, 0
  LD I, STEP
  LD [I], V0                 ; consume the request
  CALL DOGEN
  JP WAIT

; --- one generation: update the current buffer into the other, swap, draw
DOGEN:
  CALL UPDATE
  LD VD, VC                  ; buffer just written becomes the current one
  LD VC, V8
  LD V8, VD                  ; swap the two anchors
  CALL RENDER
  RET

; --- neighbour count + rule for every cell; writes all 256 cells -------
; Interior (1..14) cells are computed; the border is written dead, which
; both keeps the buffers fully defined and gives dead off-grid neighbours.
UPDATE:
  LD V0, 0                   ; dead value
  LD V7, 0                   ; write offset walks 0..255
  LD V6, 0                   ; P     = (y-1)*16 + (x-1)
  LD V4, 16                  ; P + 16, this row
  LD V5, 32                  ; P + 32, next row
  LD VB, 0                   ; y
ROW:
  SE VB, 0
  JP ROW15
  JP EDGEROW
ROW15:
  SE VB, 15
  JP INTROW
EDGEROW:
  LD V0, 0
  LD VA, 0
ECELL:
  LD I, GRID_A
  ADD I, VC
  ADD I, V7
  LD [I], V0
  ADD V7, 1
  ADD VA, 1
  SE VA, 16
  JP ECELL
  JP ROWNEXT

INTROW:
  LD V0, 0
  LD I, GRID_A
  ADD I, VC
  ADD I, V7
  LD [I], V0                 ; x = 0, dead
  ADD V7, 1
  LD VA, 1
CELL:
  LD V9, 0
  LD I, GRID_A
  ADD I, V8
  ADD I, V6                  ; previous row, cells x-1..x+1
  LD V2, [I]
  ADD V9, V0
  ADD V9, V1
  ADD V9, V2
  LD I, GRID_A
  ADD I, V8
  ADD I, V4                  ; own row (its middle byte is the cell itself)
  LD V2, [I]
  ADD V9, V0
  LD V3, V1                  ; remember whether it is alive
  ADD V9, V2
  LD I, GRID_A
  ADD I, V8
  ADD I, V5                  ; next row
  LD V2, [I]
  ADD V9, V0
  ADD V9, V1
  ADD V9, V2
  LD V0, 0                   ; next state: dead unless a rule fires
  SE V9, 3
  JP NOT3
  LD V0, 1                   ; exactly 3: born either way
  JP STORE
NOT3:
  SE V3, 0                   ; dead cells need exactly 3 ...
  JP CENTERED
  JP STORE                   ; ... live ones keep going only on 2 or 3
CENTERED:
  SNE V9, 2
  LD V0, 1
STORE:
  LD I, GRID_A
  ADD I, VC
  ADD I, V7
  LD [I], V0
  ADD V6, 1
  ADD V4, 1
  ADD V5, 1
  ADD V7, 1
  ADD VA, 1
  SE VA, 15
  JP CELL
  ADD V6, 2                   ; step the read offsets to the next row
  ADD V4, 2
  ADD V5, 2
  LD V0, 0
  LD I, GRID_A
  ADD I, VC
  ADD I, V7
  LD [I], V0                  ; x = 15, dead
  ADD V7, 1
ROWNEXT:
  ADD VB, 1
  SE VB, 16
  JP ROW
  RET

; --- draw every live interior cell of VD's buffer as a 4x2 block -------
RENDER:
  CLS
  LD V6, 17                   ; offset of cell (1,1)
  LD V4, 4                    ; pixel x of column 1
  LD V5, 2                    ; pixel y of row 1
  LD VB, 1
RROW:
  LD VA, 1
RCOL:
  LD I, GRID_A
  ADD I, VD
  ADD I, V6
  LD V0, [I]
  SNE V0, 0
  JP RSKIP                    ; dead: skip the sprite
  LD I, SPR
  DRW V4, V5, 2
RSKIP:
  ADD V6, 1
  ADD V4, 4
  ADD VA, 1
  SE VA, 15
  JP RCOL
  ADD V6, 2
  LD V4, 4
  ADD V5, 2
  ADD VB, 1
  SE VB, 15
  JP RROW
  RET

; --- randomise the interior of the current buffer ---------------------
SEED:
  LD V6, 17
  LD VB, 1
SROW:
  LD VA, 1
SCOL:
  RND V0, 1
  LD I, GRID_A
  ADD I, V8
  ADD I, V6
  LD [I], V0
  ADD V6, 1
  ADD VA, 1
  SE VA, 15
  JP SCOL
  ADD V6, 2
  ADD VB, 1
  SE VB, 15
  JP SROW
  RET

; --- input; one latched action per physical press ---------------------
; VE carries the latch inside the routine so the action helpers may use
; V0 freely.  The latch clears only when every control key is up, so a
; held key cannot repeat, however often this is polled.
KEYS:
  LD I, LATCH
  LD V0, [I]
  LD VE, V0
  LD V1, 5                    ; W / Up: pause or resume
  SKP V1
  JP K7
  SE VE, 0
  JP K7
  CALL PAUSETOG
  JP SETLATCH
K7:
  LD V1, 7                    ; A / Left: reseed
  SKP V1
  JP K9
  SE VE, 0
  JP K9
  CALL SEED
  LD VD, V8
  CALL RENDER
  JP SETLATCH
K9:
  LD V1, 9                    ; D / Right: step, but only while paused
  SKP V1
  JP K8
  SE VE, 0
  JP K8
  LD I, PAUSE
  LD V0, [I]
  SNE V0, 0
  JP K8                      ; running: stepping makes no sense
  LD V0, 1
  LD I, STEP
  LD [I], V0
  JP SETLATCH
K8:
  LD V1, 8                    ; S / Down: pace toggle
  SKP V1
  JP KEND
  SE VE, 0
  JP KEND
  CALL PACETOG
SETLATCH:
  LD V0, 1
  LD I, LATCH
  LD [I], V0
  RET
KEND:
  LD V1, 5
  SKNP V1
  RET                         ; still down: keep the latch, stop looking
  LD V1, 7
  SKNP V1
  RET
  LD V1, 8
  SKNP V1
  RET
  LD V1, 9
  SKNP V1
  RET
  LD V0, 0
  LD I, LATCH
  LD [I], V0
  RET

PAUSETOG:
  LD I, PAUSE
  LD V0, [I]
  LD V2, 1
  XOR V0, V2
  LD I, PAUSE
  LD [I], V0
  RET

PACETOG:
  LD I, PACING
  LD V0, [I]
  LD V2, 8
  XOR V0, V2                   ; 8 <-> 0 (0 runs flat out)
  LD I, PACING
  LD [I], V0
  RET

SPR:
  DB 0xF0, 0xF0                ; one cell: 4 pixels wide, 2 rows tall
PAUSE:
  DS 1
STEP:
  DS 1
LATCH:
  DS 1
PACING:
  DS 1
; GRID_B sits 255 bytes after GRID_A so a single 8-bit anchor (0 or 255)
; reaches either buffer.  They overlap in exactly one byte: A's cell 255
; is B's cell 0, both border cells that are always written dead.
GRID_A:
  DS 255                       ; A cells 0..254 (255 is GRID_B's first byte)
GRID_B:
  DS 255                       ; B cells 1..255
