#define _POSIX_C_SOURCE 200809L

#include "terminal.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

/* The CHIP-8 keypad is a hex pad, so it has to be mapped onto a QWERTY
 * keyboard. This is the arrangement nearly every emulator uses:
 *
 *     CHIP-8        1 2 3 C      host keys    1 2 3 4
 *                   4 5 6 D                   q w e r
 *                   7 8 9 E                   a s d f
 *                   A 0 B F                   z x c v
 *
 * Indexed by CHIP-8 key, so keypad[0xC] is the host key for hex C. */
static const char keypad[NUM_KEYS] = {
    'x', /* 0 */  '1', /* 1 */  '2', /* 2 */  '3', /* 3 */
    'q', /* 4 */  'w', /* 5 */  'e', /* 6 */  'a', /* 7 */
    's', /* 8 */  'd', /* 9 */  'z', /* A */  'c', /* B */
    '4', /* C */  'r', /* D */  'f', /* E */  'v', /* F */
};

/* A terminal reports key presses but never key releases, so there is no way
 * to observe "the player let go". Each key therefore stays down for this many
 * frames after it is last seen: long enough that a human tap registers with
 * Fx0A and with Ex9E polls, short enough that nothing sticks. This is the one
 * thing a real keypad does that a tty cannot, and it is why hold exists. */
#define HOLD_FRAMES 3

/* Two CHIP-8 pixels wide per terminal cell (cells are roughly twice as tall
 * as they are wide), and two CHIP-8 rows stacked per terminal row using the
 * half-block glyphs. The 64x32 framebuffer becomes 128x16 characters, which
 * fits a normal terminal. Worst case a cell is two 3-byte UTF-8 glyphs. */
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
        /* write(), not fputs(): this runs from a signal handler. */
        (void)!write(STDOUT_FILENO, "\x1b[0m\x1b[?25h", 9);
    }
    _exit(1);
}

bool term_open(void)
{
    if (tcgetattr(STDIN_FILENO, &saved) != 0)
        return false;

    struct termios raw = saved;
    /* ISIG is cleared so that ^C arrives as a byte we handle ourselves; if
     * the tty could raise SIGINT we would be killed before restoring it. */
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
    /* Let go of anything held from an earlier frame. */
    for (unsigned k = 0; k < NUM_KEYS; k++)
        if (hold[k] > 0 && --hold[k] == 0)
            chip8_set_key(c, k, false);

    unsigned char buf[64];
    for (;;) {
        ssize_t n = read(STDIN_FILENO, buf, sizeof buf);
        if (n <= 0)
            break;   /* nothing pending (0), EOF (-1), or an error */

        for (ssize_t i = 0; i < n; i++) {
            unsigned char ch = buf[i];

            if (ch == 0x03 || ch == 0x04) {   /* ^C or ^D */
                *quit = true;
                continue;
            }

            /* Accept uppercase A-F so Caps Lock does not silently dead-key
             * the right-hand column of the pad. */
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

    /* Home rather than clear: repainting over the previous frame avoids the
     * flicker a per-frame clear would cause. */
    fputs("\x1b[H", stdout);

    for (int y = 0; y < FB_H; y += 2) {
        p = row;
        p = append(p, "\x1b[92m");   /* phosphor green */
        for (int x = 0; x < FB_W; x++) {
            bool top = c->fb[y * FB_W + x];
            bool bot = c->fb[(y + 1) * FB_W + x];
            if (!top && !bot)      p = append(p, "  ");
            else if (top && bot)   p = append(p, "\u2588\u2588");
            else if (top)          p = append(p, "\u2580\u2580");
            else                   p = append(p, "\u2584\u2584");
        }
        /* Reset before erasing so the clear happens with default attributes,
         * then wipe whatever the previous longer line left behind. */
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
