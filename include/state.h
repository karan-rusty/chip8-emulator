#ifndef STATE_H
#define STATE_H

#include "chip8.h"

/* Host-side persistence for the core's snapshot format. Both return 0 on
 * success and -1 (with a message on stderr) on failure. */
int state_save_file(const Chip8 *c, const char *path);
int state_load_file(Chip8 *c, const char *path);

#endif
