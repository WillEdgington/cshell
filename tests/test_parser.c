#include "cshell/parser.h"
#include "cshell/test_framework.h"
#include <string.h>

static void test_simple_command(void) {
  Command cmd;
  memset(&cmd, 0, sizeof(Command));
  char line[] = "ls";

  ASSERT_INT_EQ(cshell_parse_line(line, &cmd), 0,
                "Simple command status should not error (return 0)");
  ASSERT_INT_EQ(cmd.arg_count, 1,
                "arg_count should correctly identify number of arguments in "
                "line (ls is 1)");
  ASSERT_STR_EQ(cmd.args[0], "ls", "args[0] should match command name (ls)");
  ASSERT_PTR_NULL(cmd.args[1], "args[arg_count] must be NULL-terminated");
  ASSERT_PTR_NULL(cmd.input_redirect,
                  "input_redirect should be NULL (for line of \"ls\")");
  ASSERT_PTR_NULL(cmd.output_redirect, "output_redirect should be NULL");
  ASSERT_INT_EQ(cmd.is_background, 0,
                "is_background should be 0 (for line of \"ls\")");
}

static void test_args_with_whitespace(void) {
  Command cmd;
  memset(&cmd, 0, sizeof(Command));
  char line[] = "   grep    -i   \"hello world\"   ";

  ASSERT_INT_EQ(cshell_parse_line(line, &cmd), 0,
                "Whitespace padding status should not error (return 0)");
  ASSERT_INT_EQ(
      cmd.arg_count, 3,
      "Padding command arg_count should be 3 (grep, -l, \"hello world\")");
  ASSERT_STR_EQ(cmd.args[0], "grep", "args[0] should catch first word (grep)");
  ASSERT_STR_EQ(cmd.args[1], "-i", "args[1] should catch flag (-i)");
  ASSERT_STR_EQ(
      cmd.args[2], "\"hello world\"",
      "args[2] should keep internal strings intact (\"hello world\")");
  ASSERT_PTR_NULL(
      cmd.args[3],
      "args[arg_length] must be NULL-terminated (args with whitespace test)");
}

static void test_output_redirection(void) {
  Command cmd;
  memset(&cmd, 0, sizeof(Command));
  char line[] = "ls -la > output.txt";

  ASSERT_INT_EQ(cshell_parse_line(line, &cmd), 0,
                "Output redirect status should not error (return 0)");
  ASSERT_INT_EQ(cmd.arg_count, 2,
                "Redirection tokens must be stripped from arg_count");
  ASSERT_PTR_NULL(
      cmd.args[2],
      "args[arg_count] must be NULL-terminated (output redirection test)");
  ASSERT_STR_EQ(cmd.output_redirect, "output.txt",
                "output_redirect target extracted correctly");
  ASSERT_PTR_NULL(cmd.input_redirect, "input_redirect should remain NULL");
}

static void test_input_redirection(void) {
  Command cmd;
  memset(&cmd, 0, sizeof(Command));
  char line[] = "sort < names.txt";

  ASSERT_INT_EQ(cshell_parse_line(line, &cmd), 0,
                "Input redirect status should not error (return 0)");
  ASSERT_INT_EQ(cmd.arg_count, 1,
                "args array should only hold command context");
  ASSERT_STR_EQ(cmd.input_redirect, "names.txt",
                "input_redirect target extracted correctly");
  ASSERT_PTR_NULL(cmd.output_redirect, "output_redirect should remain NULL");
}

static void test_background_flag(void) {
  Command cmd;
  memset(&cmd, 0, sizeof(Command));
  char line[] = "sleep 10 &";

  ASSERT_INT_EQ(cshell_parse_line(line, &cmd), 0,
                "Background parsing status should not error (return 0)");
  ASSERT_INT_EQ(cmd.arg_count, 2,
                "Background token must be stripped from arg_count");
  ASSERT_INT_EQ(cmd.is_background, 1, "is_background flag must execute true");
}

static void test_combined_operators(void) {
  Command cmd;
  memset(&cmd, 0, sizeof(Command));
  char line[] = "cat < input.txt > output.txt &";

  ASSERT_INT_EQ(cshell_parse_line(line, &cmd), 0,
                "Complex layout parsing status should not error (return 0)");
  ASSERT_INT_EQ(cmd.arg_count, 1,
                "All system operators stripped out of execution array");
  ASSERT_STR_EQ(cmd.input_redirect, "input.txt",
                "Input target matches layout file");
  ASSERT_STR_EQ(cmd.output_redirect, "output.txt",
                "Output target matches layout file");
  ASSERT_INT_EQ(cmd.is_background, 1, "Background execution state validated");
}

static void test_syntax_errors(void) {
  Command cmd;

  char err_line1[] = "ls -la >";
  memset(&cmd, 0, sizeof(Command));
  ASSERT_INT_EQ(
      cshell_parse_line(err_line1, &cmd), -1,
      "Trailing redirection character without file must return error (-1)");

  char err_line2[] = "sort < &";
  ASSERT_INT_EQ(cshell_parse_line(err_line2, &cmd), -1,
                "Operator collision without valid target filename must return "
                "error (-1)");
}

static void test_empty_input(void) {
  Command cmd;
  memset(&cmd, 0, sizeof(Command));
  char line[] = "      \n  ";

  ASSERT_INT_EQ(
      cshell_parse_line(line, &cmd), 0,
      "Empty line structural parsing status should not error (return 0)");
  ASSERT_INT_EQ(cmd.arg_count, 0, "Empty line must yield 0 arg_count");
  ASSERT_PTR_NULL(cmd.args[0],
                  "args[arg_count] must be NULL-terminated (empty input test)");
}

int main(void) {
  printf("\nRunning: %s\n", __FILE__);

  test_simple_command();
  test_args_with_whitespace();
  test_output_redirection();
  test_input_redirection();
  test_background_flag();
  test_combined_operators();
  test_syntax_errors();
  test_empty_input();

  test_summary();
  return tests_failed > 0 ? 1 : 0;
}
