# chip8

A CHIP-8 interpreter in C. Original CHIP-8 instruction set only.

## Build & Test

    make
    make test          # 87 checks, no external framework

## Play

    make play          # game selection menu
    make play-ping     # Ping Pong
    make play-snake    # Snake
    make play-life     # Game of Life

Or load your own ROM:

    ./build/chip8 game.ch8            # play in terminal
    ./build/chip8 --dump game.ch8     # disassemble and exit

## Controls

```
CHIP-8      1 2 3 C          host      1 2 3 4
            4 5 6 D                    q w e r
            7 8 9 E                    a s d f
            A 0 B F                    z x c v
```

| Game | Keys |
|------|------|
| Ping Pong | `W`/`S` or Up/Down |
| Snake | `W`/`A`/`S`/`D` |
| Game of Life | `W` pause, `D` step, `A` reseed, `S` pace |

Quit with `Ctrl-C` or `Ctrl-D`.

## Command Line

    chip8 [options] [rom]

      -r, --rom FILE        load FILE as the ROM
      -L, --load-state FILE start from a saved machine state
      -S, --save-state FILE write machine state to file when the run ends
      -q, --quirks NAME     vip (default) or modern
      -s, --speed N         core cycles per frame (default 10)
      -d, --dump            disassemble the ROM and exit
      -m, --menu            show the game selection menu
      -h, --help            show this help

## Layout

    include/               headers: chip8, disasm, frontend, rom, state
    src/                   core, terminal backend, driver, tests
    roms/                  game assembly, assembler, smoke tests

## Implementation

All 35 standard instructions (Cowgod's reference section 3.1) are implemented.
Anything else advances `pc` by 2 and changes nothing else.

### Quirks

| Quirk | vip | modern | Means |
|-------|-----|--------|-------|
| vF reset | on | off | `8xy1`/`8xy2`/`8xy3` clear `VF` |
| Memory | on | off | `Fx55`/`Fx65` advance `I` by `x + 1` |
| Display wait | on | off | `Dxyn` blocks until vertical blank |
| Clipping | on | on | sprite overhang is cut off |
| Shifting through Vy | on | off | `8xy6`/`8xyE` shift `Vy`, result in `Vx` |
| Jumping on V0 | on | off | `Bnnn` adds `V0`, not `Vx` |

### Known Limitations

- Terminals report key presses but never key releases, so a key stays down for
  3 frames after it is last seen.
- No window and no audio device; the buzzer is a terminal `BEL`.
- SCHIP/XO-CHIP additions are out of scope.
