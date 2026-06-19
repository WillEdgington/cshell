#ifndef CSHELL_TRACKER_H
#define CSHELL_TRACKER_H

#include <sys/types.h>

#define MAX_JOBS 32
#define MAX_CMD_STRING_LEN 255

typedef enum { JOB_RUNNING, JOB_DONE, JOB_FAILED } JobStatus;

typedef struct {
  pid_t pid;
  JobStatus status;
  int job_id;
  int exit_code;
  int is_allocated;
  char command_string[MAX_CMD_STRING_LEN + 1];
} BackgroundJob;

void cshell_tracker_init(void);
int cshell_tracker_add(pid_t pid, const char *cmd_str);
void cshell_tracker_report_and_clean(void);

#endif
