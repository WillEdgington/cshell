#define _GNU_SOURCE

#include "cshell/prompt.h"
#include "cshell/test_framework.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void capture_prompt_output(char *buffer, size_t max_len) {
  int pipefd[2];
  if (pipe(pipefd) == -1) {
    perror("pipe");
    return;
  }

  int old_stdout = dup(STDOUT_FILENO);
  dup2(pipefd[1], STDOUT_FILENO);
  close(pipefd[1]);

  cshell_display_prompt();
  fflush(stdout);

  dup2(old_stdout, STDOUT_FILENO);
  close(old_stdout);

  memset(buffer, 0, max_len);
  read(pipefd[0], buffer, max_len - 1);
  close(pipefd[0]);
}

static void test_prompt_home_truncation(void) {
  const char *mock_home = "/tmp/cshell_test_shorthand_84736599";
  mkdir(mock_home, 0777);

  char *orig_home = getenv(HOME_ENV_KEY);
  char *orig_cwd = getcwd(NULL, 0);

  setenv(HOME_ENV_KEY, mock_home, 1);
  chdir(mock_home);

  char output[PROMPT_PATH_MAX];
  capture_prompt_output(output, sizeof(output));

  const char *expected = "cshell:" PROMPT_CLR_PINK "~" PROMPT_CLR_RESET "> ";
  ASSERT_STR_EQ(output, expected,
                "Prompt at HOME directory must truncate path to '~'");

  if (orig_home)
    setenv(HOME_ENV_KEY, orig_home, 1);
  if (orig_cwd) {
    chdir(orig_cwd);
    free(orig_cwd);
  }
  rmdir(mock_home);
}

static void test_prompt_subdirectory_truncation(void) {
  const char *mock_home = "/tmp/cshell_test_sub_18274323";
  const char *mock_sub = "/tmp/cshell_test_sub_18274323/projects";
  mkdir(mock_home, 0777);
  mkdir(mock_sub, 0777);

  char *orig_home = getenv(HOME_ENV_KEY);
  char *orig_cwd = getcwd(NULL, 0);

  setenv(HOME_ENV_KEY, mock_home, 1);
  chdir(mock_sub);

  char output[PROMPT_PATH_MAX];
  capture_prompt_output(output, sizeof(output));

  const char *expected =
      "cshell:" PROMPT_CLR_PINK "~/projects" PROMPT_CLR_RESET "> ";
  ASSERT_STR_EQ(output, expected,
                "Prompt inside a sub-folder must accurately preserve relative "
                "trailing components");

  if (orig_home)
    setenv(HOME_ENV_KEY, orig_home, 1);
  if (orig_cwd) {
    chdir(orig_cwd);
    free(orig_cwd);
  }
  rmdir(mock_sub);
  rmdir(mock_home);
}

static void test_prompt_outside_home(void) {
  char *orig_home = getenv(HOME_ENV_KEY);
  char *orig_cwd = getcwd(NULL, 0);
  char *mock_home = "/tmp/unrelated_path_28749323";

  setenv(HOME_ENV_KEY, mock_home, 1);
  chdir("/");

  char output[PROMPT_PATH_MAX];
  capture_prompt_output(output, sizeof(output));

  const char *expected = "cshell:" PROMPT_CLR_PINK "/" PROMPT_CLR_RESET "> ";
  ASSERT_STR_EQ(output, expected,
                "Paths outside HOME context must print literal absolute "
                "strings without modifications");

  if (orig_home)
    setenv(HOME_ENV_KEY, orig_home, 1);
  if (orig_cwd) {
    chdir(orig_cwd);
    free(orig_cwd);
  }
}

int main(void) {
  printf("\nRunning: %s\n", __FILE__);

  test_prompt_home_truncation();
  test_prompt_subdirectory_truncation();
  test_prompt_outside_home();

  test_summary();
  return tests_failed > 0 ? 1 : 0;
}