#define _GNU_SOURCE

#include "cshell/executor.h"
#include "cshell/test_framework.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void test_empty_arg(void) {
  Command cmd = {.args = {NULL}, .arg_count = 0};
  ASSERT_INT_EQ(cshell_resolve_command(&cmd), CMD_TYPE_EMPTY,
                "Empty argument (\"\") should resolve correctly");
  ASSERT_INT_EQ(
      cshell_execute_command(&cmd), 0,
      "Empty argument (\"\") should return 0 immediately in execution");
}

static void test_exit_arg(void) {
  Command cmd = {.args = {"exit", NULL}, .arg_count = 1};
  ASSERT_INT_EQ(cshell_resolve_command(&cmd), CMD_TYPE_EXIT,
                "Exit argument (\"exit\") should resolve correctly");
  ASSERT_INT_EQ(
      cshell_execute_command(&cmd), SHELL_STATUS_EXIT,
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
                      .output_redirect = (char *)src_file};

  ASSERT_INT_EQ(cshell_execute_command(&echo_cmd), 0,
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
                     .output_redirect = (char *)dest_file};

  ASSERT_INT_EQ(cshell_execute_command(&cat_cmd), 0,
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

static void test_two_stage_pipeline(void) {
  Command cmd2 = {
      .args = {"grep", "systems", NULL}, .arg_count = 2, .next = NULL};
  Command cmd1 = {.args = {"echo", "systems_programming", NULL},
                  .arg_count = 2,
                  .next = &cmd2};
  Pipeline pipe = {
      .head = &cmd1, .tail = &cmd2, .command_count = 2, .is_background = 0};

  int saved_stderr = dup(STDOUT_FILENO);
  int dev_null = open("/dev/null", O_WRONLY);

  if (dev_null >= 0) {
    dup2(dev_null, STDOUT_FILENO);
    close(dev_null);
  }

  int status = cshell_execute_pipeline(&pipe);

  if (saved_stderr >= 0) {
    dup2(saved_stderr, STDOUT_FILENO);
    close(saved_stderr);
  }

  ASSERT_INT_EQ(status, 0,
                "Basic pipeline (\"echo systems_programming | grep\") should "
                "return with status 0");
}

static void test_pipeline_terminal_status(void) {
  Command cmd2_a = {.args = {"false", NULL}, .arg_count = 1, .next = NULL};
  Command cmd1_a = {.args = {"true", NULL}, .arg_count = 1, .next = &cmd2_a};
  Pipeline pipe_a = {
      .head = &cmd1_a, .tail = &cmd2_a, .command_count = 2, .is_background = 0};

  ASSERT_INT_EQ(
      cshell_execute_pipeline(&pipe_a), 1,
      "Pipeline (\"true | false\") must return terminal exit status 1");

  Command cmd2_b = {.args = {"true", NULL}, .arg_count = 1, .next = NULL};
  Command cmd1_b = {.args = {"false", NULL}, .arg_count = 1, .next = &cmd2_b};
  Pipeline pipe_b = {
      .head = &cmd1_b, .tail = &cmd2_b, .command_count = 2, .is_background = 0};

  ASSERT_INT_EQ(
      cshell_execute_pipeline(&pipe_b), 0,
      "Pipeline (\"false | true\") must return terminal exit status 0");
}

static void test_pipeline_with_redirection(void) {
  const char *out_file = "_test_pipeline_out_92837554.txt";
  unlink(out_file);

  Command cmd3 = {.args = {"cat", NULL},
                  .arg_count = 1,
                  .output_redirect = (char *)out_file,
                  .next = NULL};
  Command cmd2 = {.args = {"cat", NULL}, .arg_count = 1, .next = &cmd3};
  Command cmd1 = {
      .args = {"echo", "pipeline_test", NULL}, .arg_count = 2, .next = &cmd2};
  Pipeline pipe = {
      .head = &cmd1, .tail = &cmd3, .command_count = 3, .is_background = 0};

  ASSERT_INT_EQ(cshell_execute_pipeline(&pipe), 0,
                "Pipeline with trailing redirection should return 0");

  FILE *f = fopen(out_file, "r");
  ASSERT_PTR_NOT_NULL(
      f, "Pipeline output redirection file should have been created");

  char buffer[64];
  if (f != NULL) {
    char *fgets_res = fgets(buffer, sizeof(buffer), f);
    fclose(f);

    ASSERT_PTR_NOT_NULL(fgets_res,
                        "Should be able to read data from pipeline target");
    ASSERT_STR_EQ(buffer, "pipeline_test\n",
                  "File content must match original upstream payload");
  }

  unlink(out_file);
}

static void test_background_pipeline(void) {
  cshell_init_signals();

  Command cmd = {.args = {"sleep", "0.1", NULL}, .arg_count = 2, .next = NULL};
  Pipeline pipe = {
      .head = &cmd, .tail = &cmd, .command_count = 1, .is_background = 1};

  int saved_stdout = dup(STDOUT_FILENO);
  int dev_null = open("/dev/null", O_WRONLY);
  if (dev_null >= 0) {
    dup2(dev_null, STDOUT_FILENO);
    close(dev_null);
  }

  int status = cshell_execute_pipeline(&pipe);

  if (saved_stdout >= 0) {
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);
  }

  ASSERT_INT_EQ(status, 0,
                "Background pipeline must return 0 immediately to parent");
  usleep(150000);
}

static void test_pipeline_execution(void) {
  test_two_stage_pipeline();
  test_pipeline_terminal_status();
  test_pipeline_with_redirection();
  test_background_pipeline();
}

static void test_export_resolution(void) {
  Command cmd = {.args = {"export", "MY_VAR_19493857=value", NULL},
                 .arg_count = 2};
  ASSERT_INT_EQ(cshell_resolve_command(&cmd), CMD_TYPE_EXPORT,
                "Export statement should resolve to CMD_TYPE_EXPORT");
}

static void test_export_execution_parent_context(void) {
  char kv_arg[] = "PARENT_VAR_59382712=isolated_test";
  Command cmd = {.args = {"export", kv_arg, NULL}, .arg_count = 2};

  int status = cshell_execute_command(&cmd);
  ASSERT_INT_EQ(status, 0,
                "Executing isolated export in parent should return 0");

  char *val = getenv("PARENT_VAR_59382712");
  ASSERT_PTR_NOT_NULL(
      val, "Environment variable should be visible in parent context");
  if (val != NULL) {
    ASSERT_STR_EQ(val, "isolated_test",
                  "Environment variable value must match assignment string");
  }
  unsetenv("PARENT_VAR_59382712");
}

static void test_export_subshell_isolation(void) {
  char kv_arg[] = "SUBSHELL_VAR_12485492=should_not_leak";
  Command cmd2 = {.args = {"cat", NULL}, .arg_count = 1, .next = NULL};
  Command cmd1 = {
      .args = {"export", kv_arg, NULL}, .arg_count = 2, .next = &cmd2};

  Pipeline pipe = {
      .head = &cmd1, .tail = &cmd2, .command_count = 2, .is_background = 0};

  int saved_stdout = dup(STDOUT_FILENO);
  int dev_null = open("/dev/null", O_WRONLY);
  if (dev_null >= 0) {
    dup2(dev_null, STDOUT_FILENO);
    close(dev_null);
  }

  int status = cshell_execute_pipeline(&pipe);

  if (saved_stdout >= 0) {
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);
  }

  ASSERT_INT_EQ(
      status, 0,
      "Pipeline containing export built-in should complete with status 0");

  char *val = getenv("SUBSHELL_VAR_12485492");
  ASSERT_PTR_NULL(val, "Variables mutated inside a pipeline subshell must not "
                       "leak into the parent shell environment");
}

void test_export(void) {
  test_export_resolution();
  test_export_execution_parent_context();
  test_export_subshell_isolation();
}

int main(void) {
  printf("\nRunning: %s\n", __FILE__);

  test_all_args();
  test_pipeline_execution();
  test_export();

  test_summary();
  return tests_failed > 0 ? 1 : 0;
}
