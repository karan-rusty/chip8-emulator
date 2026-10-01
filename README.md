# chip8

A CHIP-8 interpreter in C. Original CHIP-8 instruction set only.

## Build

    make

## Test

    make test

## Run

The built-in test ROM dumps a glyph to stdout:

    ./build/chip8

Or load a ROM:

    make rom ROM=path/to/game.ch8

## Status

Implemented opcodes: `1nnn`, `2nnn`, `00EE`, `6xnn`, `7xnn`, `DxNN`, `F0nn`.

Unimplemented opcodes are inert: they advance `pc` by 2 and change nothing
else. See `src/chip8.c` for the notes on two encoding traps that are easy to
trip over:

- `DxNN` height is the low **nibble** (`& 0x0F`), not the 12-bit `nnn`.
- The `0xF` group is ambiguous (`F0nn` shares an encoding with `Fxkk`), so a
  reserved low byte is decoded as the specific opcode.

Not yet supported: the standard font set, timers, input, a window, and audio.
SCHIP/XO-CHIP additions (`Dx0`, `DxCN`, `00FB`-`00FE`, 16-bit registers) are
deliberately out of scope.
