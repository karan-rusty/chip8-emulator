#include "rom.h"

#include <stdio.h>
#include <sys/stat.h>

int rom_load(Chip8 *c, const char *path)
{
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

void rom_dump(const Chip8 *c)
{
    for (int y = 0; y < FB_H; y++) {
        for (int x = 0; x < FB_W; x++)
            putchar(c->fb[y * FB_W + x] ? '#' : ' ');
        putchar('\n');
    }
}
