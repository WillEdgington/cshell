#include "cshell/executor.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int execute_cd(const Command *cmd) {
  if (cmd->arg_count == 1) {
    fprintf(stderr, "cshell: cd: missing argument\n");
    return -1;
  } else if (cmd->arg_count == 2) {
    if (chdir(cmd->args[1]) == -1) {
      perror("cshell: cd");
      return -1;
    }
    return 0;
  }
  errno = E2BIG;
  fprintf(stderr, "cshell: cd: too many arguments\n");
  return -1;
}

static int execute_external(const Command *cmd) {
  pid_t pid = fork();

  if (pid < 0) {
    perror("cshell: fork");
    return -1;
  }

  if (pid == 0) {
    execvp(cmd->args[0], cmd->args);
    perror("cshell: external");
    _exit(1); // clean deterministic failure exit
  }

  int status;
  if (waitpid(pid, &status, 0) == -1) {
    perror("cshell: waitpid");
    return -1;
  }

  if (WIFEXITED(status))
    return WEXITSTATUS(status);
  return -1;
}

CommandType cshell_resolve_command(const Command *cmd) {
  if (cmd->arg_count == 0) {
    return CMD_TYPE_EMPTY;
  } else if (strcmp(cmd->args[0], "exit") == 0) {
    return CMD_TYPE_EXIT;
  } else if (strcmp(cmd->args[0], "cd") == 0) {
    return CMD_TYPE_CD;
  }
  return CMD_TYPE_EXTERNAL;
}

int cshell_execute(Command *cmd) {
  switch (cshell_resolve_command(cmd)) {
  case CMD_TYPE_EMPTY:
    return 0;
  case CMD_TYPE_EXIT:
    return SHELL_STATUS_EXIT;
  case CMD_TYPE_CD:
    return execute_cd(cmd);
  case CMD_TYPE_EXTERNAL:
    return execute_external(cmd);
  default:
    fprintf(stderr, "cshell: excecute: unknown command type\n");
    return -1;
  }
}
