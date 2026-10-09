A CHIP-8 emulator written in C that runs in your terminal, with three complete
games bundled: **Ping Pong**, **Snake**, and **Conway's Game of Life**.

First release. Linux x86_64, nothing to build.

## Run it

```sh
tar xzf chip8-v1.0.0-linux-x86_64.tar.gz   # -> chip8, roms/, README.md, TESTING.md
chmod +x chip8
./chip8 --menu
```

- Run it **from the folder you extracted into**: the menu loads `roms/*.ch8`
  relative to the current directory.
- Use a **full-screen terminal**. The 64x32 framebuffer is drawn 128 columns
  wide, so a narrower window clips it — an 80-column terminal shows the left half.
- Arrow keys (or `1`/`2`/`3`) pick a game, Enter starts it. **Ctrl-C** quits.
- The binary is **statically linked for 64-bit Linux** — no runtime dependencies,
  works on glibc and musl distros alike.

Check the download (optional):

```sh
sha256sum -c chip8-v1.0.0-linux-x86_64.tar.gz.sha256
```

## Run a game directly

`./chip8 --menu` runs all three games, each at its own speed, using the default
`vip` quirks profile. Jump straight into one instead:

| Game             | Command                                       | Controls                                              |
| ---------------- | --------------------------------------------- | ----------------------------------------------------- |
| **Ping Pong**    | `./chip8 roms/ping.ch8 -s 120 -q modern`      | `W`/`S` or `↑`/`↓` — paddle; right paddle is a simple AI |
| **Snake**        | `./chip8 roms/snake.ch8 -s 120 -q modern`     | `W` `A` `S` `D` or arrows — steer; speed ramps with score |
| **Game of Life** | `./chip8 roms/life.ch8 -s 16000 -q modern`    | `W` pause · `D` step · `A` reseed · `S` pace          |

Game of Life redraws cleanly under `-q modern`; the menu's default `vip` profile
will animate it slowly, so use the command above for that one.

The CHIP-8 keypad maps onto the keyboard like this:

```
1 2 3 C    ->  1 2 3 4
4 5 6 D    ->  q w e r
7 8 9 E    ->  a s d f
A 0 B F    ->  z x c v
```

## What's in it

- Complete CHIP-8 interpreter (all 16 opcode groups) with `vip` (original) and
  `modern` quirk profiles — `-q vip|modern`.
- Terminal frontend at 60 Hz, framebuffer drawn with Unicode half-blocks, sound
  through the terminal bell.
- ROM disassembler (`./chip8 -d roms/snake.ch8`), machine-state save and load
  (`-S` / `-L`), speed control in core cycles per frame (`-s`).
- `./chip8 --help` lists every option; `./chip8` with no arguments runs a
  built-in test ROM and dumps the framebuffer.

## Build from source

```sh
make            # build/chip8
make test       # unit tests: 87 checks, 0 failures
make play       # build everything and open the game menu
make play-ping  # also: play-snake, play-life
make release    # reproduce this tarball (needs a static libc)
```
