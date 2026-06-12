#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cshell/executor.h"
#include "cshell/parser.h"

#define MAX_LINE_LENGTH 4096

static void handle_shutdown(int sig) {
  (void)sig;
  exit(1);
}

int main(void) {
  signal(SIGTERM, handle_shutdown);

  char line[MAX_LINE_LENGTH];
  Command cmd;
  memset(&cmd, 0, sizeof(Command));

  while (1) {
    printf("cshell> ");
    fflush(stdout);

    if (fgets(line, sizeof(line), stdin) == NULL) {
      printf("\n");
      handle_shutdown(SIGTERM);
    }

    if (cshell_parse_line(line, &cmd) == -1)
      continue;

    if (cshell_execute(&cmd) == SHELL_STATUS_EXIT)
      handle_shutdown(SIGTERM);
  }
  return 0;
}
