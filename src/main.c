#define _POSIX_C_SOURCE 200809L

#include "chip8.h"
#include "rom.h"
#include "terminal.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define TIMER_HZ         60
#define CYCLES_PER_FRAME 10

static const uint8_t test_rom[] = {
    0x60, 0x10, 0x61, 0x08, 0xA0, 0x00, 0xD0, 0x15, 0x00, 0x00,
};

static void add_frame(struct timespec *t)
{
    t->tv_nsec += 1000000000L / TIMER_HZ;
    if (t->tv_nsec >= 1000000000L) {
        t->tv_nsec -= 1000000000L;
        t->tv_sec++;
    }
}

static void sleep_until(struct timespec *deadline)
{
    struct timespec now, req;
    clock_gettime(CLOCK_MONOTONIC, &now);

    if (now.tv_sec > deadline->tv_sec ||
        (now.tv_sec == deadline->tv_sec && now.tv_nsec >= deadline->tv_nsec)) {
        *deadline = now;
        return;
    }

    req.tv_sec  = deadline->tv_sec  - now.tv_sec;
    req.tv_nsec = deadline->tv_nsec - now.tv_nsec;
    if (req.tv_nsec < 0) {
        req.tv_nsec += 1000000000L;
        req.tv_sec--;
    }
    while (nanosleep(&req, &req) == -1 && errno == EINTR)
        ;
}

static void run_interactive(Chip8 *c)
{
    if (!term_open()) {
        fprintf(stderr, "chip8: stdin is not a terminal, cannot take input\n"
                        "     run `./build/chip8` with no arguments for the\n"
                        "     built-in ROM and a text dump instead\n");
        return;
    }
    atexit(term_close);

    bool quit = false;
    struct timespec deadline;
    clock_gettime(CLOCK_MONOTONIC, &deadline);

    while (!quit) {
        term_poll(c, &quit);
        chip8_run(c, CYCLES_PER_FRAME);
        chip8_tick_timers(c);
        term_draw(c);
        if (chip8_sound(c)) {
            fputc('\a', stdout);
            fflush(stdout);
        }
        add_frame(&deadline);
        sleep_until(&deadline);
    }
}

int main(int argc, char **argv)
{
    Chip8 c;
    chip8_init(&c);

    if (argc > 1) {
        if (rom_load(&c, argv[1]) != 0)
            return 1;
        run_interactive(&c);
        return 0;
    }

    bool truncated = false;
    chip8_load_rom(&c, test_rom, sizeof test_rom, &truncated);
    chip8_run(&c, 10);
    rom_dump(&c);
    return 0;
}
