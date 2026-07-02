#define _GNU_SOURCE

#include "cshell/completion.h"
#include "cshell/runtime.h"
#include "cshell/test_framework.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void setup_mock_dir(const char *path) { mkdir(path, 0777); }

static void teardown_mock_dir(const char *path) { rmdir(path); }

static void setup_mock_file(const char *path) {
  FILE *f = fopen(path, "w");
  if (f != NULL)
    fclose(f);
}

static void teardown_mock_file(const char *path) { unlink(path); }

static void test_completion_local_file(void) {
  char match[MAX_LINE_LEN] = {0};
  char *filename = "_test_f_48593823";
  setup_mock_file(filename);

  ASSERT_INT_EQ(cshell_get_completion("_test_f_", 0, match, sizeof(match)), 1,
                "Should successfully find unique file match");
  ASSERT_STR_EQ(match, "_test_f_48593823 ",
                "Match string must contain trailing formatting space");

  teardown_mock_file(filename);
}

static void test_completion_nested_directory(void) {
  char match[MAX_LINE_LEN] = {0};
  char *parent_dir = "_test_d_98473821";
  char *sub_dir = "_test_d_98473821/_test_sub_11223344";

  setup_mock_dir(parent_dir);
  setup_mock_dir(sub_dir);

  ASSERT_INT_EQ(cshell_get_completion("./_test_d_98473821/_test_s", 0, match,
                                      sizeof(match)),
                1, "Should successfully resolve nested directory targets");
  ASSERT_STR_EQ(match, "./_test_d_98473821/_test_sub_11223344/",
                "Directories must preserve folder paths and append slashes");

  teardown_mock_dir(sub_dir);
  teardown_mock_dir(parent_dir);
}

static void test_completion_ambiguity_or_none(void) {
  char match[MAX_LINE_LEN] = {0};
  ASSERT_INT_EQ(
      cshell_get_completion("unknown_18537291", 0, match, sizeof(match)), 0,
      "Missing target prefixes must return zero status");

  char match_ambig[MAX_LINE_LEN] = {0};

  char *filename_1 = "_test_f_47856321";
  char *filename_2 = "_test_f_23224532";
  setup_mock_file(filename_1);
  setup_mock_file(filename_2);

  ASSERT_INT_EQ(
      cshell_get_completion("_te", 1, match_ambig, sizeof(match_ambig)), 1,
      "LCP for ambiguous completion request should be found");
  ASSERT_STR_EQ(
      match_ambig, "_test_f_",
      "LCP for ambiguous completion request should resolve correctly");

  teardown_mock_file(filename_1);
  teardown_mock_file(filename_2);
}

int main(void) {
  printf("\nRunning: %s\n", __FILE__);

  test_completion_local_file();
  test_completion_nested_directory();
  test_completion_ambiguity_or_none();

  test_summary();
  return tests_failed > 0 ? 1 : 0;
}