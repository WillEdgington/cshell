#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clib/arena.h"
#include "cshell/executor.h"
#include "cshell/parser.h"
#include "cshell/prompt.h"
#include "cshell/tracker.h"
#include "cshell/runtime.h"
#include "cshell/handler.h"

#define MAX_LINE_LENGTH 4096
#define PIPELINE_ARENA_SLAB_SIZE 8192 // 8 KB

static void handle_shutdown(int sig) {
  (void)sig;
  printf("\n");
  exit(1);
}

static void handle_exit(int sig, Arena *a) {
  arena_free(a);
  handle_shutdown(sig);
}

int main(void) {
  signal(SIGINT, SIG_IGN);
  signal(SIGTERM, handle_shutdown);

  char line[MAX_LINE_LENGTH];
  Pipeline pipe;
  Arena arena;

  arena_init(&arena, PIPELINE_ARENA_SLAB_SIZE);
  cshell_init_signals();
  cshell_tracker_init();

  shell_r.last_exit_status = 0;

  while (1) {
    arena_reset(&arena);
    pipeline_init(&pipe, &arena);
    cshell_tracker_report_and_clean();

    cshell_display_prompt();

    if (fgets(line, sizeof(line), stdin) == NULL) {
      if (errno != EINTR) {
        handle_exit(SIGTERM, &arena);
      }
      shell_r.last_exit_status = errno;
      errno = 0;
      continue;
    }

    if (cshell_parse_line(line, &pipe) == -1) {
      shell_r.last_exit_status = 1;
      continue;
    }
    
    cshell_handle_execution(&pipe);
    if (shell_r.last_exit_status == SHELL_STATUS_EXIT)
      handle_exit(SIGTERM, &arena);
  }
  arena_free(&arena);
  return 0;
}
