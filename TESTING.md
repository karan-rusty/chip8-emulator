# Testing chip8

Thanks for helping test! This is a CHIP-8 emulator in C that runs in your
terminal and bundles three games: Ping Pong, Snake, and Conway's Game of Life.

You don't need to build anything. Grab the release, run it, and tell us what
broke. The whole thing takes about five minutes.

## 1. Get the build

Download `chip8-<version>-linux-x86_64.tar.gz` from the
[Releases page](../../releases/latest), then:

```sh
tar xzf chip8-*-linux-x86_64.tar.gz   # -> chip8, roms/, README.md, TESTING.md
chmod +x chip8
```

The tarball has no wrapper folder — run the commands from the directory you
extracted into.

The binary is **statically linked for 64-bit Linux (x86_64)** — no runtime
dependencies. Verify the download with the `.sha256` file if you like:

```sh
sha256sum -c chip8-*-linux-x86_64.tar.gz.sha256
```

> Run `./chip8` from the extracted folder. The menu loads the ROMs from the
> `roms/` directory next to the binary.

### Building from source instead

```sh
make          # build build/chip8
make test     # run the unit tests (expects: 87 checks, 0 failures)
make play     # build everything and open the game menu
```

## 2. Run it

Play in a **full-screen terminal**: the 64x32 framebuffer is drawn 128
characters wide, so a small window will clip it. On a normal 80-column
terminal you will only see the left half.

```sh
./chip8 --menu
```

Use the arrow keys (or `1`/`2`/`3`) to pick a game and press Enter.

Or jump straight into one:

```sh
./chip8 roms/ping.ch8 -s 120   -q modern
./chip8 roms/snake.ch8 -s 120  -q modern
./chip8 roms/life.ch8 -s 16000 -q modern
```

Press **Ctrl-C** to quit at any time.

## 3. Controls

The CHIP-8 keypad maps onto the keyboard like this:

```
1 2 3 C    ->  1 2 3 4
4 5 6 D    ->  q w e r
7 8 9 E    ->  a s d f
A 0 B F    ->  z x c v
```

| Game         | Controls                                            |
| ------------ | --------------------------------------------------- |
| Ping Pong    | `W` / `S` or `↑` / `↓` — move your paddle           |
| Snake        | `W` `A` `S` `D` or arrows — steer                   |
| Game of Life | `W` pause · `D` single step · `A` reseed · `S` pace |

## 4. What to check

Please try at least the first few items and report anything that looks wrong.

- [ ] `./chip8 --menu` opens the menu; arrows navigate; Enter starts a game.
- [ ] **Ping Pong** — your paddle moves with W/S and arrows; the ball bounces
      off walls and paddles; a miss is scored and play continues.
- [ ] **Snake** — the snake moves, eats food, and grows; steering works; running
      into a wall or itself shows a game-over screen; any key restarts.
- [ ] **Game of Life** — cells evolve each generation; `W` pauses/resumes;
      `D` advances one generation; `A` reseeds; `S` changes the pace.
- [ ] **Ctrl-C** quits cleanly (cursor comes back, terminal isn't garbled).
- [ ] Sound plays (a terminal beep) on the Snake game-over.
- [ ] Resizing the terminal while playing doesn't crash it.

## 5. Report what you find

Open an [issue](../../issues/new) with:

1. Your OS and terminal (e.g. "Ubuntu 24.04, GNOME Terminal 3.52").
2. The exact command you ran.
3. What you expected vs. what happened.
4. A screenshot or short screen recording if it's visual.

Even "everything worked" is useful — drop a comment with your OS and terminal.
