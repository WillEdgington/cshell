#include "clib/arena.h"
#include "cshell/parser.h"
#include "cshell/test_framework.h"
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

static void test_simple_command(Arena *a) {
  Pipeline pipe;
  pipeline_init(&pipe, a);

  char line[] = "ls";

  ASSERT_INT_EQ(cshell_parse_line(line, &pipe), 0,
                "Simple command status should not error (return 0)");
  ASSERT_INT_EQ(pipe.command_count, 1,
                "Command count should equal 1 (no \"|\")");

  Command *cmd = pipe.head;
  ASSERT_INT_EQ(cmd->arg_count, 1,
                "arg_count should correctly identify number of arguments in "
                "line (ls is 1)");
  ASSERT_STR_EQ(cmd->args[0], "ls", "args[0] should match command name (ls)");
  ASSERT_PTR_NULL(cmd->args[1], "args[arg_count] must be NULL-terminated");
  ASSERT_PTR_NULL(cmd->input_redirect,
                  "input_redirect should be NULL (for line of \"ls\")");
  ASSERT_PTR_NULL(cmd->output_redirect, "output_redirect should be NULL");
  ASSERT_INT_EQ(pipe.is_background, 0,
                "is_background should be 0 (for line of \"ls\")");
}

static void test_args_with_whitespace(Arena *a) {
  Pipeline pipe;
  pipeline_init(&pipe, a);

  char line[] = "   grep    -i   \"hello world\"   ";

  ASSERT_INT_EQ(cshell_parse_line(line, &pipe), 0,
                "Whitespace padding status should not error (return 0)");

  Command *cmd = pipe.head;
  ASSERT_INT_EQ(
      cmd->arg_count, 3,
      "Padding command arg_count should be 3 (grep, -l, \"hello world\")");
  ASSERT_STR_EQ(cmd->args[0], "grep", "args[0] should catch first word (grep)");
  ASSERT_STR_EQ(cmd->args[1], "-i", "args[1] should catch flag (-i)");
  ASSERT_STR_EQ(cmd->args[2], "hello world",
                "args[2] should keep internal strings intact and strip any "
                "quotation wrappers");
  ASSERT_PTR_NULL(
      cmd->args[3],
      "args[arg_length] must be NULL-terminated (args with whitespace test)");
}

static void test_output_redirection(Arena *a) {
  Pipeline pipe;
  pipeline_init(&pipe, a);

  char line[] = "ls -la > output.txt";

  ASSERT_INT_EQ(cshell_parse_line(line, &pipe), 0,
                "Output redirect status should not error (return 0)");

  Command *cmd = pipe.head;
  ASSERT_INT_EQ(cmd->arg_count, 2,
                "Redirection tokens must be stripped from arg_count");
  ASSERT_PTR_NULL(
      cmd->args[2],
      "args[arg_count] must be NULL-terminated (output redirection test)");
  ASSERT_STR_EQ(cmd->output_redirect, "output.txt",
                "output_redirect target extracted correctly");
  ASSERT_PTR_NULL(cmd->input_redirect, "input_redirect should remain NULL");
}

static void test_input_redirection(Arena *a) {
  Pipeline pipe;
  pipeline_init(&pipe, a);

  char line[] = "sort < names.txt";

  ASSERT_INT_EQ(cshell_parse_line(line, &pipe), 0,
                "Input redirect status should not error (return 0)");

  Command *cmd = pipe.head;
  ASSERT_INT_EQ(cmd->arg_count, 1,
                "args array should only hold command context");
  ASSERT_STR_EQ(cmd->input_redirect, "names.txt",
                "input_redirect target extracted correctly");
  ASSERT_PTR_NULL(cmd->output_redirect, "output_redirect should remain NULL");
}

static void test_background_flag(Arena *a) {
  Pipeline pipe;
  pipeline_init(&pipe, a);

  char line[] = "sleep 10 &";

  ASSERT_INT_EQ(cshell_parse_line(line, &pipe), 0,
                "Background parsing status should not error (return 0)");

  Command *cmd = pipe.head;
  ASSERT_INT_EQ(cmd->arg_count, 2,
                "Background token must be stripped from arg_count");
  ASSERT_INT_EQ(pipe.is_background, 1, "is_background flag must execute true");
}

static void test_combined_operators(Arena *a) {
  Pipeline pipe;
  pipeline_init(&pipe, a);

  char line[] = "cat < input.txt > output.txt &";

  ASSERT_INT_EQ(cshell_parse_line(line, &pipe), 0,
                "Complex layout parsing status should not error (return 0)");

  Command *cmd = pipe.head;
  ASSERT_INT_EQ(cmd->arg_count, 1,
                "All system operators stripped out of execution array");
  ASSERT_STR_EQ(cmd->input_redirect, "input.txt",
                "Input target matches layout file");
  ASSERT_STR_EQ(cmd->output_redirect, "output.txt",
                "Output target matches layout file");
  ASSERT_INT_EQ(pipe.is_background, 1, "Background execution state validated");
}

static void test_standard_pipeline(Arena *a) {
  Pipeline pipe;
  pipeline_init(&pipe, a);

  char line[] = "cat /etc/passwd | grep root | wc -l";

  ASSERT_INT_EQ(cshell_parse_line(line, &pipe), 0,
                "Multi-stage pipeline parsing validation");
  ASSERT_INT_EQ(pipe.command_count, 3,
                "Identifies exactly three command stages");

  Command *cmd1 = pipe.head;
  ASSERT_STR_EQ(cmd1->args[0], "cat", "Stage 1 parsing resolves cleanly");
  ASSERT_STR_EQ(cmd1->args[1], "/etc/passwd", "Stage 1 flags matched");

  Command *cmd2 = cmd1->next;
  ASSERT_PTR_NOT_NULL(cmd2, "Subsequent link element exists");
  ASSERT_STR_EQ(cmd2->args[0], "grep", "Stage 2 validation maps accurately");
  ASSERT_STR_EQ(cmd2->args[1], "root", "Stage 2 parameter mapping verified");

  Command *cmd3 = cmd2->next;
  ASSERT_PTR_NOT_NULL(cmd3, "Terminal link node tracked accurately");
  ASSERT_STR_EQ(cmd3->args[0], "wc", "Stage 3 parsing resolves correctly");
  ASSERT_PTR_NULL(cmd3->next, "Pipeline list capped correctly");
  ASSERT(cmd3 == pipe.tail, "Final command is assigned to pipeline tail");
}

static void test_syntax_errors(Arena *a) {
  Pipeline pipe;
  pipeline_init(&pipe, a);

  int saved_stderr = dup(STDERR_FILENO);
  int dev_null = open("/dev/null", O_WRONLY);

  if (dev_null >= 0) {
    dup2(dev_null, STDERR_FILENO);
    close(dev_null);
  }

  char err_line1[] = "ls -la >";
  pipeline_init(&pipe, a);
  int err_status1 = cshell_parse_line(err_line1, &pipe);

  char err_line2[] = "sort < &";
  pipeline_init(&pipe, a);
  int err_status2 = cshell_parse_line(err_line2, &pipe);

  char err_line3[] = "| grep root";
  pipeline_init(&pipe, a);
  int err_status3 = cshell_parse_line(err_line3, &pipe);

  char err_line4[] = "cat history.log |";
  pipeline_init(&pipe, a);
  int err_status4 = cshell_parse_line(err_line4, &pipe);

  char err_line5[] = "echo & hello";
  pipeline_init(&pipe, a);
  int err_status5 = cshell_parse_line(err_line5, &pipe);

  if (saved_stderr >= 0) {
    dup2(saved_stderr, STDERR_FILENO);
    close(saved_stderr);
  }

  ASSERT_INT_EQ(
      err_status1, -1,
      "Trailing redirection character without file must return error (-1)");

  ASSERT_INT_EQ(
    err_status2, -1,
    "Operator collision without valid target filename must return "
    "error (-1)");

  ASSERT_INT_EQ(err_status3, -1,
                "Leading pipe marker with missing upstream process must error");

  ASSERT_INT_EQ(err_status4, -1,
                "Dangling pipe execution statement must error");

  ASSERT_INT_EQ(err_status5, -1,
                "non-trailing '&' in line must error");
}

static void test_empty_input(Arena *a) {
  Pipeline pipe;
  pipeline_init(&pipe, a);

  char line[] = "      \n  ";

  ASSERT_INT_EQ(
      cshell_parse_line(line, &pipe), 0,
      "Empty line structural parsing status should not error (return 0)");
  ASSERT_INT_EQ(pipe.command_count, 0,
                "Empty line yields 0 active execution instances");
  ASSERT_PTR_NULL(pipe.head->args[0],
                  "Main interface parameter tracking remains unbound");
}

static void test_escaped_tokens_in_quoted_args(Arena *a) {
  Pipeline pipe;

  char escaped_q[] = "echo \"\\\"hello\\\", I said\"";
  pipeline_init(&pipe, a);
  cshell_parse_line(escaped_q, &pipe);
  ASSERT_STR_EQ(
      pipe.head->args[1], "\"hello\", I said",
      "Escaped quotes in quoted args should be included in parsed arg");

  char escaped_bslash[] = "echo \"C:\\\\User\\\\Windows\\\\\"";
  pipeline_init(&pipe, a);
  cshell_parse_line(escaped_bslash, &pipe);
  ASSERT_STR_EQ(
      pipe.head->args[1], "C:\\User\\Windows\\",
      "Escaped back-slash in quoted args should be included in parsed arg");
}

static void test_input_with_comment(Arena *a) {
  Pipeline pipe;

  char spaced_comment[] = "echo hello # this is a comment";
  pipeline_init(&pipe, a);
  cshell_parse_line(spaced_comment, &pipe);
  ASSERT_INT_EQ(pipe.head->arg_count, 2, "Parser should only parse uncommented args");
  ASSERT((
    strcmp("echo", pipe.head->args[0]) == 0 &&
    strcmp("hello", pipe.head->args[1]) == 0 &&
    pipe.head->args[2] == NULL
  ),
    "Parser should assign correct args for input with comments"
  );

  char messy_comment[] = "echo hello #use the # to make a comment##\nworld ##comment\r";
  pipeline_init(&pipe, a);
  cshell_parse_line(messy_comment, &pipe);
  ASSERT_INT_EQ(pipe.head->arg_count, 3, "Parser should only parse uncommented args for multi-line commands");
  ASSERT((
    strcmp("echo", pipe.head->args[0]) == 0 &&
    strcmp("hello", pipe.head->args[1]) == 0 &&
    strcmp("world", pipe.head->args[2]) == 0 &&
    pipe.head->args[3] == NULL
  ),
    "Parser should assign correct args for multi-line input with comments"
  );
}

static void test_input_with_logical_ops(Arena *a) {
  Pipeline pipe;

  char input_or[] = "cat text.txt || echo backup info";
  pipeline_init(&pipe, a);
  ASSERT_INT_EQ(cshell_parse_line(input_or, &pipe), 0, "Command status including logical operator should not error");
  ASSERT(
    pipe.next != NULL &&
    pipe.next_op == LOGICAL_OP_OR,
    "Logical OR ('||') should correctly create new connected pipeline and store LogicalOp correctly"
  );
  ASSERT((
    strcmp(pipe.tail->args[1], "text.txt") == 0 &&
    strcmp(pipe.next->head->args[0], "echo") == 0 &&
    pipe.head->next == NULL &&
    pipe.head->arg_count == 2 &&
    pipe.next->head->next == NULL &&
    pipe.next->head->arg_count == 3
  ),
    "Commands either side of a logical operator should correctly split into their pipeline"
  );

  char input_and[] = "cat text.txt && echo see this on success";
  pipeline_init(&pipe, a);
  cshell_parse_line(input_and, &pipe);
  ASSERT(
    pipe.next != NULL &&
    pipe.next_op == LOGICAL_OP_AND,
    "Logical OR ('&&') should correctly create new connected pipeline and store LogicalOp correctly"
  );

  int saved_stderr = dup(STDERR_FILENO);
  int dev_null = open("/dev/null", O_WRONLY);

  if (dev_null >= 0) {
    dup2(dev_null, STDERR_FILENO);
    close(dev_null);
  }

  char dangling_op[] = "cat text.txt &&";
  pipeline_init(&pipe, a);
  int dangling_status = cshell_parse_line(dangling_op, &pipe);

  char consecutive_op[] = "cat text.txt && || echo terrible input";
  pipeline_init(&pipe, a);
  int consecutive_status = cshell_parse_line(consecutive_op, &pipe);

  if (saved_stderr >= 0) {
    dup2(saved_stderr, STDERR_FILENO);
    close(saved_stderr);
  }

  ASSERT_INT_EQ(dangling_status, -1, "Command status including dangling operator should error");
  ASSERT_INT_EQ(consecutive_status, -1, "Command status including consecutive operators should error");
}

int main(void) {
  printf("\nRunning: %s\n", __FILE__);

  Arena a;
  arena_init(&a, 4096);

  test_simple_command(&a);
  test_args_with_whitespace(&a);
  test_output_redirection(&a);
  test_input_redirection(&a);
  test_background_flag(&a);
  test_combined_operators(&a);
  test_standard_pipeline(&a);
  test_syntax_errors(&a);
  test_empty_input(&a);
  test_escaped_tokens_in_quoted_args(&a);
  test_input_with_comment(&a);
  test_input_with_logical_ops(&a);

  arena_free(&a);

  test_summary();
  return tests_failed > 0 ? 1 : 0;
}
