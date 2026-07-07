#ifndef CSHELL_RUNTIME_H
#define CSHELL_RUNTIME_H

#define MAX_LINE_LEN 4096

typedef struct {
  int last_exit_status;
  int is_interactive;
} ShellRuntime;

extern ShellRuntime shell_r;

#endif
