#ifndef CSHELL_STARTUP_H
#define CSHELL_STARTUP_H

#include "clib/arena.h"
#include "cshell/parser.h"

#define STARTUP_TARGET_FILENAME ".cshellrc"

int cshell_run_startup(Pipeline *pipe, Arena *arena);

#endif
