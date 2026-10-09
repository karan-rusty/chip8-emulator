A CHIP-8 emulator written in C that runs in your terminal, with three complete
games bundled: **Ping Pong**, **Snake**, and **Conway's Game of Life**.

This is the first release and it needs testers. Download, run, report — the basic
pass takes about five minutes and requires no build tools.

https://github.com/user-attachments/assets/e9d61d8d-7ffa-4343-8975-765bb523b892

## Run it (Linux x86_64, nothing to build)

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

## Test it

Please go through as many of these as you can:

- [ ] `./chip8 --menu` opens the menu; arrows navigate; Enter starts a game.
- [ ] **Ping Pong** — the paddle moves with W/S and arrows; the ball bounces off
      walls and paddles; a miss is scored and play continues.
- [ ] **Snake** — moves, eats, grows; steering works; hitting a wall or itself
      shows a game-over panel; any key restarts.
- [ ] **Game of Life** — cells evolve; `W` pauses, `D` steps one generation,
      `A` reseeds, `S` changes the pace.
- [ ] **Ctrl-C** quits cleanly (cursor returns, terminal is not garbled).
- [ ] Sound (terminal bell) plays on the Snake game-over.
- [ ] Resizing the terminal while playing does not crash it.

The tarball also contains `TESTING.md`, the full step-by-step tester guide.

## Report what you find

[Open an issue](https://github.com/karan-rusty/chip8-emulator/issues/new) with
your OS and terminal, the exact command you ran, and what you expected versus
what happened. "Everything worked" is useful too — just note your OS and terminal.

## Build from source

```sh
make            # build/chip8
make test       # unit tests: 87 checks, 0 failures
make play       # build everything and open the game menu
make play-ping  # also: play-snake, play-life
make release    # reproduce this tarball (needs a static libc)
```

## Known limits

- The prebuilt binary is **Linux x86_64 only**; other platforms must build from source.
- The full 128-column framebuffer needs a wide terminal; narrower windows clip it.
- Sound is the **terminal bell**, so it follows your terminal's bell setting and can be muted.
- The standalone `chip8` asset is the same binary without the `roms/` folder —
  `--menu` needs the ROMs next to it, so use the tarball.
