#define _GNU_SOURCE

#include "cshell/terminal.h"
#include "cshell/test_framework.h"
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

static void test_terminal_transitions(void) {
  int init_res = cshell_terminal_init();

  // redirect Non-TTY environments (e.g. CI runners or input redirection)
  if (!isatty(STDIN_FILENO)) {
    ASSERT_INT_EQ(init_res, -1,
                  "Initialisation must fail gracefully if stdin is not a TTY");
    printf("  [Info] Non-TTY environment detected. Skipping active mode "
           "transition tests.\n");
    return;
  }

  ASSERT_INT_EQ(init_res, 0, "Initialisation must return 0 (no error)");

  cshell_terminal_mode_raw();

  struct termios current_settings;
  int getattr_res = tcgetattr(STDIN_FILENO, &current_settings);
  ASSERT_INT_EQ(getattr_res, 0,
                "Kernel must allow querying current terminal states");

  int icanon_active = (current_settings.c_lflag & ICANON) ? 1 : 0;
  int echo_active = (current_settings.c_lflag & ECHO) ? 1 : 0;
  ASSERT_INT_EQ(icanon_active, 0,
                "ICANON bit must be completely disabled in raw mode");
  ASSERT_INT_EQ(echo_active, 0,
                "ECHO bit must be completely disabled in raw mode");

  cshell_terminal_mode_canonical();
  getattr_res = tcgetattr(STDIN_FILENO, &current_settings);
  ASSERT_INT_EQ(getattr_res, 0,
                "Kernel must allow querying state after restoration");

  icanon_active = (current_settings.c_lflag & ICANON) ? 1 : 0;
  echo_active = (current_settings.c_lflag & ECHO) ? 1 : 0;

  ASSERT_INT_EQ(icanon_active, 1,
                "ICANON bit must be restored to 1 in canonical mode");
  ASSERT_INT_EQ(echo_active, 1,
                "ECHO bit must be restored to 1 in canonical mode");
}

int main(void) {
  printf("\nRunning: %s\n", __FILE__);

  test_terminal_transitions();

  test_summary();
  return tests_failed > 0 ? 1 : 0;
}
