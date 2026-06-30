#define _GNU_SOURCE

#include "cshell/history.h"
#include "cshell/linereader.h"
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

static void test_linereader_basic(void) {
  char buf[HISTORY_MAX_LINE_LEN];
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
  char buf[HISTORY_MAX_LINE_LEN];
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
  char buf_1[HISTORY_MAX_LINE_LEN];
  const char input_up[] = "\033[A\n";
  feed_mock_input(input_up, strlen(input_up));
  cshell_read_line(buf_1, sizeof(buf_1));
  // Up, Up, Down, Enter: Up to entry 1, Up to entry 2, Down back to entry 1
  char buf_2[HISTORY_MAX_LINE_LEN];
  const char input_sequence[] = "\033[A\033[A\033[B\n";
  feed_mock_input(input_sequence, strlen(input_sequence));
  cshell_read_line(buf_2, sizeof(buf_2));

  unmute_output(saved_stdout);

  ASSERT_STR_EQ(buf_1, "gcc main.c -o cshell",
                "Single Up-Arrow sequence must retrieve newest history item");
  ASSERT_STR_EQ(buf_2, "gcc main.c -o cshell",
                "Up-Up-Down sequence must traverse linear states correctly");
}

int main(void) {
  printf("\nRunning: %s\n", __FILE__);

  test_linereader_basic();
  test_linereader_backspace();
  test_linereader_history_navigation();

  test_summary();
  return tests_failed > 0 ? 1 : 0;
}