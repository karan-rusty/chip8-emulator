# chip8

A CHIP-8 interpreter in C. Original CHIP-8 instruction set only.

## Build

    make

## Test

    make test

`make test` runs 87 checks across 9 test functions with no external
framework.

### Against the reference suite

All seven ROMs in the [Timendus chip8-test-suite](https://github.com/Timendus/chip8-test-suite)
render pixel-for-pixel identical to the suite's own reference screenshots:

```
1-chip8-logo  2-ibm-logo  3-corax+  4-flags  6-keypad  7-beep   0 / 2048 px
5-quirks                                                          0 / 2048 px
```

`5-quirks` is not a pass/fail test — it prints which of the six quirks it
detected and ticks the ones matching the platform you pick. It waits at a
platform menu, so it needs `1` pressed to reach the report. See
[Quirk profile](#quirk-profile) below for what it reports here.

## Run

    ./build/chip8                     # built-in test ROM, text dump to stdout
    ./build/chip8 game.ch8            # play a ROM in the terminal
    ./build/chip8 --dump game.ch8     # disassemble it and exit
    make rom ROM=path/to/game.ch8     # same as `./build/chip8 path`

## Ping Pong (bundled ROM)

`roms/ping.asm` is a playable Ping Pong in original CHIP-8 assembly (688
bytes, VIP- and modern-safe). `roms/asm.py` assembles it, and
`roms/smoke_ping.py` runs 8 headless checks against a Python model of
`src/ops.c`.

    make ping                  # rebuild roms/ping.ch8
    make play-ping             # play: -s 120 -q modern
    python3 roms/smoke_ping.py # no terminal needed

Controls: `W`/`S` (or Up/Down arrows) move the left paddle; the right
paddle is a simple AI that tracks the ball. First to 5 shows the score
panel, clears, and restarts on any key. Note that `LD [I], Vx`/`LD Vx, [I]` stores/loads registers V0..Vx, so
the score digit at "SCORE+2" uses a stock `REG` pop and the ball loop
destroys it. Score rendering copies BCD to memory with `Fx33` whenever the
score changes, so the draw loop reads from RAM rather than the (overwritten)
right-score register.

## Snake (bundled ROM)

`roms/snake.asm` is a playable Snake in original CHIP-8 assembly (731 bytes,
VIP- and modern-safe). `roms/asm.py` assembles it, `roms/smoke_snake.py` runs
10 headless logic checks against a Python model of `src/ops.c`, and
`roms/test_pty.py` drives the real binary through a PTY (pixels, controls,
death screen).

    make snake                  # rebuild roms/snake.ch8
    make play-snake             # play: ./build/chip8 roms/snake.ch8 -s 120 -q modern
    python3 roms/smoke_snake.py # no terminal needed
    python3 roms/test_pty.py    # end-to-end through a fake terminal

The snake and the food are both 2x2 cells on the same 2px grid, so eating is
just "head lands on the food". The ROM polls the keypad continuously (not only
while it is waiting on the timer), so a tapped key registers even at the stock
`-s 10`.

Steer with `W`/`S`/`A`/`D` (up/down/left/right, case-insensitive, no
180-degree turns; arrow keys are ignored by the terminal backend). The snake
starts slow and speeds up as you eat. Eating beeps and grows the snake; wall
or self hit shows a bordered score panel (blinks twice, longer beep) and any
key restarts after a short grace. Stock vip quirks also work but draw one
sprite per frame, so run vip with `-q vip -s 200` for a steady picture.

## Game of Life (bundled ROM)

`roms/life.asm` is Conway's Game of Life in original CHIP-8 assembly (992
bytes). `roms/asm.py` assembles it and `roms/smoke_life.py` runs 39 headless
checks against a Python model of `src/ops.c` plus a reference Life
implementation.

    make life                   # rebuild roms/life.ch8
    make play-life              # play: -s 16000 -q modern
    python3 roms/smoke_life.py  # no terminal needed

The playfield is a 16x16 grid of cells, each drawn as a solid 4x2 pixel
block, so the grid exactly fills the 64x32 screen. A cell is one byte in one
of two 256-byte buffers; every generation computes into the other and the
two are swapped by anchor registers instead of being copied, so no buffer
clear is ever needed. The playfield edges are written dead every
generation, which is also how out-of-range neighbours are handled.

Controls: `W`/Up pauses and resumes, `D`/Right steps one generation while
paused, `A`/Left reseeds with a random pattern, `S`/Down toggles the pace
(8 delay ticks between generations, or flat out). One generation is about
10.4k instructions, so `-s 16000` fits the whole update and redraw in a
single frame and the screen never shows half a step. Run it with `-q
modern`: vip's display wait would draw one of the ~196 redraw sprites per
frame.

### Command line
    chip8 [options] [rom]

      -r, --rom FILE        load FILE as the ROM
      -L, --load-state FILE start from a saved machine state
      -S, --save-state FILE write machine state to FILE when the run ends
      -q, --quirks NAME     vip (default) or modern
      -s, --speed N         core cycles per frame (default 10)
      -d, --dump            disassemble the ROM and exit
      -h, --help            show this help

`CHIP8_FRONTEND` picks the backend and `CHIP8_QUIRKS` sets the default quirk
profile; the flags override the environment.

### Terminal controls

The CHIP-8 has a hex keypad, so it is mapped onto QWERTY:

```
CHIP-8      1 2 3 C          host      1 2 3 4
            4 5 6 D                    q w e r
            7 8 9 E                    a s d f
            A 0 B F                    z x c v
```

Quit with `Ctrl-C` or `Ctrl-D`. Uppercase letters fold to lowercase, so Caps
Lock does not silently dead-key the pad. Escape sequences (arrows, F-keys)
are swallowed, not mapped to keys.

## Layout

```
include/chip8.h        core state and API
include/disasm.h       disassembler API
include/frontend.h     backend interface and registry
include/rom.h          host-side ROM file I/O
include/state.h        host-side state file I/O

src/chip8.c            lifecycle, quirks, and state snapshots
src/ops.c              the decoder: all 35 opcodes
src/disasm.c           the disassembler
src/frontend.c         the backend registry
src/terminal.c         the terminal backend: termios, ANSI, keypad map
src/rom.c              ROM file loading and the headless text dump
src/state.c            save/load a machine state to a file
src/main.c             the generic driver loop and the entry point

src/tests/test.h       assertion macros and shared helpers
src/tests/test_main.c  runner
src/tests/test_*.c     test groups: core, alu, flow, draw, mem, io, quirks,
                       disasm, state
```

## Implementation

All 35 standard instructions (Cowgod's reference section 3.1) are implemented.
Anything else advances `pc` by 2 and changes nothing else.

### Frontends and state

State lives in exactly two places, and a new host backend never adds a third:

- **Machine state** is the `Chip8` struct. Only `chip8_*` touches it.
- **Host state** is one struct per backend, private to that backend's `.c`
  file. Nothing outside the backend can see it.

A backend fills in one `Frontend` (`include/frontend.h`): a name, its state
block, and six callbacks — `open`, `close`, `poll`, `present`, `sound`,
`wait_frame`. `main()` is a generic loop over that interface, so it never
mentions termios or ANSI. The terminal backend is `src/terminal.c`: raw
`termios` plus ANSI escapes — no SDL, no ncurses, nothing beyond libc. The
64x32 framebuffer is drawn as 128x16 characters using the half-block glyphs,
repainted at 60Hz with an absolute deadline so frame timing does not drift.

The core never reads stdin or wall time; a backend pushes both in through
`chip8_set_key()` and `chip8_tick_timers()` and pulls the framebuffer out of
the same `Chip8` it was handed. Adding a second backend means one new file plus
one line in `src/frontend.c`; `CHIP8_FRONTEND=name` selects it.

### Quirk profile

Where the platform dialects disagree, the six behaviours live in a `Quirks`
struct in machine state. The default `vip` preset does what the original
COSMAC VIP did; `--quirks modern` (`CHIP8_QUIRKS=modern`) selects the Chip-48
dialect. The default is reported as expected by the quirks test in the
[Timendus chip8-test-suite](https://github.com/Timendus/chip8-test-suite),
whose `5-quirks` ROM asks which platform you target and prints a tick per
quirk:

| Quirk (VIP behaviour) | vip | modern | Means |
| --- | --- | --- | --- |
| vF reset | on | off | `8xy1`/`8xy2`/`8xy3` clear `VF` after the operation |
| Memory | on | off | `Fx55`/`Fx65` advance `I` by `x + 1` |
| Display wait | on | off | `Dxyn` blocks until the next vertical blank |
| Clipping | on | on | a sprite overhanging the right or bottom edge is cut off |
| Shifting through Vy | on | off | `8xy6`/`8xyE` shift **`Vy`** and park the result in `Vx` |
| Jumping on V0 | on | off | `Bnnn` adds `V0`, not `Vx` |

"Display wait" is why drawing is capped at one sprite per frame: `Dxyn`
raises `c->vblank`, `chip8_run()` stops spending that frame's cycles, and
`chip8_tick_timers()` — which the frontend already calls once per frame —
clears it. This is a limit the real hardware had, not a concession.

Shifting through `Vy` is the one that catches people out, because Cowgod's
reference and most modern emulators shift `Vx` in place instead. `VF` takes
the outgoing bit of whichever register was shifted.

### Other decisions worth knowing about

- **`Dxyn` height is the low nibble** (`& 0x0F`), not the 12-bit `nnn` and not
  the low byte. `Dxyn` also sets **`VF` on collision**, which games use to
  bounce off walls.

- **There is no `F0nn` instruction.** Setting `I` is `Annn`. The `0xF` group is
  decoded purely by its low byte, so `F055` means *store V0 at I* and nothing
  else — `F007` reads `DT` into `V0`, `F018` loads `ST` from `V0`, and the
  247 low bytes that are not instructions are inert. An earlier revision of
  this code decoded `F0nn` as `I = nn`, which stole `F055`, `F007` and `F018`
  from every real ROM and left `Annn` unimplemented, so no ROM could set `I`
  at all.

- **An overflowing call stack saturates.** `sp` is a `uint8_t`, so a runaway
  call chain must stop at 255 rather than wrap to 0 and start overwriting
  `stack[0]` from below. `00EE` collapses any frames that overflowed the
  array before reading it, so a pop never indexes past the end.

### Memory map

`0x000`-`0x1FF` is interpreter space, holding the 80-byte hex font at
`0x000`. Programs load at `0x200` and may run to the end of memory, so
`Fx55`/`Fx65` can legitimately overwrite the font.

### Save states

`chip8_state_save()`/`chip8_state_load()` snapshot the whole machine — memory,
registers, stack, framebuffer, keys, timers, the RNG and the quirk profile —
into a 6226-byte, versioned, big-endian record. `--save-state FILE` writes one
when the run ends and `--load-state FILE` resumes from one. A wrong magic,
version or length is rejected rather than half-loaded.

### Disassembler

`--dump` disassembles the loaded ROM (`src/disasm.c`). Every implemented
opcode gets a mnemonic and anything else prints as `DW nnnn`, so a bad opcode
is visible rather than silently inert.

### Known limitations

- Terminals report key presses but never key releases, so a key stays down for
  3 frames after it is last seen. This is the one thing a real keypad does
  that a tty cannot.
- `Fx0A` waits for "a key is currently down" rather than a rising edge, for the
  same reason.
- No window and no audio device; the buzzer is a terminal `BEL` while `ST` is
  non-zero.

SCHIP/XO-CHIP additions (`Dx0`, `DxCN`, `00FB`-`00FE`, `Fx30`, `Fx75`/`Fx85`,
16-bit registers) are deliberately out of scope. A ROM that needs them will
look wrong rather than crash; treat that as "needs SCHIP", not as a decoder
fault.
