#define _POSIX_C_SOURCE 200809L

#include "frontend.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

static const char keypad[NUM_KEYS] = {
    'x',   '1',   '2',   '3',
    'q',   'w',   'e',   'a',
    's',   'd',   'z',   'c',
    '4',   'r',   'f',   'v',
};

#define HOLD_FRAMES 3
#define ROW_BYTES   (FB_W * 2 * 6 + 32)

/* The terminal backend's whole mutable state, private to this file. Nothing
 * here is reachable from the core or from main. */
typedef struct {
    struct termios  saved;
    sig_atomic_t    opened;
    int             hold[NUM_KEYS];
    struct timespec deadline;
    int             esc;   /* 0 normal, 1 seen ESC, 2 in CSI, 3 in SS3 */
} Terminal;

static Terminal term;

static char *append(char *p, const char *s)
{
    size_t n = strlen(s);
    memcpy(p, s, n);
    return p + n;
}

static void on_signal(int sig)
{
    (void)sig;
    if (term.opened) {
        term.opened = 0;
        (void)!tcsetattr(STDIN_FILENO, TCSANOW, &term.saved);

        (void)!write(STDOUT_FILENO, "\x1b[0m\x1b[?25h", 9);
    }
    _exit(1);
}

static void add_frame(struct timespec *t)
{
    t->tv_nsec += 1000000000L / FRAME_HZ;
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

static bool term_open(void *state)
{
    Terminal *t = state;

    if (tcgetattr(STDIN_FILENO, &t->saved) != 0) {
        fprintf(stderr, "chip8: stdin is not a terminal, cannot take input\n"
                        "     run `./build/chip8` with no arguments for the\n"
                        "     built-in ROM and a text dump instead\n");
        return false;
    }

    struct termios raw = t->saved;

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

    t->opened = 1;
    clock_gettime(CLOCK_MONOTONIC, &t->deadline);

    fputs("\x1b[?25l\x1b[2J\x1b[H", stdout);
    fflush(stdout);
    return true;
}

static void term_close(void *state)
{
    Terminal *t = state;

    if (!t->opened)
        return;
    t->opened = 0;
    (void)tcsetattr(STDIN_FILENO, TCSANOW, &t->saved);
    fputs("\x1b[0m\x1b[?25h", stdout);
    fflush(stdout);
}

static void term_poll(void *state, Chip8 *c, bool *quit)
{
    Terminal *t = state;

    for (unsigned k = 0; k < NUM_KEYS; k++)
        if (t->hold[k] > 0 && --t->hold[k] == 0)
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
                t->esc = 0;
                continue;
            }

            /* Swallow ESC sequences (arrows, F-keys) so they never become
             * phantom keypad presses. State survives across reads. */
            if (t->esc == 2) {              /* inside CSI: ESC [ ... */
                if (ch >= 0x40 && ch <= 0x7E) {
                    unsigned char arrow = 0;
                    switch (ch) {
                    case 'A': arrow = keypad[5]; break;   /* up    -> w */
                    case 'B': arrow = keypad[8]; break;   /* down  -> s */
                    case 'C': arrow = keypad[9]; break;   /* right -> d */
                    case 'D': arrow = keypad[7]; break;   /* left  -> a */
                    }
                    if (arrow) {
                        for (unsigned k = 0; k < NUM_KEYS; k++) {
                            if (keypad[k] == arrow) {
                                chip8_set_key(c, k, true);
                                t->hold[k] = HOLD_FRAMES;
                            }
                        }
                    }
                    t->esc = 0;
                }
                continue;
            }
            if (t->esc == 3) {              /* SS3: ESC O x */
                if (ch == 'A') { chip8_set_key(c, 5, true); t->hold[5] = HOLD_FRAMES; }
                if (ch == 'B') { chip8_set_key(c, 8, true); t->hold[8] = HOLD_FRAMES; }
                if (ch == 'C') { chip8_set_key(c, 9, true); t->hold[9] = HOLD_FRAMES; }
                if (ch == 'D') { chip8_set_key(c, 7, true); t->hold[7] = HOLD_FRAMES; }
                t->esc = 0;
                continue;
            }
            if (t->esc == 1) {
                if (ch == '[') {
                    t->esc = 2;
                    continue;
                }
                if (ch == 'O') {
                    t->esc = 3;
                    continue;
                }
                /* Lone ESC followed by a normal key: the ESC itself is
                 * dropped, the key is handled below. */
                t->esc = 0;
                if (ch == 0x1B) {
                    t->esc = 1;
                    continue;
                }
            } else if (ch == 0x1B) {
                t->esc = 1;
                continue;
            }

            if (ch >= 'A' && ch <= 'Z')
                ch = (unsigned char)(ch - 'A' + 'a');

            for (unsigned k = 0; k < NUM_KEYS; k++) {
                if (keypad[k] == (char)ch) {
                    chip8_set_key(c, k, true);
                    t->hold[k] = HOLD_FRAMES;
                }
            }
        }
    }
}

static void term_present(void *state, const Chip8 *c)
{
    (void)state;

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

static void term_sound(void *state, const Chip8 *c)
{
    (void)state;

    if (chip8_sound(c)) {
        fputc('\a', stdout);
        fflush(stdout);
    }
}

static void term_wait_frame(void *state)
{
    Terminal *t = state;

    add_frame(&t->deadline);
    sleep_until(&t->deadline);
}

static const Frontend terminal_frontend = {
    .name       = "terminal",
    .state      = &term,
    .open       = term_open,
    .close      = term_close,
    .poll       = term_poll,
    .present    = term_present,
    .sound      = term_sound,
    .wait_frame = term_wait_frame,
};

const Frontend *frontend_terminal(void)
{
    return &terminal_frontend;
}
