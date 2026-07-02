#define _GNU_SOURCE

#include "cshell/history.h"
#include "cshell/linereader.h"
#include "cshell/runtime.h"
#include "cshell/test_framework.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void feed_mock_input(const char *bytes, size_t len) {
  int pipefds[2];
  if (pipe(pipefds) < 0) {
    perror("  [INFO] pipe generation failed");
    return;
  }

  write(pipefds[1], bytes, len);
  close(pipefds[1]);
  dup2(pipefds[0], STDIN_FILENO);
  close(pipefds[0]);
}

static int mute_output(void) {
  int saved_stdout = dup(STDOUT_FILENO);
  int dev_null = open("/dev/null", O_WRONLY);
  if (dev_null >= 0) {
    dup2(dev_null, STDOUT_FILENO);
    close(dev_null);
  }
  return saved_stdout;
}

static void unmute_output(int saved_stdout) {
  if (saved_stdout >= 0) {
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);
  }
}

static void setup_mock_file(const char *path) {
  FILE *f = fopen(path, "w");
  if (f != NULL)
    fclose(f);
}

static void teardown_mock_file(const char *path) { unlink(path); }

static void test_linereader_basic(void) {
  char buf[MAX_LINE_LEN];
  const char *input = "echo engineering\n";
  feed_mock_input(input, strlen(input));

  int saved_stdout = mute_output();
  ssize_t ret = cshell_read_line(buf, sizeof(buf));
  unmute_output(saved_stdout);

  ASSERT_INT_EQ((int)ret, (int)strlen(input) - 1,
                "Should return exact string size excluding newline");
  ASSERT_STR_EQ(buf, "echo engineering",
                "Buffer should accurately capture printable ASCII text");
}

static void test_linereader_backspace(void) {
  char buf[MAX_LINE_LEN];
  // Equivalent to user typing: 'a', 'b', 'c', BACKSPACE, 'd', ENTER
  const char input[] = {'a', 'b', 'c', 0x7F, 'd', '\n'};
  feed_mock_input(input, sizeof(input));

  int saved_stdout = mute_output();
  ssize_t ret = cshell_read_line(buf, sizeof(buf));
  unmute_output(saved_stdout);

  ASSERT_INT_EQ((int)ret, 3,
                "Length calculation must reflect the destructive edit drop");
  ASSERT_STR_EQ(buf, "abd",
                "Buffer array must apply destructive backspaces cleanly");
}

static void test_linereader_history_navigation(void) {
  cshell_history_init();
  cshell_history_add("docker ps");
  cshell_history_add("gcc main.c -o cshell");

  int saved_stdout = mute_output();

  // Up Arrow (\033[A) then Enter (\n): target the newest history entry
  char buf_1[MAX_LINE_LEN];
  const char input_up[] = "\033[A\n";
  feed_mock_input(input_up, strlen(input_up));
  cshell_read_line(buf_1, sizeof(buf_1));
  // Up, Up, Down, Enter: Up to entry 1, Up to entry 2, Down back to entry 1
  char buf_2[MAX_LINE_LEN];
  const char input_sequence[] = "\033[A\033[A\033[B\n";
  feed_mock_input(input_sequence, strlen(input_sequence));
  cshell_read_line(buf_2, sizeof(buf_2));

  unmute_output(saved_stdout);

  ASSERT_STR_EQ(buf_1, "gcc main.c -o cshell",
                "Single Up-Arrow sequence must retrieve newest history item");
  ASSERT_STR_EQ(buf_2, "gcc main.c -o cshell",
                "Up-Up-Down sequence must traverse linear states correctly");
}

static void test_linereader_horizontal_cursor_navigation(void) {
  char buf[MAX_LINE_LEN];
  int saved_stdout;
  ssize_t ret;

  // Equivalent to: Type "abc", Left, Left, Type 'X', Enter -> "aXbc"
  const char input_insert[] = "abc\033[D\033[Dx\n";
  feed_mock_input(input_insert, strlen(input_insert));

  saved_stdout = mute_output();
  ret = cshell_read_line(buf, sizeof(buf));
  unmute_output(saved_stdout);

  ASSERT_INT_EQ((int)ret, 4, "Length should increase after mid-line insertion");
  ASSERT_STR_EQ(
      buf, "axbc",
      "Buffer must shift characters rightward to allow mid-line insertion");

  // Equivalent to: Type "abc", Left, Backspace, Enter -> "ac"
  const char input_delete[] = {'a', 'b',  'c',  '\033', '[',
                               'D', 0x7F, '\n', '\0'};
  feed_mock_input(input_delete, strlen(input_delete));

  saved_stdout = mute_output();
  ret = cshell_read_line(buf, sizeof(buf));
  unmute_output(saved_stdout);

  ASSERT_INT_EQ((int)ret, 2, "Length should decrease after mid-line deletion");
  ASSERT_STR_EQ(buf, "ac",
                "Buffer must shift characters leftward to bridge a mid-line "
                "deletion gap");

  // Equivalent to: Left (at start), Right (at start), Type "xyz", Right, Right,
  // Enter -> "xyz"
  const char input_boundary[] = "\033[D\033[Cxyz\033[C\033[C\n";
  feed_mock_input(input_boundary, strlen(input_boundary));

  saved_stdout = mute_output();
  ret = cshell_read_line(buf, sizeof(buf));
  unmute_output(saved_stdout);

  ASSERT_INT_EQ(
      (int)ret, 3,
      "Length should remain unaffected by out-of-bound arrow sequences");
  ASSERT_STR_EQ(buf, "xyz",
                "Clamping limits must prevent cursor underflows or overflows");
}

static void test_linereader_tab_completion(void) {
  char buf[MAX_LINE_LEN];
  int saved_stdout;
  ssize_t ret;
  char *filename = "_test_f_77394821";

  setup_mock_file(filename);

  // Type prefix, Tab, Enter
  const char input_stream[] = "_test_f_77\t\n";
  feed_mock_input(input_stream, strlen(input_stream));

  saved_stdout = mute_output();
  ret = cshell_read_line(buf, sizeof(buf));
  unmute_output(saved_stdout);

  teardown_mock_file(filename);

  ASSERT_STR_EQ(
      buf, "_test_f_77394821 ",
      "State machine tab expansion should expand unique transient file tokens");
  ASSERT_INT_EQ((int)ret, 17,
                "Returned length must match the expanded string size");
}

int main(void) {
  printf("\nRunning: %s\n", __FILE__);

  test_linereader_basic();
  test_linereader_backspace();
  test_linereader_history_navigation();
  test_linereader_horizontal_cursor_navigation();
  test_linereader_tab_completion();

  test_summary();
  return tests_failed > 0 ? 1 : 0;
}
