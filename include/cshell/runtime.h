#ifndef CSHELL_RUNTIME_H
#define CSHELL_RUNTIME_H

typedef struct {
  int last_exit_status;
} ShellRuntime;

extern ShellRuntime shell_r;

#endif
