#include "state.h"

#include <stdio.h>

/* Bigger than any snapshot chip8_state_size() can report; a stack buffer
 * keeps the no-malloc rule the rest of the host follows. */
#define STATE_BUF 8192

int state_save_file(const Chip8 *c, const char *path)
{
    size_t n = chip8_state_size();
    uint8_t buf[STATE_BUF];

    if (n > sizeof buf) {
        fprintf(stderr, "chip8: state is too large to save\n");
        return -1;
    }
    if (!chip8_state_save(c, buf, sizeof buf)) {
        fprintf(stderr, "chip8: cannot serialize state\n");
        return -1;
    }

    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "chip8: cannot open %s for writing\n", path);
        return -1;
    }

    size_t wrote = fwrite(buf, 1, n, f);
    int bad = ferror(f) != 0 || wrote != n;
    fclose(f);

    if (bad) {
        fprintf(stderr, "chip8: error writing %s\n", path);
        return -1;
    }
    return 0;
}

int state_load_file(Chip8 *c, const char *path)
{
    size_t n = chip8_state_size();
    uint8_t buf[STATE_BUF];

    if (n > sizeof buf) {
        fprintf(stderr, "chip8: state is too large to load\n");
        return -1;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "chip8: cannot open %s\n", path);
        return -1;
    }

    size_t got = fread(buf, 1, n, f);
    int bad = ferror(f) != 0;
    fclose(f);

    if (bad) {
        fprintf(stderr, "chip8: error reading %s\n", path);
        return -1;
    }
    if (got != n || !chip8_state_load(c, buf, n)) {
        fprintf(stderr, "chip8: %s is not a chip8 state file\n", path);
        return -1;
    }
    return 0;
}
