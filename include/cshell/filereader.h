#ifndef CSHELL_FILEREADER_H
#define CSHELL_FILEREADER_H

#include "clib/arena.h"
#include "cshell/parser.h"

#define PATH_NOT_FOUND_STATUS 1

int cshell_process_file(const char *path, Pipeline *pipe, Arena *arena,
                        int mute);

#endif