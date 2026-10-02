#include "frontend.h"

#include <stddef.h>
#include <string.h>

const Frontend *frontend_by_name(const char *name)
{
    const Frontend *backends[] = { frontend_terminal() };

    for (size_t i = 0; i < sizeof backends / sizeof *backends; i++)
        if (strcmp(backends[i]->name, name) == 0)
            return backends[i];
    return NULL;
}
