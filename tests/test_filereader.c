#define _DEFAULT_SOURCE

#include "clib/arena.h"
#include "cshell/filereader.h"
#include "cshell/test_framework.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void test_filereader_missing_file(Arena *a) {
  Pipeline pipe;

  ASSERT_INT_EQ(
      cshell_process_file("_non_existent_file_xyz.txt", &pipe, a, 1),
      PATH_NOT_FOUND_STATUS,
      "Processing a missing file must return PATH_NOT_FOUND_STATUS macro");
  arena_reset(a);
}

static void test_filereader_valid_script(Arena *a) {
  const char *test_script = "_test_script_57849111.txt";
  FILE *f = fopen(test_script, "w");
  ASSERT_PTR_NOT_NULL(f,
                      "Should be able to create a temporary test script file");

  char *test_var_key = "TEST_FILEREADER_VAR_58945261";
  if (f != NULL) {
    fprintf(f, "\n");
    fprintf(f, "# This is a comment that should be skipped\n");
    fprintf(f, "export %s=success\n", test_var_key);
    fclose(f);
  } else {
    ASSERT_PTR_NOT_NULL(
        f,
        "Should be able to create a temporary test script file using fopen()");
  }

  Pipeline pipe;
  int status = cshell_process_file(test_script, &pipe, a, 1);
  arena_reset(a);

  ASSERT_INT_EQ(status, 0,
                "Processing a valid script should complete with status 0");

  char *val = getenv(test_var_key);
  ASSERT_PTR_NOT_NULL(
      val,
      "The env assignment statement inside the script should have executed");
  if (val != NULL) {
    ASSERT_STR_EQ(val, "success",
                  "The variable mutation should match the script payload");
  }

  unsetenv(test_var_key);
  unlink(test_script);
}

int main(void) {
  printf("\nRunning: %s\n", __FILE__);

  Arena a;
  arena_init(&a, 1024);

  test_filereader_missing_file(&a);
  test_filereader_valid_script(&a);

  arena_free(&a);

  test_summary();
  return tests_failed > 0 ? 1 : 0;
}