#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

/* This is a direct copy of the test framework wrote in
 * github.com/WillEdgington/clib */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define COLOR_RED "\x1b[31m"
#define COLOR_GREEN "\x1b[32m"
#define COLOR_RESET "\x1b[0m"

static int tests_run = 0;
static int tests_failed = 0;

#define _ASSERT_FAIL(msg, ...)                                                 \
  do {                                                                         \
    printf(COLOR_RED "  [FAIL] " COLOR_RESET);                                 \
    printf(msg, ##__VA_ARGS__);                                                \
    printf("\n         Line %d in %s\n", __LINE__, __FILE__);                  \
    tests_failed++;                                                            \
  } while (0)

#define ASSERT(condition, message)                                             \
  do {                                                                         \
    tests_run++;                                                               \
    if (!(condition)) {                                                        \
      _ASSERT_FAIL("%s", message);                                             \
    } else {                                                                   \
      printf(COLOR_GREEN "  [PASS] " COLOR_RESET "%s\n", message);             \
    }                                                                          \
  } while (0)

#define ASSERT_INT_EQ(actual, expected, message)                               \
  do {                                                                         \
    tests_run++;                                                               \
    if ((actual) != (expected)) {                                              \
      _ASSERT_FAIL("%s (Expected %d, got %d)", message, (int)expected,         \
                   (int)actual);                                               \
    } else {                                                                   \
      printf(COLOR_GREEN "  [PASS] " COLOR_RESET "%s\n", message);             \
    }                                                                          \
  } while (0)

#define ASSERT_STR_EQ(actual, expected, message)                               \
  do {                                                                         \
    tests_run++;                                                               \
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) { \
      _ASSERT_FAIL("%s (Expected \"%s\", got \"%s\")", message,                \
                   expected != NULL ? expected : "NULL",                       \
                   actual != NULL ? actual : "NULL");                          \
    } else {                                                                   \
      printf(COLOR_GREEN "  [PASS] " COLOR_RESET "%s\n", message);             \
    }                                                                          \
  } while (0)

#define ASSERT_PTR_NOT_NULL(ptr, message)                                      \
  do {                                                                         \
    tests_run++;                                                               \
    if ((ptr) == NULL) {                                                       \
      _ASSERT_FAIL("%s (Pointer is NULL)", message);                           \
    } else {                                                                   \
      printf(COLOR_GREEN "  [PASS] " COLOR_RESET "%s\n", message);             \
    }                                                                          \
  } while (0)

#define ASSERT_PTR_NULL(ptr, message)                                          \
  do {                                                                         \
    tests_run++;                                                               \
    if ((ptr) == NULL) {                                                       \
      printf(COLOR_GREEN "  [PASS] " COLOR_RESET "%s\n", message);             \
    } else {                                                                   \
      _ASSERT_FAIL("%s (Pointer is not NULL)", message);                       \
    }                                                                          \
  } while (0)

static inline void test_summary() {
  printf("\n---------------------------\n");
  if (tests_failed == 0) {
    printf(COLOR_GREEN "ALL %d TESTS PASSED" COLOR_RESET "\n", tests_run);
  } else {
    printf(COLOR_RED "TESTS FAILED: %d / %d" COLOR_RESET "\n", tests_failed,
           tests_run);
  }
  printf("---------------------------\n");
}

#endif