#ifndef ROM_H
#define ROM_H

#include "chip8.h"

#include <stddef.h>

/* Load path into the machine. On success writes the (possibly truncated)
 * length to *len when len is not NULL. Returns 0 on success, -1 on failure
 * with a message on stderr. */
int rom_load(Chip8 *c, const char *path, size_t *len);

void rom_dump(const Chip8 *c);

#endif
