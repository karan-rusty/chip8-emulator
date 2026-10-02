#include "chip8.h"
#include "disasm.h"
#include "frontend.h"
#include "rom.h"
#include "state.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static const uint8_t test_rom[] = {
    0x60, 0x10, 0x61, 0x08, 0xA0, 0x00, 0xD0, 0x15, 0x00, 0x00,
};

typedef struct {
    const char *name;
    const char *rom;
    int         speed;
    const char *controls;
} Game;

static const Game games[] = {
    { "Ping Pong", "roms/ping.ch8", 120,  "W/S or Up/Down: move paddle" },
    { "Snake",     "roms/snake.ch8", 120,  "WASD: steer" },
    { "Game of Life", "roms/life.ch8", 16000, "W:pause D:step A:reseed S:pace" },
};

#define NUM_GAMES (sizeof games / sizeof games[0])

static const Frontend *pick_frontend(void)
{
    const char *name = getenv("CHIP8_FRONTEND");
    const Frontend *fe = name ? frontend_by_name(name) : NULL;
    return fe ? fe : frontend_terminal();
}

static void usage(FILE *out)
{
    fputs("usage: chip8 [options] [rom]\n"
          "\n"
          "  -r, --rom FILE        load FILE as the ROM\n"
          "  -L, --load-state FILE start from a saved machine state\n"
          "  -S, --save-state FILE write machine state to FILE when the run ends\n"
          "  -q, --quirks NAME     vip (default) or modern\n"
          "  -s, --speed N         core cycles per frame (default 10)\n"
          "  -d, --dump            disassemble the ROM and exit\n"
          "  -m, --menu            show the game selection menu\n"
          "  -h, --help            show this help\n"
          "\n"
          "With no ROM or state, runs the built-in test ROM and dumps the\n"
          "framebuffer as text.\n"
          "\n"
          "Environment: CHIP8_FRONTEND, CHIP8_QUIRKS\n", out);
}

/* Show the game selection menu and return the chosen game, or NULL to quit.
 * Uses raw terminal mode so a single keypress is enough — no Enter needed. */
static const Game *pick_game(void)
{
    struct termios saved, raw;
    if (tcgetattr(STDIN_FILENO, &saved) != 0)
        return NULL;
    raw = saved;
    raw.c_lflag &= (tcflag_t)~(ICANON | ECHO);
    raw.c_cc[VMIN]  = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0)
        return NULL;

    int sel = 0;
    bool done = false;
    const Game *result = NULL;

    while (!done) {
        fputs("\x1b[2J\x1b[H", stdout);
        fputs("\x1b[1;36m  CHIP-8 Arcade\x1b[0m\n\n", stdout);

        for (size_t i = 0; i < NUM_GAMES; i++) {
            if ((int)i == sel)
                fprintf(stdout, "  \x1b[1;32m> %zu. %-14s\x1b[0m  %s\n",
                        i + 1, games[i].name, games[i].controls);
            else
                fprintf(stdout, "    %zu. %-14s  %s\n",
                        i + 1, games[i].name, games[i].controls);
        }

        fputs("\n  \x1b[2mUp/Down: navigate  Enter: select  Q: quit\x1b[0m\n", stdout);
        fflush(stdout);

        char buf[8];
        ssize_t n = read(STDIN_FILENO, buf, sizeof buf);
        if (n <= 0)
            break;

        for (ssize_t i = 0; i < n; i++) {
            unsigned char ch = (unsigned char)buf[i];
            if (ch == '\r' || ch == '\n') {
                result = &games[sel];
                done = true;
                break;
            } else if (ch == 'q' || ch == 'Q' || ch == 0x03 || ch == 0x04) {
                done = true;
                break;
            } else if (ch == 0x1B && i + 2 < n && buf[i+1] == '[') {
                if (buf[i+2] == 'A')      sel = (sel - 1 + NUM_GAMES) % NUM_GAMES;
                else if (buf[i+2] == 'B') sel = (sel + 1) % NUM_GAMES;
                i += 2;
            } else if (ch >= '1' && ch <= '0' + NUM_GAMES) {
                result = &games[ch - '1'];
                done = true;
                break;
            }
        }
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &saved);
    fputs("\x1b[2J\x1b[H", stdout);
    fflush(stdout);
    return result;
}

static void run(Chip8 *c, const Frontend *fe, int speed)
{
    if (!fe->open(fe->state))
        return;

    bool quit = false;
    while (!quit) {
        fe->poll(fe->state, c, &quit);
        chip8_run(c, speed);
        chip8_tick_timers(c);
        fe->present(fe->state, c);
        fe->sound(fe->state, c);
        fe->wait_frame(fe->state);
    }

    fe->close(fe->state);
}

static bool needs_value(int i, int argc, const char *opt)
{
    if (i >= argc) {
        fprintf(stderr, "chip8: %s needs a value\n", opt);
        return false;
    }
    return true;
}

int main(int argc, char **argv)
{
    Chip8 c;
    chip8_init(&c);

    const char *rom = NULL;
    const char *load_state = NULL;
    const char *save_state = NULL;
    int speed = CYCLES_PER_FRAME;
    bool dump = false;
    bool menu = false;

    const char *qe = getenv("CHIP8_QUIRKS");
    if (qe) {
        QuirkPreset p;
        if (chip8_quirk_parse(qe, &p))
            chip8_set_quirks(&c, p);
    }

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];

        if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
            usage(stdout);
            return 0;
        } else if (strcmp(a, "-d") == 0 || strcmp(a, "--dump") == 0) {
            dump = true;
        } else if (strcmp(a, "-m") == 0 || strcmp(a, "--menu") == 0) {
            menu = true;
        } else if (strcmp(a, "-r") == 0 || strcmp(a, "--rom") == 0) {
            if (!needs_value(++i, argc, a))
                return 2;
            rom = argv[i];
        } else if (strcmp(a, "-L") == 0 || strcmp(a, "--load-state") == 0) {
            if (!needs_value(++i, argc, a))
                return 2;
            load_state = argv[i];
        } else if (strcmp(a, "-S") == 0 || strcmp(a, "--save-state") == 0) {
            if (!needs_value(++i, argc, a))
                return 2;
            save_state = argv[i];
        } else if (strcmp(a, "-q") == 0 || strcmp(a, "--quirks") == 0) {
            QuirkPreset p;
            if (!needs_value(++i, argc, a))
                return 2;
            if (!chip8_quirk_parse(argv[i], &p)) {
                fprintf(stderr, "chip8: unknown quirks '%s' (want vip or modern)\n",
                        argv[i]);
                return 2;
            }
            chip8_set_quirks(&c, p);
        } else if (strcmp(a, "-s") == 0 || strcmp(a, "--speed") == 0) {
            char *end = NULL;
            long v;
            if (!needs_value(++i, argc, a))
                return 2;
            v = strtol(argv[i], &end, 10);
            if (!end || *end != '\0' || v < 1 || v > 100000) {
                fprintf(stderr, "chip8: bad speed '%s'\n", argv[i]);
                return 2;
            }
            speed = (int)v;
        } else if (a[0] == '-' && a[1] != '\0') {
            fprintf(stderr, "chip8: unknown option %s\n", a);
            usage(stderr);
            return 2;
        } else if (!rom) {
            rom = a;
        } else {
            fprintf(stderr, "chip8: unexpected argument %s\n", a);
            return 2;
        }
    }

    if (menu) {
        const Game *g = pick_game();
        if (!g)
            return 0;
        rom = g->rom;
        speed = g->speed;
    }

    if (!rom && !load_state) {
        chip8_load_rom(&c, test_rom, sizeof test_rom, NULL);
        if (dump) {
            disasm_rom(&c, PROG_BASE, sizeof test_rom, stdout);
            return 0;
        }
        chip8_run(&c, speed);
        rom_dump(&c);
        return 0;
    }

    if (load_state) {
        if (state_load_file(&c, load_state) != 0)
            return 1;
    } else {
        size_t rom_len = 0;
        if (rom_load(&c, rom, &rom_len) != 0)
            return 1;
        if (dump) {
            disasm_rom(&c, PROG_BASE, rom_len, stdout);
            return 0;
        }
    }

    run(&c, pick_frontend(), speed);

    if (save_state && state_save_file(&c, save_state) != 0)
        return 1;

    return 0;
}
