#include "cshell/executor.h"
#include "cshell/test_framework.h"

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

static void test_external_arg(void) {
  Command cmd = {.args = {"ls", "-l", NULL}, .arg_count = 2};
  ASSERT_INT_EQ(cshell_resolve_command(&cmd), CMD_TYPE_EXTERNAL,
                "External argument (\"ls -l\") should resolve correctly");
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
