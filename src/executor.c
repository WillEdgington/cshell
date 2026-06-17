#include "cshell/executor.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
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

static void execute_external(const Command *cmd) {
  execvp(cmd->args[0], cmd->args);
  perror("cshell: external");
  _exit(CHILD_EXEC_FAILURE);
}

static int execute_external_with_subshell_creation(const Command *cmd) {
  pid_t pid = fork();

  if (pid < 0) {
    perror("cshell: fork");
    return -1;
  }

  if (pid == 0) {
    signal(SIGINT, SIG_DFL);
    if (cmd->input_redirect != NULL) {
      int in_fd = open(cmd->input_redirect, O_RDONLY);
      if (in_fd == -1) {
        perror("cshell: input redirection");
        _exit(CHILD_EXEC_FAILURE);
      }

      if (dup2(in_fd, STDIN_FILENO) == -1) {
        perror("cshell: dup2 input");
        close(in_fd);
        _exit(CHILD_EXEC_FAILURE);
      }
      close(in_fd);
    }

    if (cmd->output_redirect != NULL) {
      int out_fd =
          open(cmd->output_redirect, O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (out_fd == -1) {
        perror("cshell: output redirection");
        _exit(CHILD_EXEC_FAILURE);
      }

      if (dup2(out_fd, STDOUT_FILENO) == -1) {
        perror("cshell: dup2 output");
        close(out_fd);
        _exit(CHILD_EXEC_FAILURE);
      }
      close(out_fd);
    }

    execute_external(cmd);
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

static void setup_child_io(const Command *cmd, int prev_read_fd,
                           int tunnel[2]) {
  if (prev_read_fd != STDIN_FILENO) {
    if (dup2(prev_read_fd, STDIN_FILENO) == -1) {
      perror("cshell: child input pipe linking failed");
      _exit(CHILD_EXEC_FAILURE);
    }
    close(prev_read_fd);
  }

  if (cmd->next != NULL) {
    close(tunnel[0]);
    if (dup2(tunnel[1], STDOUT_FILENO) == -1) {
      perror("cshell: child output pipe linking failed");
      _exit(CHILD_EXEC_FAILURE);
    }
    close(tunnel[1]);
  }

  if (cmd->input_redirect != NULL) {
    int in_fd = open(cmd->input_redirect, O_RDONLY);
    if (in_fd == -1) {
      perror("cshell: input redirection");
      _exit(CHILD_EXEC_FAILURE);
    }

    if (dup2(in_fd, STDIN_FILENO) == -1) {
      perror("cshell: dup2 input");
      close(in_fd);
      _exit(CHILD_EXEC_FAILURE);
    }
    close(in_fd);
  }

  if (cmd->output_redirect != NULL) {
    int out_fd = open(cmd->output_redirect, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd == -1) {
      perror("cshell: output redirection");
      _exit(CHILD_EXEC_FAILURE);
    }

    if (dup2(out_fd, STDOUT_FILENO) == -1) {
      perror("cshell: dup2 output");
      close(out_fd);
      _exit(CHILD_EXEC_FAILURE);
    }
    close(out_fd);
  }
}

static void execute_child_dispatch(const Command *cmd) {
  switch (cshell_resolve_command(cmd)) {
  case CMD_TYPE_EXIT:
  case CMD_TYPE_EMPTY:
    _exit(0);
  case CMD_TYPE_CD:
    _exit(execute_cd(cmd) == 0 ? 0 : CHILD_EXEC_FAILURE);
  case CMD_TYPE_EXTERNAL:
    execute_external(cmd);
    break;
  default:
    _exit(CHILD_EXEC_FAILURE);
  }
}

static void update_parent_fds(int *prev_read_fd, int tunnel[2], int has_next) {
  if (*prev_read_fd != STDIN_FILENO)
    close(*prev_read_fd);

  if (has_next == 1) {
    close(tunnel[1]);
    *prev_read_fd = tunnel[0];
  }
}

static int reap_pipeline(pid_t last_pid, int command_count) {
  int status;
  int terminal_exit_status = -1;

  for (int i = 0; i < command_count; i++) {
    pid_t reaped_pid = wait(&status);
    if (reaped_pid == last_pid) {
      terminal_exit_status = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
  }
  return terminal_exit_status;
}

static int execute_multi_cmd_pipeline(Pipeline *pipeline) {
  int prev_read_fd = STDIN_FILENO;
  pid_t last_pid = -1;
  Command *cmd = pipeline->head;

  for (int i = 0; i < pipeline->command_count; i++) {
    int tunnel[2] = {-1, -1};

    if (cmd->next != NULL) {
      if (pipe(tunnel) == -1) {
        perror("cshell: pipe creation failure");
        return -1;
      }
    }

    pid_t pid = fork();
    if (pid < 0) {
      perror("cshell: fork failure");
      return -1;
    }

    if (pid == 0) {
      signal(SIGINT, SIG_DFL);
      setup_child_io(cmd, prev_read_fd, tunnel);
      execute_child_dispatch(cmd);
    }

    update_parent_fds(&prev_read_fd, tunnel, cmd->next != NULL ? 1 : 0);

    last_pid = pid;
    cmd = cmd->next;
  }

  return reap_pipeline(last_pid, pipeline->command_count);
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

int cshell_execute_command(Command *cmd) {
  switch (cshell_resolve_command(cmd)) {
  case CMD_TYPE_EMPTY:
    return 0;
  case CMD_TYPE_EXIT:
    return SHELL_STATUS_EXIT;
  case CMD_TYPE_CD:
    return execute_cd(cmd);
  case CMD_TYPE_EXTERNAL:
    return execute_external_with_subshell_creation(cmd);
  default:
    fprintf(stderr, "cshell: excecute: unknown command type\n");
    return -1;
  }
}

int cshell_execute_pipeline(Pipeline *pipe) {
  if (pipe == NULL || pipe->command_count == 0)
    return 0;

  if (pipe->command_count == 1)
    return cshell_execute_command(pipe->head);
  return execute_multi_cmd_pipeline(pipe);
}
