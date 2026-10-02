#ifndef FRONTEND_H
#define FRONTEND_H

#include <stdbool.h>

#include "chip8.h"

/* One frame at 60Hz, ten core cycles per frame. Every backend paces with
 * these so a ROM behaves the same no matter which host runs it. */
#define FRAME_HZ         60
#define CYCLES_PER_FRAME 10

/* Every host backend owns exactly one state block (opaque to main) plus the
 * callbacks that drive it. main() speaks only this interface, so a new
 * backend is a new file that fills one of these in: no core edits and no new
 * globals. */
typedef struct Frontend {
    const char *name;
    void       *state;

    bool (*open)(void *state);
    void (*close)(void *state);
    void (*poll)(void *state, Chip8 *c, bool *quit);
    void (*present)(void *state, const Chip8 *c);
    void (*sound)(void *state, const Chip8 *c);
    void (*wait_frame)(void *state);
} Frontend;

const Frontend *frontend_terminal(void);
const Frontend *frontend_by_name(const char *name);

#endif
