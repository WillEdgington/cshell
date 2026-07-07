#define _GNU_SOURCE

#include "clib/arena.h"
#include "cshell/parser.h"
#include "cshell/runtime.h"
#include "cshell/startup.h"
#include "cshell/test_framework.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

void setup_mock_dir(const char *path) { mkdir(path, 0777); }

void teardown_mock_dir(const char *path) { rmdir(path); }

void setup_mock_file(const char *path, const char *content) {
  FILE *f = fopen(path, "w");
  if (f != NULL) {
    if (content != NULL)
      fprintf(f, "%s", content);
    fclose(f);
  }
}

void teardown_mock_file(const char *path) { unlink(path); }

static void test_startup_execution_flow(Arena *a) {
  Pipeline pipe;
  shell_r.last_exit_status = 0;

  char *mock_env_dir = "_test_home_58392012";
  char mock_rc_filepath[128];
  snprintf(mock_rc_filepath, sizeof(mock_rc_filepath), "%s/%s", mock_env_dir,
           STARTUP_TARGET_FILENAME);

  setup_mock_dir(mock_env_dir);

  setup_mock_file(mock_rc_filepath,
                  "# This is a comment configuration row\n"
                  "export STARTUP_TEST_VAR=functional_success");

  char *original_home = getenv("HOME");
  setenv("HOME", mock_env_dir, 1);

  int status = cshell_run_startup(&pipe, a);

  if (original_home) {
    setenv("HOME", original_home, 1);
  } else {
    unsetenv("HOME");
  }

  teardown_mock_file(mock_rc_filepath);
  teardown_mock_dir(mock_env_dir);

  ASSERT_INT_EQ(status, 0, "Startup routine must complete cleanly");

  char *resolved_env = getenv("STARTUP_TEST_VAR");
  ASSERT_PTR_NOT_NULL(resolved_env, "Environment modification statements in "
                                    "script must execute successfully");
  ASSERT_STR_EQ(resolved_env, "functional_success",
                "Export assignments must persist in parent memory space");

  unsetenv("STARTUP_TEST_VAR");
}

int main(void) {
  printf("\nRunning: %s\n", __FILE__);

  Arena a;
  arena_init(&a, 4096);

  test_startup_execution_flow(&a);

  arena_free(&a);

  test_summary();
  return tests_failed > 0 ? 1 : 0;
}
