# chip8

A CHIP-8 emulator in C with three bundled games: Ping Pong, Snake, and Conway's Game of Life.

## Demo

https://github.com/user-attachments/assets/e9d61d8d-7ffa-4343-8975-765bb523b892


## Get the release

Download a prebuilt, statically linked Linux (x86_64) binary from the
[Releases page](https://github.com/karan-rusty/chip8-emulator/releases/latest),
or build it yourself with `make release`. New to the project? Follow
[TESTING.md](TESTING.md) for a step-by-step guide to running and testing it.

## Build

    make
    make test

## Play

    make play          # menu
    make play-ping     # ping pong
    make play-snake    # snake
    make play-life     # game of life

## Controls

    1 2 3 C    ->  1 2 3 4
    4 5 6 D    ->  q w e r
    7 8 9 E    ->  a s d f
    A 0 B F    ->  z x c v

Ping Pong: W/S or arrows
Snake: WASD
Life: W pause, D step, A reseed, S pace

Ctrl-C to quit.

**Warning:** Play in full screen. The 64x32 framebuffer is drawn as 128x16 characters — a small terminal will clip it.
