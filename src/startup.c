#define _DEFAULT_SOURCE

#include "cshell/startup.h"
#include "clib/arena.h"
#include "cshell/filereader.h"
#include "cshell/parser.h"

#include <stdio.h>
#include <stdlib.h>

#define MAX_PATH_LEN 256

int cshell_run_startup(Pipeline *pipe, Arena *arena) {
  const char *home = getenv("HOME");
  if (home == NULL)
    return 0;

  char raw_path[MAX_PATH_LEN];
  snprintf(raw_path, MAX_PATH_LEN, "%s/%s", home, STARTUP_TARGET_FILENAME);

  int status = cshell_process_file(raw_path, pipe, arena, 1); // mute
  return status == PATH_NOT_FOUND_STATUS ? 0 : status;
}