#define _GNU_SOURCE
#include "cshell/test_framework.h"
#include "cshell/tracker.h"

#include <fcntl.h>
#include <unistd.h>

static void test_tracker_lifecycle(void) {
  cshell_tracker_init();

  int j1 = cshell_tracker_add(12345, "sleep 10 &");
  ASSERT_INT_EQ(j1, 1, "First background job should obtain job ID 1");

  int j2 = cshell_tracker_add(12346, "echo hello &");
  ASSERT_INT_EQ(j2, 2, "Second background job should obtain job ID 2");

  for (int i = 0; i < MAX_JOBS - 2; i++)
    cshell_tracker_add(20000 + i, "dummy_cd &");
  int overflow_id = cshell_tracker_add(99999, "overflow &");
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

  int j_id = cshell_tracker_add(pid, "successful_job &");
  ASSERT_INT_EQ(j_id, 1, "Real child process should occupy slot 1");

  usleep(50000); // time to transition child to zombie state

  const char *log_file = "_test_tracker_done.txt";
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

  int j_id = cshell_tracker_add(pid, "failing_job &");
  ASSERT_INT_EQ(j_id, 1, "Failing child process should occupy slot 1");

  usleep(50000); // time to transition child to zombie state

  const char *log_file = "_test_tracker_fail.txt";
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

void test_tracker_reaping(void) {
  test_tracker_reaping_success();
  test_tracker_reaping_failure();
}

int main(void) {
  printf("\nRunning: %s\n", __FILE__);

  test_tracker_lifecycle();
  test_tracker_reaping();

  test_summary();
  return tests_failed > 0 ? 1 : 0;
}
