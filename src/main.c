#include "chip8.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

/* A hand-assembled ROM used when no file is given.
 *
 *   0x200: 60 10   V0 = 0x10   (x = 16)
 *   0x202: 61 08   V1 = 0x08   (y = 8)
 *   0x204: F0 20   I  = 0x020  (the glyph in font memory)
 *                    (must be x=0; FB 20 is FxB0, the flag pointer)
 *   0x206: D0 14   draw 4 rows at V0,V1 from mem[I]
 *                    (four hex digits: D x y N, so x=0 -> V0, y=1 -> V1)
 *   0x208: 00 00   noop
 *
 * Expect a hollow 4x4 square with the top-left pixel missing, at x=16..19,
 * y=8..11.
 */
static const uint8_t test_rom[] = {
    0x60, 0x10, 0x61, 0x08, 0xF0, 0x20, 0xD0, 0x14, 0x00, 0x00,
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

int main(int argc, char **argv)
{
    Chip8 c;
    chip8_init(&c);

    if (argc > 1) {
        if (load_file(&c, argv[1]) != 0)
            return 1;
    } else {
        bool truncated = false;
        chip8_load_rom(&c, test_rom, sizeof(test_rom), &truncated);
    }

    /* 10 cycles is enough for the test ROM (5 opcodes) plus margin. Once a
     * halt opcode exists, run until halted instead of counting. */
    chip8_run(&c, 10);

    print_screen(&c);
    return 0;
}
