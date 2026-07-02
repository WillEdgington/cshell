#include "cshell/history.h"
#include "cshell/runtime.h"
#include <string.h>

static char history_buffer[HISTORY_MAX_ENTRIES][MAX_LINE_LEN];
static int history_head = 0;
static int history_size = 0;

void cshell_history_init(void) {
  history_head = 0;
  history_size = 0;
  memset(history_buffer, 0, sizeof(history_buffer));
}

int cshell_history_get_size(void) { return history_size; }

void cshell_history_add(const char *line) {
  if (line == NULL)
    return;

  char clean_line[MAX_LINE_LEN];
  strncpy(clean_line, line, MAX_LINE_LEN - 1);
  clean_line[MAX_LINE_LEN - 1] = '\0';
  size_t len = strlen(clean_line);
  if (len > 0 && clean_line[len - 1] == '\n')
    clean_line[len - 1] = '\0';

  if (strlen(clean_line) == 0)
    return;

  // check for duplicate entry
  if (history_size > 0) {
    int last_index =
        (history_head - 1 + HISTORY_MAX_ENTRIES) % HISTORY_MAX_ENTRIES;
    if (strcmp(history_buffer[last_index], clean_line) == 0)
      return;
  }

  strncpy(history_buffer[history_head], clean_line, MAX_LINE_LEN - 1);
  history_buffer[history_head][MAX_LINE_LEN - 1] = '\0';

  history_head = (history_head + 1) % HISTORY_MAX_ENTRIES;
  if (history_size < HISTORY_MAX_ENTRIES)
    history_size++;
}

const char *cshell_history_get(int back_offset) {
  if (back_offset < 0 || back_offset >= history_size)
    return NULL;

  int target_index = (history_head - 1 - back_offset) % HISTORY_MAX_ENTRIES;
  if (target_index < 0)
    target_index += HISTORY_MAX_ENTRIES;

  return history_buffer[target_index];
}
