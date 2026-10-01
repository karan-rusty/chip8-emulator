#define _POSIX_C_SOURCE 200809L

#include "terminal.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static const char keypad[NUM_KEYS] = {
    'x',   '1',   '2',   '3',
    'q',   'w',   'e',   'a',
    's',   'd',   'z',   'c',
    '4',   'r',   'f',   'v',
};

#define HOLD_FRAMES 3

#define ROW_BYTES (FB_W * 2 * 6 + 32)
static struct termios saved;
static sig_atomic_t opened;
static int hold[NUM_KEYS];

static char *append(char *p, const char *s)
{
    size_t n = strlen(s);
    memcpy(p, s, n);
    return p + n;
}

static void on_signal(int sig)
{
    (void)sig;
    if (opened) {
        opened = 0;
        (void)!tcsetattr(STDIN_FILENO, TCSANOW, &saved);

        (void)!write(STDOUT_FILENO, "\x1b[0m\x1b[?25h", 9);
    }
    _exit(1);
}

bool term_open(void)
{
    if (tcgetattr(STDIN_FILENO, &saved) != 0)
        return false;

    struct termios raw = saved;

    raw.c_lflag &= (tcflag_t)~(ICANON | ECHO | ISIG);
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0)
        return false;

    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    (void)sigaction(SIGINT,  &sa, NULL);
    (void)sigaction(SIGTERM, &sa, NULL);
    (void)sigaction(SIGHUP,  &sa, NULL);

    opened = 1;
    fputs("\x1b[?25l\x1b[2J\x1b[H", stdout);
    fflush(stdout);
    return true;
}

void term_close(void)
{
    if (!opened)
        return;
    opened = 0;
    (void)tcsetattr(STDIN_FILENO, TCSANOW, &saved);
    fputs("\x1b[0m\x1b[?25h", stdout);
    fflush(stdout);
}

void term_poll(Chip8 *c, bool *quit)
{
    for (unsigned k = 0; k < NUM_KEYS; k++)
        if (hold[k] > 0 && --hold[k] == 0)
            chip8_set_key(c, k, false);

    unsigned char buf[64];
    for (;;) {
        ssize_t n = read(STDIN_FILENO, buf, sizeof buf);
        if (n <= 0)
            break;

        for (ssize_t i = 0; i < n; i++) {
            unsigned char ch = buf[i];

            if (ch == 0x03 || ch == 0x04) {
                *quit = true;
                continue;
            }

            if (ch >= 'A' && ch <= 'F')
                ch = (unsigned char)(ch - 'A' + 'a');

            for (unsigned k = 0; k < NUM_KEYS; k++) {
                if (keypad[k] == (char)ch) {
                    chip8_set_key(c, k, true);
                    hold[k] = HOLD_FRAMES;
                }
            }
        }
    }
}

void term_draw(const Chip8 *c)
{
    char row[ROW_BYTES];
    char *p;

    fputs("\x1b[H", stdout);

    for (int y = 0; y < FB_H; y += 2) {
        p = row;
        p = append(p, "\x1b[92m");
        for (int x = 0; x < FB_W; x++) {
            bool top = c->fb[y * FB_W + x];
            bool bot = c->fb[(y + 1) * FB_W + x];
            if (!top && !bot)      p = append(p, "  ");
            else if (top && bot)   p = append(p, "\u2588\u2588");
            else if (top)          p = append(p, "\u2580\u2580");
            else                   p = append(p, "\u2584\u2584");
        }

        p = append(p, "\x1b[0m\x1b[K\r\n");
        *p = '\0';
        fputs(row, stdout);
    }

    char status[96];
    snprintf(status, sizeof status,
             " dt %3u  st %3u   pad 1234/qwer/asdf/zxcv   ^C quit\x1b[K",
             (unsigned)c->dt, (unsigned)c->st);
    fputs(status, stdout);
    fflush(stdout);
}
