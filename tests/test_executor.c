#include "cshell/executor.h"
#include "cshell/test_framework.h"
#include <stdio.h>
#include <unistd.h>

static void test_empty_arg(void) {
  Command cmd = {.args = {NULL}, .arg_count = 0};
  ASSERT_INT_EQ(cshell_resolve_command(&cmd), CMD_TYPE_EMPTY,
                "Empty argument (\"\") should resolve correctly");
  ASSERT_INT_EQ(
      cshell_execute(&cmd), 0,
      "Empty argument (\"\") should return 0 immediately in execution");
}

static void test_exit_arg(void) {
  Command cmd = {.args = {"exit", NULL}, .arg_count = 1};
  ASSERT_INT_EQ(cshell_resolve_command(&cmd), CMD_TYPE_EXIT,
                "Exit argument (\"exit\") should resolve correctly");
  ASSERT_INT_EQ(
      cshell_execute(&cmd), SHELL_STATUS_EXIT,
      "Exit argument (\"exit\") should return SHELL_STATUS_EXIT in execution");
}

static void test_cd_arg(void) {
  Command cmd = {.args = {"cd", "..", NULL}, .arg_count = 2};
  ASSERT_INT_EQ(cshell_resolve_command(&cmd), CMD_TYPE_CD,
                "Valid cd argument (\"cd ..\") should resolve correctly");
}

static void test_redirection(void) {
  const char *src_file = "_test_input_source_57382933.txt";
  const char *dest_file = "_test_input_dest_85937832.txt";

  unlink(src_file);
  unlink(dest_file);

  // Test output redirection alone (using echo)
  Command echo_cmd = {.args = {"echo", "systems_programming", NULL},
                      .arg_count = 2,
                      .input_redirect = NULL,
                      .output_redirect = (char *)src_file,
                      .is_background = 0};

  ASSERT_INT_EQ(cshell_execute(&echo_cmd), 0,
                "Pipeline execution with output redirection should return 0");

  FILE *src_f = fopen(src_file, "r");
  ASSERT_PTR_NOT_NULL(
      src_f, "Output redirection file should have been created on disk");

  char src_buffer[64];
  if (src_f != NULL) {
    char *fgets_src_result = fgets(src_buffer, sizeof(src_buffer), src_f);
    fclose(src_f);

    ASSERT_PTR_NOT_NULL(fgets_src_result,
                        "Should be able to read data from the redirected file");
    ASSERT_STR_EQ(src_buffer, "systems_programming\n",
                  "File content must exactly match redirected payload");
  }

  // Test input/output direction in unison (using cat)
  Command cat_cmd = {.args = {"cat", NULL},
                     .arg_count = 1,
                     .input_redirect = (char *)src_file,
                     .output_redirect = (char *)dest_file,
                     .is_background = 0};

  ASSERT_INT_EQ(cshell_execute(&cat_cmd), 0,
                "Pipeline execution with dual redirection should return 0");

  FILE *dst_f = fopen(dest_file, "r");
  ASSERT_PTR_NOT_NULL(dst_f,
                      "Destination file should have been created on disk");

  char dst_buffer[64];
  if (dst_f != NULL) {
    char *fgets_dst_result = fgets(dst_buffer, sizeof(dst_buffer), dst_f);
    fclose(dst_f);

    ASSERT_PTR_NOT_NULL(
        fgets_dst_result,
        "Should be able to read data from the destination file");
    ASSERT_STR_EQ(
        dst_buffer, src_buffer,
        "Destination file content must match the source payload (for cat)");
  }

  unlink(src_file);
  unlink(dest_file);
}

static void test_external_arg(void) {
  Command cmd = {.args = {"ls", "-l", NULL}, .arg_count = 2};
  ASSERT_INT_EQ(cshell_resolve_command(&cmd), CMD_TYPE_EXTERNAL,
                "External argument (\"ls -l\") should resolve correctly");
  test_redirection();
}

static void test_all_args(void) {
  test_empty_arg();
  test_exit_arg();
  test_cd_arg();
  test_external_arg();
}

int main(void) {
  printf("\nRunning: %s\n", __FILE__);

  test_all_args();

  test_summary();
  return tests_failed > 0 ? 1 : 0;
}
