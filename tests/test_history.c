#include "cshell/history.h"
#include "cshell/test_framework.h"

void test_history_basic(void) {
  cshell_history_init();
  ASSERT_INT_EQ(cshell_history_get_size(), 0, "Initial history size must be 0");
  ASSERT_PTR_NULL(cshell_history_get(0),
                  "Retrieving from empty history must return NULL");

  cshell_history_add("ls -la");
  ASSERT_INT_EQ(cshell_history_get_size(), 1,
                "History size must reflect added entry");
  ASSERT_STR_EQ(cshell_history_get(0), "ls -la",
                "Offset 0 must return the most recent entry");
  ASSERT_PTR_NULL(cshell_history_get(1),
                  "Out of bounds offset lookup must return NULL");
}

void test_history_guards(void) {
  cshell_history_init();

  cshell_history_add("");
  cshell_history_add("\n");
  ASSERT_INT_EQ(cshell_history_get_size(), 0,
                "Empty lines or literal newlines must not be stored");

  cshell_history_add("clear\n");
  ASSERT_INT_EQ(cshell_history_get_size(), 1,
                "Valid command containing trailing newline must be accepted");
  ASSERT_STR_EQ(cshell_history_get(0), "clear",
                "Stored entry must have its trailing newline stripped");

  cshell_history_add("mkdir test_dir");
  cshell_history_add("mkdir test_dir");
  ASSERT_INT_EQ(cshell_history_get_size(), 2,
                "Consecutive duplicate command strings must be dropped");
  ASSERT_STR_EQ(cshell_history_get(0), "mkdir test_dir",
                "Offset 0 must match the unique command");
  ASSERT_STR_EQ(cshell_history_get(1), "clear",
                "Offset 1 must match the older distinct entry");
}

static void test_history_wrap_around(void) {
  cshell_history_init();

  char cmd_buffer[16];
  for (int i = 0; i < HISTORY_MAX_ENTRIES + 3; i++) {
    sprintf(cmd_buffer, "cmd_%d", i);
    cshell_history_add(cmd_buffer);
  }

  ASSERT_INT_EQ(cshell_history_get_size(), HISTORY_MAX_ENTRIES,
                "History storage capacity must cap at defined max");

  ASSERT_STR_EQ(cshell_history_get(0), "cmd_34",
                "Offset 0 must return the latest executed command");
  ASSERT_STR_EQ(cshell_history_get(1), "cmd_33",
                "Offset 1 must return the second newest command");

  ASSERT_STR_EQ(cshell_history_get(HISTORY_MAX_ENTRIES - 1), "cmd_3",
                "Offset of max - 1 must resolve to the oldest unevicted entry");
  ASSERT_PTR_NULL(cshell_history_get(HISTORY_MAX_ENTRIES),
                  "Offset exceeding capacity boundaries must evaluate to NULL");
}

int main(void) {
  printf("\nRunning: %s\n", __FILE__);

  test_history_basic();
  test_history_guards();
  test_history_wrap_around();

  test_summary();
  return tests_failed > 0 ? 1 : 0;
}
