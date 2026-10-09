A CHIP-8 emulator written in C that runs in your terminal, with three bundled
games: **Ping Pong**, **Snake**, and **Conway's Game of Life**.

First release. Linux x86_64, nothing to build.

## Run it

```sh
tar xzf chip8-v1.0.0-linux-x86_64.tar.gz
chmod +x chip8
./chip8 --menu
```

Run it from the folder you extracted into, in a full-screen terminal — the 64x32
framebuffer is drawn 128 columns wide, so a narrower window clips it. Arrow keys
(or `1`/`2`/`3`) pick a game, Enter starts it, **Ctrl-C** quits.

## Games

```sh
./chip8 roms/ping.ch8  -s 120   -q modern
./chip8 roms/snake.ch8 -s 120   -q modern
./chip8 roms/life.ch8  -s 16000 -q modern
```

| Game         | Controls                                    |
| ------------ | ------------------------------------------- |
| Ping Pong    | `W`/`S` or `↑`/`↓` — move your paddle       |
| Snake        | `W` `A` `S` `D` or arrows — steer           |
| Game of Life | `W` pause · `D` step · `A` reseed · `S` pace |

CHIP-8 keypad → keyboard: `123C` → `1234`, `456D` → `qwer`, `789E` → `asdf`,
`A0BF` → `zxcv`.

## Build from source

```sh
make        # build/chip8
make test   # unit tests: 87 checks, 0 failures
make play   # build everything and open the game menu
```
