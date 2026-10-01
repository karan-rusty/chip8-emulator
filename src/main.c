#define _POSIX_C_SOURCE 200809L

#include "chip8.h"
#include "terminal.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>

/* Timers run at 60Hz on real hardware, and that is the clock the whole
 * frontend is paced by. The CPU is then given a slice of work per tick. */
#define TIMER_HZ         60
#define CYCLES_PER_FRAME 10   /* 10 * 60 = 600 instructions per second */

/* A hand-assembled ROM used when no file is given.
 *
 *   0x200: 60 10   V0 = 0x10   (x = 16)
 *   0x202: 61 08   V1 = 0x08   (y = 8)
 *   0x204: A0 00   I  = 0x000   the glyph '0', which lives at FONT_BASE
 *   0x206: D0 15   draw 5 rows at V0,V1 from mem[I]
 *                    four hex digits: D x y N, so x=0 -> V0, y=1 -> V1, N=5
 *   0x208: 00 00   inert; advances pc
 *
 * Expect the glyph '0': a 4-wide, 5-tall ring at x=16..19, y=8..12, with 14
 * pixels lit. This ROM used to load I with F020, which is not an instruction
 * on any CHIP-8 - it read four bytes out of the middle of glyph '6'. */
static const uint8_t test_rom[] = {
    0x60, 0x10, 0x61, 0x08, 0xA0, 0x00, 0xD0, 0x15, 0x00, 0x00,
};

static int load_file(Chip8 *c, const char *path)
{
    /* fopen succeeds on a directory, and fread then yields 0 bytes, which
     * would otherwise look like a valid but empty ROM. */
    struct stat st;
    if (stat(path, &st) != 0) {
        fprintf(stderr, "chip8: cannot stat %s\n", path);
        return -1;
    }
    if (!S_ISREG(st.st_mode)) {
        fprintf(stderr, "chip8: %s is not a regular file\n", path);
        return -1;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "chip8: cannot open %s\n", path);
        return -1;
    }

    /* Read one byte more than we can store, so an oversized ROM is
     * detectable here. Comparing fread's result alone cannot catch it,
     * because the read is capped at the buffer size. */
    uint8_t buf[PROG_MAX + 1];
    size_t n = fread(buf, 1, sizeof(buf), f);
    bool read_err = ferror(f) != 0;
    fclose(f);

    if (read_err) {
        fprintf(stderr, "chip8: error reading %s\n", path);
        return -1;
    }

    if (n == 0) {
        fprintf(stderr, "chip8: %s is empty\n", path);
        return -1;
    }

    bool truncated = false;
    if (!chip8_load_rom(c, buf, n, &truncated)) {
        fprintf(stderr, "chip8: %s is empty\n", path);
        return -1;
    }
    if (truncated)
        fprintf(stderr, "chip8: warning: %s exceeds %d bytes, truncated\n",
                path, PROG_MAX);
    return 0;
}

static void print_screen(const Chip8 *c)
{
    for (int y = 0; y < FB_H; y++) {
        for (int x = 0; x < FB_W; x++)
            putchar(c->fb[y * FB_W + x] ? '#' : ' ');
        putchar('\n');
    }
}

/* Advance a deadline by one frame. */
static void add_frame(struct timespec *t)
{
    t->tv_nsec += 1000000000L / TIMER_HZ;
    if (t->tv_nsec >= 1000000000L) {
        t->tv_nsec -= 1000000000L;
        t->tv_sec++;
    }
}

/* Sleep until `deadline`, or return at once if it has already passed.
 * Sleeping to an absolute deadline rather than for a frame's worth keeps the
 * timing free of the work time, which would otherwise accumulate. */
static void sleep_until(struct timespec *deadline)
{
    struct timespec now, req;
    clock_gettime(CLOCK_MONOTONIC, &now);

    if (now.tv_sec > deadline->tv_sec ||
        (now.tv_sec == deadline->tv_sec && now.tv_nsec >= deadline->tv_nsec)) {
        /* Behind schedule. Drop the backlog instead of sprinting to catch
         * up, so one slow frame cannot turn into a fast-forward. */
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

/* Interactive run loop: input, execute, tick the 60Hz timers, repaint. */
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
            fputc('\a', stdout);   /* the one tone CHIP-8 has */
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
        if (load_file(&c, argv[1]) != 0)
            return 1;
        run_interactive(&c);
        return 0;
    }

    bool truncated = false;
    chip8_load_rom(&c, test_rom, sizeof test_rom, &truncated);

    /* Headless path: the built-in ROM is five opcodes, so a fixed slice is
     * enough and there is no loop to escape from. */
    chip8_run(&c, 10);
    print_screen(&c);
    return 0;
}
