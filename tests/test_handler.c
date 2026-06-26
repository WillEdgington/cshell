#define _GNU_SOURCE

#include "cshell/handler.h"
#include "cshell/executor.h"
#include "cshell/parser.h"
#include "cshell/runtime.h"
#include "cshell/test_framework.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void test_handler_logical_and(void) {
  Command cmd_t1 = {.args = {"true", NULL}, .arg_count = 1, .next = NULL};
  Command cmd_t2 = {.args = {"true", NULL}, .arg_count = 1, .next = NULL};

  Pipeline p2 = {.head = &cmd_t2, .tail = &cmd_t2, .command_count = 1, .next = NULL, .next_op = LOGICAL_OP_NONE};
  Pipeline p1 = {.head = &cmd_t1, .tail = &cmd_t1, .command_count = 1, .next = &p2, .next_op = LOGICAL_OP_AND};

  shell_r.last_exit_status = 0;
  int status = cshell_handle_execution(&p1);
  ASSERT_INT_EQ(status, 0, "true && true must execute both links and return 0");

  Command cmd_f1 = {.args = {"false", NULL}, .arg_count = 1, .next = NULL};
  Command cmd_t3 = {.args = {"true", NULL}, .arg_count = 1, .next = NULL};

  Pipeline p4 = {.head = &cmd_t3, .tail = &cmd_t3, .command_count = 1, .next = NULL, .next_op = LOGICAL_OP_NONE};
  Pipeline p3 = {.head = &cmd_f1, .tail = &cmd_f1, .command_count = 1, .next = &p4, .next_op = LOGICAL_OP_AND};

  shell_r.last_exit_status = 0;
  status = cshell_handle_execution(&p3);
  ASSERT_INT_EQ(status, 1, "false && true must short-circuit and retain status 1");
}

static void test_handler_logical_or(void) {
  Command cmd_t1 = {.args = {"true", NULL}, .arg_count = 1, .next = NULL};
  Command cmd_f1 = {.args = {"false", NULL}, .arg_count = 1, .next = NULL};

  Pipeline p2 = {.head = &cmd_f1, .tail = &cmd_f1, .command_count = 1, .next = NULL, .next_op = LOGICAL_OP_NONE};
  Pipeline p1 = {.head = &cmd_t1, .tail = &cmd_t1, .command_count = 1, .next = &p2, .next_op = LOGICAL_OP_OR};

  shell_r.last_exit_status = 0;
  int status = cshell_handle_execution(&p1);
  ASSERT_INT_EQ(status, 0, "true || false must short-circuit and retain status 0");

  Command cmd_f2 = {.args = {"false", NULL}, .arg_count = 1, .next = NULL};
  Command cmd_t2 = {.args = {"true", NULL}, .arg_count = 1, .next = NULL};

  Pipeline p4 = {.head = &cmd_t2, .tail = &cmd_t2, .command_count = 1, .next = NULL, .next_op = LOGICAL_OP_NONE};
  Pipeline p3 = {.head = &cmd_f2, .tail = &cmd_f2, .command_count = 1, .next = &p4, .next_op = LOGICAL_OP_OR};

  shell_r.last_exit_status = 0;
  status = cshell_handle_execution(&p3);
  ASSERT_INT_EQ(status, 0, "false || true must execute the fallback link and return 0");
}

static void test_handler_multi_link_chains(void) {
  Command cmd_f1 = {.args = {"false", NULL}, .arg_count = 1, .next = NULL};
  Command cmd_f2 = {.args = {"false", NULL}, .arg_count = 1, .next = NULL};
  Command cmd_t1 = {.args = {"true", NULL}, .arg_count = 1, .next = NULL};

  Pipeline p3 = {.head = &cmd_t1, .tail = &cmd_t1, .command_count = 1, .next = NULL, .next_op = LOGICAL_OP_NONE};
  Pipeline p2 = {.head = &cmd_f2, .tail = &cmd_f2, .command_count = 1, .next = &p3, .next_op = LOGICAL_OP_OR};
  Pipeline p1 = {.head = &cmd_f1, .tail = &cmd_f1, .command_count = 1, .next = &p2, .next_op = LOGICAL_OP_AND};

  shell_r.last_exit_status = 0;
  int status = cshell_handle_execution(&p1);
  ASSERT_INT_EQ(status, 0, "false && false || true must fully recover from skipped nodes and evaluate trailing operators");
}

int main(void) {
  printf("\nRunning: %s\n", __FILE__);

  test_handler_logical_and();
  test_handler_logical_or();
  test_handler_multi_link_chains();

  test_summary();
  return tests_failed > 0 ? 1 : 0;
}