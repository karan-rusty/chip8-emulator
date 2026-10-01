# chip8

A CHIP-8 interpreter in C. Original CHIP-8 instruction set only.

## Build

    make

## Test

    make test

`make test` runs 468 checks across 37 test functions with no external
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
    make rom ROM=path/to/game.ch8     # play a ROM in the terminal

### Terminal controls

The CHIP-8 has a hex keypad, so it is mapped onto QWERTY:

```
CHIP-8      1 2 3 C          host      1 2 3 4
            4 5 6 D                    q w e r
            7 8 9 E                    a s d f
            A 0 B F                    z x c v
```

Quit with `Ctrl-C` or `Ctrl-D`. Uppercase `A`-`F` also register, so Caps Lock
does not silently dead-key the right-hand column.

## Implementation

All 35 standard instructions (Cowgod's reference section 3.1) are implemented.
Anything else advances `pc` by 2 and changes nothing else.

The frontend is raw `termios` plus ANSI escapes — no SDL, no ncurses, nothing
beyond libc. The 64x32 framebuffer is drawn as 128x16 characters using the
half-block glyphs, repainted at 60Hz with an absolute deadline so frame timing
does not drift. The core never reads stdin or wall time; the frontend pushes
both in via `chip8_set_key()` and `chip8_tick_timers()`.

### Quirk profile

Where the platform dialects disagree, this build does what the original
COSMAC VIP did. All six are reported as expected by the quirks test in the
[Timendus chip8-test-suite](https://github.com/Timendus/chip8-test-suite),
whose `5-quirks` ROM asks which platform you target and prints a tick per
quirk:

| Quirk | This build | Means |
| --- | --- | --- |
| vF reset | on | `8xy1`/`8xy2`/`8xy3` clear `VF` after the operation |
| Memory | on | `Fx55`/`Fx65` advance `I` by `x + 1` |
| Display wait | on | `Dxyn` blocks until the next vertical blank |
| Clipping | on | a sprite overhanging the right or bottom edge is cut off |
| Shifting | off | `8xy6`/`8xyE` shift **`Vy`** and park the result in `Vx` |
| Jumping | off | `Bnnn` adds `V0`, not `Vx` |

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
