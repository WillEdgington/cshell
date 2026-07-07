#define _GNU_SOURCE
#include "cshell/test_framework.h"
#include "cshell/tracker.h"

#include <fcntl.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <unistd.h>

static void test_tracker_lifecycle(void) {
  cshell_tracker_init();

  int j1 = cshell_tracker_add(12345, "sleep 10 &", JOB_RUNNING);
  ASSERT_INT_EQ(j1, 1, "First background job should obtain job ID 1");

  int j2 = cshell_tracker_add(12346, "echo hello &", JOB_RUNNING);
  ASSERT_INT_EQ(j2, 2, "Second background job should obtain job ID 2");

  for (int i = 0; i < MAX_JOBS - 2; i++)
    cshell_tracker_add(20000 + i, "dummy_cd &", JOB_RUNNING);
  int overflow_id = cshell_tracker_add(99999, "overflow &", JOB_RUNNING);
  ASSERT_INT_EQ(
      overflow_id, -1,
      "Tracker must reject allocation when table limits are saturated");
}

static void test_tracker_reaping_success(void) {
  cshell_tracker_init();

  pid_t pid = fork();
  if (pid == 0) {
    _exit(0); // successful exit
  }

  int j_id = cshell_tracker_add(pid, "successful_job &", JOB_RUNNING);
  ASSERT_INT_EQ(j_id, 1, "Real child process should occupy slot 1");

  usleep(50000); // time to transition child to zombie state

  const char *log_file = "_test_tracker_done_99553341.txt";
  unlink(log_file);

  int saved_stdout = dup(STDOUT_FILENO);
  int out_fd = open(log_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (out_fd >= 0) {
    dup2(out_fd, STDOUT_FILENO);
    close(out_fd);
  }

  cshell_tracker_report_and_clean(0); // don't mute

  if (saved_stdout >= 0) {
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);
  }

  FILE *f = fopen(log_file, "r");
  ASSERT_PTR_NOT_NULL(f, "Tracker log file should be openable");

  char buffer[128] = {0};
  if (f != NULL) {
    char *res = fgets(buffer, sizeof(buffer), f);
    fclose(f);
    ASSERT_PTR_NOT_NULL(res, "Tracker must write a status log on termination");
    ASSERT_STR_EQ(buffer, "[1]+  Done                    successful_job &\n",
                  "Output format must match expected successful job layout");
  }
  unlink(log_file);
}

static void test_tracker_reaping_failure(void) {
  cshell_tracker_init();

  pid_t pid = fork();
  if (pid == 0) {
    _exit(42); // failure exit
  }

  int j_id = cshell_tracker_add(pid, "failing_job &", JOB_RUNNING);
  ASSERT_INT_EQ(j_id, 1, "Failing child process should occupy slot 1");

  usleep(50000); // time to transition child to zombie state

  const char *log_file = "_test_tracker_fail_77778934.txt";
  unlink(log_file);

  int saved_stdout = dup(STDOUT_FILENO);
  int out_fd = open(log_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (out_fd >= 0) {
    dup2(out_fd, STDOUT_FILENO);
    close(out_fd);
  }

  cshell_tracker_report_and_clean(0); // don't mute

  if (saved_stdout >= 0) {
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);
  }

  FILE *f = fopen(log_file, "r");
  char buffer[128] = {0};
  if (f != NULL) {
    fgets(buffer, sizeof(buffer), f);
    fclose(f);
    ASSERT_STR_EQ(buffer, "[1]+  Exit 42                failing_job &\n",
                  "Output format must match expected failing job layout");
  }
  unlink(log_file);
}

static void test_tracker_reaping(void) {
  test_tracker_reaping_success();
  test_tracker_reaping_failure();
}

static void test_tracker_suspension_and_resumption(void) {
  cshell_tracker_init();

  pid_t pid = fork();
  if (pid == 0) {
    while (1) {
      usleep(10000);
    }
  }

  int j_id = cshell_tracker_add(pid, "interactive_job &", JOB_RUNNING);
  ASSERT_INT_EQ(j_id, 1, "Job should securely allocate into slot 1");

  kill(pid, SIGSTOP);
  usleep(50000);

  const char *stop_log = "_test_tracker_stop_48593222.txt";
  unlink(stop_log);

  int saved_stdout = dup(STDOUT_FILENO);
  int out_fd = open(stop_log, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (out_fd >= 0) {
    dup2(out_fd, STDOUT_FILENO);
    close(out_fd);
  }

  cshell_tracker_report_and_clean(0); // don't mute

  if (saved_stdout >= 0) {
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);
  }

  FILE *f_stop = fopen(stop_log, "r");
  char buffer[128] = {0};
  if (f_stop != NULL) {
    fgets(buffer, sizeof(buffer), f_stop);
    fclose(f_stop);
    ASSERT_STR_EQ(
        buffer, "[1]+  Stopped                    interactive_job &\n",
        "Tracker report should reflect the explicit Stopped state change");
  }
  unlink(stop_log);

  kill(pid, SIGCONT);
  usleep(50000);

  cshell_tracker_report_and_clean(1);

  kill(pid, SIGKILL);
  int dummy;
  waitpid(pid, &dummy, 0);
}

static void test_tracker_retrieval_getters(void) {
  cshell_tracker_init();

  int j1 = cshell_tracker_add(55555, "sleep 50 &", JOB_RUNNING);
  int j2 = cshell_tracker_add(55556, "sleep 60", JOB_STOPPED);

  BackgroundJob *job_pt1 = cshell_tracker_get_by_id(j1);
  ASSERT_PTR_NOT_NULL(job_pt1, "Should retrieve job 1 pointer successfully");
  ASSERT_INT_EQ(job_pt1->pid, 55555,
                "Retrieved job PID must exactly match corresponding element");

  BackgroundJob *invalid_job_high = cshell_tracker_get_by_id(99);
  ASSERT_PTR_NULL(invalid_job_high,
                  "Requesting an unallocated job index must return NULL");

  BackgroundJob *invalid_job_low = cshell_tracker_get_by_id(0);
  ASSERT_PTR_NULL(invalid_job_low,
                  "Job index 0 is out of bounds and must return NULL");

  BackgroundJob *latest = cshell_tracker_get_latest();
  ASSERT_PTR_NOT_NULL(latest,
                      "Latest allocation lookup should not return NULL");
  ASSERT_INT_EQ(
      latest->job_id, j2,
      "Latest job pointer must return the highest indexed active allocation");
}

int main(void) {
  printf("\nRunning: %s\n", __FILE__);

  test_tracker_lifecycle();
  test_tracker_reaping();
  test_tracker_suspension_and_resumption();
  test_tracker_retrieval_getters();

  test_summary();
  return tests_failed > 0 ? 1 : 0;
}
