#define _GNU_SOURCE // expose linux and posix-specific APIs to compiler

#include "cshell/executor.h"
#include "clib/arena.h"
#include "cshell/expansion.h"
#include "cshell/filereader.h"
#include "cshell/runtime.h"
#include "cshell/tracker.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static void serialise_pipeline(const Pipeline *pipe, char *buf,
                               size_t max_len) {
  buf[0] = '\0';
  size_t cur_len = 0;

  for (Command *cmd = pipe->head; cmd != NULL; cmd = cmd->next) {
    for (int i = 0; i < cmd->arg_count && cmd->args[i] != NULL; i++) {
      size_t arg_len = strlen(cmd->args[i]);
      if (cur_len + arg_len + 4 >= max_len)
        break;
      if (cur_len > 0) {
        strcat(buf, " ");
        cur_len++;
      }
      strcat(buf, cmd->args[i]);
      cur_len += arg_len;
    }

    if (cmd->next != NULL && cur_len + 3 < max_len) {
      strcat(buf, " |");
      cur_len += 2;
    }
  }
  if (pipe->is_background && cur_len + 3 < max_len)
    strcat(buf, " &");
}

static void handle_sigchld(int sig) {
  (void)sig;
  // Do nothing: let tracker module handle reaping synchronously
}

void cshell_init_signals(void) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));

  sa.sa_handler = handle_sigchld;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;

  if (sigaction(SIGCHLD, &sa, NULL) == -1)
    perror("cshell: sigaction registration failed");
}

static int execute_cd(const Command *cmd) {
  if (cmd->arg_count == 1) {
    fprintf(stderr, "cshell: cd: missing argument\n");
    return 1;
  } else if (cmd->arg_count == 2) {
    if (chdir(cmd->args[1]) == -1) {
      perror("cshell: cd");
      return errno;
    }
    return 0;
  }
  errno = E2BIG;
  fprintf(stderr, "cshell: cd: too many arguments\n");
  return errno;
}

static void execute_external(const Command *cmd) {
  execvp(cmd->args[0], cmd->args);
  if (errno == ENOENT) {
    fprintf(stderr, "cshell: %s: command not found\n", cmd->args[0]);
    _exit(NOT_FOUND_STATUS);
  } else if (errno == EACCES) {
    fprintf(stderr, "cshell: %s: Permission denied\n", cmd->args[0]);
    _exit(DENIED_STATUS);
  } else {
    fprintf(stderr, "cshell: %s: %s\n", cmd->args[0], strerror(errno));
    _exit(1);
  }
}

static int execute_export(const Command *cmd) {
  if (cmd->arg_count < 2) {
    fprintf(stderr, "cshell: export: missing assignment statement\n");
    return 1;
  }

  char *assignment = cmd->args[1];
  char *delim = strchr(assignment, '=');
  if (delim == NULL) {
    fprintf(stderr, "cshell: export: invalid syntax, expected NAME=VALUE\n");
    return 1;
  }

  *delim = '\0';
  char *key = assignment;
  char *value = delim + 1;

  if (setenv(key, value, 1) == -1) {
    perror("cshell: export");
    return errno;
  }
  return 0;
}

static int execute_jobs(const Command *cmd) {
  if (cmd->arg_count == 1) {
    cshell_tracker_print_jobs();
    return 0;
  }

  int return_val = 0;
  for (int i = 1; i < cmd->arg_count; i++) {
    char *arg = cmd->args[i];
    if (arg[0] == '%')
      arg++; // skip optional '%' prefix

    char *end_ptr;
    long job_id = strtol(arg, &end_ptr, 10); // base 10

    if (*end_ptr == '\0' && job_id > 0 && job_id <= MAX_JOBS) {
      BackgroundJob *job = cshell_tracker_get_by_id((int)job_id);
      if (job != NULL) {
        cshell_tracker_print_job(*job);
        continue;
      }
    }
    return_val = 1;
    fprintf(stderr, "cshell: jobs: %s: no such job\n", cmd->args[i]);
  }

  return return_val;
}

static int execute_fg(const Command *cmd) {
  BackgroundJob *job = NULL;

  if (cmd->arg_count == 1) {
    job = cshell_tracker_get_latest();
    if (job == NULL) {
      fprintf(stderr, "cshell: fg: current: no such job\n");
      return 1;
    }
  } else {
    char *arg = cmd->args[1];
    if (arg[0] == '%')
      arg++;

    char *end_ptr;
    long job_id = strtol(arg, &end_ptr, 10);
    if (*end_ptr != '\0' || job_id <= 0 || job_id > MAX_JOBS ||
        (job = cshell_tracker_get_by_id((int)job_id)) == NULL) {
      fprintf(stderr, "cshell: fg: %s: no such job\n", cmd->args[1]);
      return 1;
    }
  }

  printf("%s\n", job->command_string);
  fflush(stdout);

  if (shell_r.is_interactive) {
    tcsetpgrp(STDIN_FILENO, job->pid);
  }

  if (kill(-job->pid, SIGCONT) == -1) {
    perror("cshell: fg: kill failed");
    if (shell_r.is_interactive)
      tcsetpgrp(STDIN_FILENO, getpgrp());
    return 1;
  }

  job->is_allocated = 0;

  int status;
  int terminal_exit_status = 0;

  pid_t reaped = waitpid(job->pid, &status, WUNTRACED);
  if (reaped > 0) {
    if (WIFEXITED(status)) {
      terminal_exit_status = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
      terminal_exit_status = 128 + WTERMSIG(status);
    } else if (WIFSTOPPED(status)) {
      terminal_exit_status = SHELL_STATUS_STOPPED;

      job->is_allocated = 1;
      job->status = JOB_STOPPED;
      printf("\n[%d]+  Stopped                 %s\n", job->job_id,
             job->command_string);
      fflush(stdout);
    }
  }

  if (shell_r.is_interactive) {
    tcsetpgrp(STDIN_FILENO, getpgrp());
  }
  return terminal_exit_status;
}

static int execute_bg(const Command *cmd) {
  if (cmd->arg_count == 1) {
    BackgroundJob *job = cshell_tracker_get_latest();
    if (job == NULL) {
      fprintf(stderr, "cshell: bg: current: no such job\n");
      return 1;
    }
    if (kill(-job->pid, SIGCONT) == -1) {
      perror("cshell: bg: kill failed");
      return 1;
    }

    job->status = JOB_RUNNING;
    printf("[%d]+ %s &\n", job->job_id, job->command_string);
    fflush(stdout);
    return 0;
  }

  int return_val = 0;

  for (int i = 1; i < cmd->arg_count; i++) {
    char *arg = cmd->args[i];
    if (arg[0] == '%')
      arg++;

    char *end_ptr;
    long job_id = strtol(arg, &end_ptr, 10); // base 10
    if (*end_ptr != '\0' || job_id <= 0 || job_id > MAX_JOBS) {
      fprintf(stderr, "cshell: bg: %s: no such job\n", cmd->args[i]);
      return_val = 1;
      continue;
    }

    BackgroundJob *job = cshell_tracker_get_by_id(job_id);
    if (job == NULL) {
      fprintf(stderr, "cshell: bg: %s: no such job\n", cmd->args[i]);
      return_val = 1;
      continue;
    }

    if (kill(-job->pid, SIGCONT) == -1) {
      perror("cshell: bg: kill failed");
      return 1;
    }

    job->status = JOB_RUNNING;
    printf("[%d]+ %s &\n", job->job_id, job->command_string);
    fflush(stdout);
  }
  return return_val;
}

static int execute_clear(const Command *cmd) {
  if (cmd->arg_count > 1) {
    fprintf(stderr, "cshell: clear: too many arguments\n");
    return 1;
  }
  write(STDOUT_FILENO, "\033[2J\033[3J\033[H", 11);
  fflush(stdout);
  return 0;
}

static int execute_source(const Command *cmd) {
  if (cmd->arg_count != 2) {
    fprintf(stderr, "cshell: source: filename argument required\n");
    shell_r.last_exit_status = 1;
    return 1;
  }

  Arena arena;
  arena_init(&arena, PIPELINE_ARENA_SLAB_SIZE);
  Pipeline pipe;

  int status = cshell_process_file(cmd->args[1], &pipe, &arena, 1);

  arena_free(&arena);
  if (status == PATH_NOT_FOUND_STATUS) {
    fprintf(stderr, "cshell: source: %s: No such file or directory\n",
            cmd->args[1]);
    return 1;
  }
  return 0;
}

static void setup_child_io(const Command *cmd, int prev_read_fd,
                           int tunnel[2]) {
  if (prev_read_fd != STDIN_FILENO) {
    if (dup2(prev_read_fd, STDIN_FILENO) == -1) {
      perror("cshell: child input pipe linking failed");
      _exit(errno);
    }
    close(prev_read_fd);
  }

  if (cmd->next != NULL) {
    close(tunnel[0]);
    if (dup2(tunnel[1], STDOUT_FILENO) == -1) {
      perror("cshell: child output pipe linking failed");
      _exit(errno);
    }
    close(tunnel[1]);
  }

  if (cmd->input_redirect != NULL) {
    int in_fd = open(cmd->input_redirect, O_RDONLY);
    if (in_fd == -1) {
      perror("cshell: input redirection");
      _exit(errno);
    }

    if (dup2(in_fd, STDIN_FILENO) == -1) {
      perror("cshell: dup2 input");
      close(in_fd);
      _exit(errno);
    }
    close(in_fd);
  }

  if (cmd->output_redirect != NULL) {
    int out_fd = open(cmd->output_redirect, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd == -1) {
      perror("cshell: output redirection");
      _exit(errno);
    }

    if (dup2(out_fd, STDOUT_FILENO) == -1) {
      perror("cshell: dup2 output");
      close(out_fd);
      _exit(errno);
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
    _exit(execute_cd(cmd));
  case CMD_TYPE_EXTERNAL:
    execute_external(cmd);
    break;
  case CMD_TYPE_EXPORT:
    _exit(execute_export(cmd));
  case CMD_TYPE_JOBS:
    _exit(execute_jobs(cmd));
  case CMD_TYPE_FG:
    _exit(execute_fg(cmd));
  case CMD_TYPE_BG:
    _exit(execute_bg(cmd));
  case CMD_TYPE_CLEAR:
    _exit(execute_clear(cmd));
  case CMD_TYPE_SOURCE:
    _exit(execute_source(cmd));
  default:
    _exit(1);
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

static int handle_background_branch(Pipeline *pipe, pid_t last_pid,
                                    sigset_t old_set) {
  char cmd_str[MAX_CMD_STRING_LEN + 1];
  serialise_pipeline(pipe, cmd_str, sizeof(cmd_str));

  int job_id = cshell_tracker_add(last_pid, cmd_str, JOB_RUNNING);
  if (job_id != -1) {
    printf("[%d] %d\n", job_id, (int)last_pid);
    fflush(stdout);
  }

  sigprocmask(SIG_SETMASK, &old_set, NULL);
  return 0;
}

static void cleanup_fork_failure(pid_t *pids, int prev_read_fd, int tunnel[2],
                                 int pid_count, sigset_t old_set) {
  if (prev_read_fd != STDIN_FILENO)
    close(prev_read_fd);
  if (tunnel[0] != -1)
    close(tunnel[0]);
  if (tunnel[1] != -1)
    close(tunnel[1]);

  for (int i = 0; i < pid_count; i++) {
    kill(pids[i], SIGTERM);
    int dummy;
    waitpid(pids[i], &dummy, 0);
  }

  sigprocmask(SIG_SETMASK, &old_set, NULL);
}

static int reap_pipeline(Pipeline *pipeline, pid_t *pids, pid_t last_pid,
                         int command_count) {
  int status;
  int terminal_exit_status = 1;

  for (int i = 0; i < command_count; i++) {
    pid_t reaped_pid = waitpid(pids[i], &status, WUNTRACED);
    if (reaped_pid == last_pid) {
      if (WIFEXITED(status)) {
        terminal_exit_status = WEXITSTATUS(status);
      } else if (WIFSIGNALED(status)) {
        terminal_exit_status = 128 + WTERMSIG(status);
      } else if (WIFSTOPPED(status)) {
        terminal_exit_status = SHELL_STATUS_STOPPED;

        char cmd_str[MAX_CMD_STRING_LEN + 1];
        serialise_pipeline(pipeline, cmd_str, MAX_CMD_STRING_LEN + 1);
        int jid = cshell_tracker_add(last_pid, cmd_str, JOB_STOPPED);
        printf("\n[%d]+  Stopped                 %s\n", jid, cmd_str);
        fflush(stdout);
      }
    }
  }
  return terminal_exit_status;
}

static int execute_multi_cmd_pipeline(Pipeline *pipeline) {
  int prev_read_fd = STDIN_FILENO;
  pid_t last_pid = -1;
  Command *cmd = pipeline->head;

  pid_t pids[MAX_JOBS];
  if (pipeline->command_count > MAX_JOBS) {
    fprintf(stderr, "cshell: pipeline exceeds max job tracking capacity (%d)\n",
            MAX_JOBS);
    return -1;
  }

  sigset_t set, old_set;
  sigemptyset(&set);
  sigaddset(&set, SIGCHLD);

  if (sigprocmask(SIG_BLOCK, &set, &old_set) == -1) {
    perror("cshell: sigprocmask block failure");
    return -1;
  }

  pid_t pgid = 0;

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
      cleanup_fork_failure(pids, prev_read_fd, tunnel, i, old_set);
      return -1;
    }

    if (pid == 0) {
      sigprocmask(SIG_SETMASK, &old_set, NULL);

      if (i == 0) {
        setpgid(0, 0);
      } else {
        setpgid(0, pgid);
      }

      if (!pipeline->is_background && shell_r.is_interactive)
        tcsetpgrp(STDIN_FILENO, (i == 0) ? getpid() : pgid);

      signal(SIGTSTP, SIG_DFL);
      signal(SIGINT, SIG_DFL);

      setup_child_io(cmd, prev_read_fd, tunnel);
      execute_child_dispatch(cmd);
    }

    if (i == 0)
      pgid = pid;

    setpgid(pid, pgid);

    if (!pipeline->is_background && shell_r.is_interactive)
      tcsetpgrp(STDIN_FILENO, pgid);

    update_parent_fds(&prev_read_fd, tunnel, cmd->next != NULL ? 1 : 0);

    pids[i] = pid;
    last_pid = pid;
    cmd = cmd->next;
  }

  if (pipeline->is_background)
    return handle_background_branch(pipeline, last_pid, old_set);

  int status = reap_pipeline(pipeline, pids, last_pid, pipeline->command_count);

  // Reclaim terminal control
  if (!pipeline->is_background && shell_r.is_interactive)
    tcsetpgrp(STDIN_FILENO, getpgrp());

  sigprocmask(SIG_SETMASK, &old_set, NULL);
  return status;
}

CommandType cshell_resolve_command(const Command *cmd) {
  if (cmd->arg_count == 0) {
    return CMD_TYPE_EMPTY;
  } else if (strcmp(cmd->args[0], "exit") == 0) {
    return CMD_TYPE_EXIT;
  } else if (strcmp(cmd->args[0], "cd") == 0) {
    return CMD_TYPE_CD;
  } else if (strcmp(cmd->args[0], "export") == 0) {
    return CMD_TYPE_EXPORT;
  } else if (strcmp(cmd->args[0], "jobs") == 0) {
    return CMD_TYPE_JOBS;
  } else if (strcmp(cmd->args[0], "fg") == 0) {
    return CMD_TYPE_FG;
  } else if (strcmp(cmd->args[0], "bg") == 0) {
    return CMD_TYPE_BG;
  } else if (strcmp(cmd->args[0], "clear") == 0) {
    return CMD_TYPE_CLEAR;
  } else if (strcmp(cmd->args[0], "source") == 0 ||
             strcmp(cmd->args[0], ".") == 0) {
    return CMD_TYPE_SOURCE;
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
  case CMD_TYPE_EXPORT:
    return execute_export(cmd);
  case CMD_TYPE_JOBS:
    return execute_jobs(cmd);
  case CMD_TYPE_FG:
    return execute_fg(cmd);
  case CMD_TYPE_BG:
    return execute_bg(cmd);
  case CMD_TYPE_CLEAR:
    return execute_clear(cmd);
  case CMD_TYPE_SOURCE:
    return execute_source(cmd);
  default:
    fprintf(stderr, "cshell: excecute: unknown command type\n");
    return -1;
  }
}

int cshell_execute_pipeline(Pipeline *pipe) {
  if (pipe == NULL || pipe->command_count == 0)
    return 0;

  if (cshell_expand_pipeline(pipe) == -1)
    return -1;

  if (pipe->command_count == 1) {
    CommandType type = cshell_resolve_command(pipe->head);
    if (type != CMD_TYPE_EXTERNAL)
      return cshell_execute_command(pipe->head);
  }
  return execute_multi_cmd_pipeline(pipe);
}
