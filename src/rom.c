#define _POSIX_C_SOURCE 200809L

#include "rom.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

int rom_load(Chip8 *c, const char *path, size_t *len)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "chip8: cannot open %s\n", path);
        return -1;
    }

    struct stat st;
    if (fstat(fd, &st) != 0) {
        fprintf(stderr, "chip8: cannot stat %s\n", path);
        close(fd);
        return -1;
    }
    if (!S_ISREG(st.st_mode)) {
        fprintf(stderr, "chip8: %s is not a regular file\n", path);
        close(fd);
        return -1;
    }

    uint8_t buf[PROG_MAX + 1];
    size_t want = sizeof buf;
    size_t got = 0;

    while (got < want) {
        ssize_t n = read(fd, buf + got, want - got);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            fprintf(stderr, "chip8: error reading %s\n", path);
            close(fd);
            return -1;
        }
        if (n == 0)
            break;
        got += (size_t)n;
    }
    close(fd);

    if (got == 0) {
        fprintf(stderr, "chip8: %s is empty\n", path);
        return -1;
    }

    bool truncated = false;
    if (!chip8_load_rom(c, buf, got, &truncated)) {
        fprintf(stderr, "chip8: %s is empty\n", path);
        return -1;
    }
    if (truncated)
        fprintf(stderr, "chip8: warning: %s exceeds %d bytes, truncated\n",
                path, PROG_MAX);

    if (len)
        *len = truncated ? (size_t)PROG_MAX : got;
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
