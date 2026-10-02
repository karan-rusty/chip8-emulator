# chip8

A CHIP-8 emulator in C with three bundled games: Ping Pong, Snake, and Conway's Game of Life.

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
