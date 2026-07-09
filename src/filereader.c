#define _DEFAULT_SOURCE

#include "cshell/filereader.h"
#include "clib/arena.h"
#include "cshell/executor.h"
#include "cshell/handler.h"
#include "cshell/parser.h"
#include "cshell/runtime.h"
#include "cshell/tracker.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_file_from_path(const char *path, long *fsize) {
  FILE *f = fopen(path, "r");
  if (f == NULL)
    return NULL;

  fseek(f, 0, SEEK_END);
  *fsize = ftell(f);
  rewind(f);
  if (*fsize <= 0) {
    fclose(f);
    return NULL;
  }

  char *content = malloc(*fsize + 1);
  if (content == NULL) {
    fclose(f);
    return NULL;
  }

  size_t read_bytes = fread(content, 1, *fsize, f);
  content[read_bytes] = '\0';
  fclose(f);
  return content;
}

// 1 for true, 0 for false
static int is_eol(char c) {
  if (c == '\n' || c == '\r' || c == '\0')
    return 1;
  return 0;
}

// 1 for true, 0 for false
static int is_delim(char c) {
  if (c == '\n' || c == ' ' || c == '\t' || c == '\r')
    return 1;
  return 0;
}

static int get_and_split_line(char **start_line, char **nxt) {
  char *ptr = *start_line;
  while (is_delim(*ptr) == 1 || *ptr == '#') {
    if (*ptr == '#') {
      while (is_eol(*ptr) == 0) {
        *ptr = '\0';
        ptr++;
      }
    } else {
      *ptr = '\0';
      ptr++;
    }
  }

  *start_line = ptr;

  if (**start_line == '\0') {
    *nxt = *start_line;
    return 1;
  }

  while (is_eol(*ptr) == 0)
    ptr++;

  if (*ptr == '\0') {
    *nxt = ptr;
  } else {
    *ptr = '\0';
    *nxt = ptr + 1;
  }
  return 0;
}

int cshell_process_file(const char *path, Pipeline *pipe, Arena *arena,
                        int mute) {
  long fsize;
  char *content = read_file_from_path(path, &fsize);
  if (content == NULL)
    return PATH_NOT_FOUND_STATUS;

  char *start_line = content;
  char *next_line;

  while (get_and_split_line(&start_line, &next_line) == 0) {
    if (strlen(start_line) > MAX_LINE_LEN) {
      start_line = next_line;
    }
    pipeline_init(pipe, arena);
    cshell_tracker_report_and_clean(mute); // mute

    if (cshell_parse_line(start_line, pipe) != -1) {
      cshell_handle_execution(pipe);
      if (shell_r.last_exit_status == SHELL_STATUS_EXIT) {
        free(content);
        return SHELL_STATUS_EXIT;
      }
    } else {
      shell_r.last_exit_status = 1;
    }

    start_line = next_line;
    arena_reset(arena);
  }

  arena_reset(arena);
  cshell_tracker_report_and_clean(mute); // mute
  free(content);
  return 0;
}