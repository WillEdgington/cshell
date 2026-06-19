#include "cshell/tracker.h"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

static BackgroundJob job_table[MAX_JOBS];

static void print_job(BackgroundJob job) {
  switch (job.status) {
  case JOB_DONE:
    printf("[%d]+  Done                    %s\n", job.job_id,
           job.command_string);
    break;
  case JOB_FAILED:
    printf("[%d]+  Exit %d                %s\n", job.job_id, job.exit_code,
           job.command_string);
    break;
  case JOB_RUNNING:
    return;
  }
}

void cshell_tracker_init(void) {
  for (int i = 0; i < MAX_JOBS; i++) {
    job_table[i].is_allocated = 0;
    job_table[i].pid = 0;
    job_table[i].job_id = 0;
    job_table[i].status = JOB_RUNNING;
    job_table[i].command_string[0] = '\0';
  }
}

int cshell_tracker_add(pid_t pid, const char *cmd_str) {
  for (int i = 0; i < MAX_JOBS; i++) {
    if (job_table[i].is_allocated == 1)
      continue;

    job_table[i].is_allocated = 1;
    job_table[i].pid = pid;
    job_table[i].job_id = i + 1;
    job_table[i].status = JOB_RUNNING;
    strncpy(job_table[i].command_string, cmd_str, MAX_CMD_STRING_LEN);
    job_table[i].command_string[MAX_CMD_STRING_LEN] = '\0';
    return i + 1;
  }
  return -1;
}

void cshell_tracker_report_and_clean(void) {
  for (int i = 0; i < MAX_JOBS; i++) {
    if (job_table[i].is_allocated == 0)
      continue;

    int status;
    pid_t res = waitpid(job_table[i].pid, &status, WNOHANG);

    if (res == 0) { // still running
      continue;
    } else if (res > 0) { // terminate or changed state
      if (WIFEXITED(status)) {
        job_table[i].exit_code = WEXITSTATUS(status);
        job_table[i].status =
            (job_table[i].exit_code == 0) ? JOB_DONE : JOB_FAILED;
      } else if (WIFSIGNALED(status)) {
        job_table[i].exit_code = WTERMSIG(status);
        job_table[i].status = JOB_FAILED;
      }

      print_job(job_table[i]);
      fflush(stdout);
      job_table[i].is_allocated = 0;
    } else if (res == -1) { // no longer exists or reaped internally
      job_table[i].is_allocated = 0;
    }
  }
}
