#define _GNU_SOURCE

#include "clib/arena.h"
#include "cshell/expansion.h"
#include "cshell/parser.h"
#include "cshell/test_framework.h"
#include "cshell/runtime.h"

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

static void test_successful_expansion(Arena *a) {
  char *test_exp = "$CSHELL_TEST_ENV_12840423";
  char *test_key = test_exp + 1;
  char *test_val = "validated_payload";
  setenv(test_key, test_val, 1);

  Pipeline pipe;
  pipeline_init(&pipe, a);
  pipe.head = &(Command){
      .args = {"echo", test_exp, NULL}, .arg_count = 2, .next = NULL};
  pipe.command_count = 1;

  ASSERT_INT_EQ(cshell_expand_pipeline(&pipe), 0,
                "Valid environment variable expansion should return 0");
  ASSERT_STR_EQ(pipe.head->args[1], test_val,
                "Argument should be replaced with the environment value");

  unsetenv(test_key);
}

static void test_missing_expansion(Arena *a) {
  char invalid_exp[] = "$NON_EXISTENT_VAR_29548301";

  Pipeline pipe;
  pipeline_init(&pipe, a);
  pipe.head = &(Command){
      .args = {"echo", invalid_exp, NULL}, .arg_count = 2, .next = NULL};
  pipe.command_count = 1;

  cshell_expand_pipeline(&pipe);

  ASSERT(*pipe.head->args[1] == '\0',
        "Unset environment variable expansion must replace arg with empty string ('\\0')");
}

static void test_last_exit_status_expansion(Arena *a) {
  shell_r.last_exit_status = 0;

  Pipeline pipe;
  pipeline_init(&pipe, a);
  pipe.head = &(Command){
      .args = {"echo", "$?", NULL}, .arg_count = 2, .next = NULL};
  pipe.command_count = 1;

  cshell_expand_pipeline(&pipe);

  ASSERT_STR_EQ(pipe.head->args[1], "0", "expansion of '$?' should resolve the last exit status");
}

int main(void) {
  printf("\nRunning: %s\n", __FILE__);

  Arena a;
  arena_init(&a, 4096);

  test_successful_expansion(&a);
  test_missing_expansion(&a);
  test_last_exit_status_expansion(&a);

  arena_free(&a);

  test_summary();
  return tests_failed > 0 ? 1 : 0;
}
