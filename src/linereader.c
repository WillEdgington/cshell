#include "cshell/linereader.h"
#include "cshell/history.h"
#include "cshell/prompt.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef enum { STATE_NORMAL, STATE_ESC_SEEN, STATE_BRACKET_SEEN } InputState;

static void move_cursor(size_t cursor_pos, size_t len) {
  if (cursor_pos < len) {
    size_t delta = len - cursor_pos;
    for (size_t i = 0; i < delta; i++) {
      write(STDOUT_FILENO, "\033[D", 3);
    }
  }
}

static void redraw_line(const char *line) {
  // \r: Move the cursor back to the exact start of the line (column 0)
  // \033[K: ANSI escape code to erase everything from the cursor to end of the
  // line
  const char clear_and_reset[] = "\r\033[K";
  write(STDOUT_FILENO, clear_and_reset, strlen(clear_and_reset));

  cshell_display_prompt();
  write(STDOUT_FILENO, line, strlen(line));
}

static void input_state_esc_seen(InputState *state, char *c) {
  *state = *c == '[' ? STATE_BRACKET_SEEN : STATE_NORMAL;
}

static void
input_state_bracket_seen(InputState *state, char *c, char *buffer,
                         char saved_active_line[HISTORY_MAX_LINE_LEN],
                         int *view_index, size_t *len, size_t *cursor_pos,
                         size_t max_len) {
  *state = STATE_NORMAL;

  switch (*c) {
  // Up arrow
  case 'A': {
    int history_size = cshell_history_get_size();
    if (history_size > 0 && *view_index < history_size - 1) {
      if (*view_index == -1) {
        // Save the active line before moving through history
        strncpy(saved_active_line, buffer, HISTORY_MAX_LINE_LEN - 1);
        saved_active_line[HISTORY_MAX_LINE_LEN - 1] = '\0';
      }

      (*view_index)++;
      const char *hist_entry = cshell_history_get(*view_index);
      if (hist_entry != NULL) {
        strncpy(buffer, hist_entry, max_len - 1);
        buffer[max_len - 1] = '\0';
        *len = strlen(buffer);
        *cursor_pos = *len;
        redraw_line(buffer);
      }
    }
    break;
  }
  // Down arrow
  case 'B':
    if (*view_index > 0) {
      (*view_index)--;
      const char *hist_entry = cshell_history_get(*view_index);
      if (hist_entry != NULL) {
        strncpy(buffer, hist_entry, max_len - 1);
        buffer[max_len - 1] = '\0';
        *len = strlen(buffer);
        *cursor_pos = *len;
        redraw_line(buffer);
      }
    } else if (*view_index == 0) {
      // Back to active line
      (*view_index)--;
      strncpy(buffer, saved_active_line, max_len - 1);
      buffer[max_len - 1] = '\0';
      *len = strlen(buffer);
      *cursor_pos = *len;
      redraw_line(buffer);
    }
    break;
  // Right arrow
  case 'C':
    if (*cursor_pos < *len) {
      (*cursor_pos)++;
      write(STDOUT_FILENO, "\033[C", 3);
    }
    break;
  // Left arrow
  case 'D':
    if (*cursor_pos > 0) {
      (*cursor_pos)--;
      write(STDOUT_FILENO, "\033[D", 3);
    }
    break;
  default:
    break;
  }
}

static int handle_enter(char *buffer, size_t *len, size_t *cursor_pos) {
  if (*cursor_pos < *len) {
    // Move cursor to end of string line
    size_t delta = *len - *cursor_pos;
    for (size_t i = 0; i < delta; i++) {
      write(STDOUT_FILENO, "\033[C", 3);
    }
    *cursor_pos = *len;
  }

  write(STDOUT_FILENO, "\n", 1);
  buffer[*len] = '\0';
  return 1;
}

static void handle_backspace(char *buffer, size_t *len, size_t *cursor_pos) {
  if (*len == 0 || *cursor_pos == 0)
    return;
  (*len)--;
  size_t cur_pos = *cursor_pos - 1;
  while (cur_pos < *len) {
    buffer[cur_pos] = buffer[cur_pos + 1];
    cur_pos++;
  }

  buffer[*len] = '\0';
  (*cursor_pos)--;

  redraw_line(buffer);
  move_cursor(*cursor_pos, *len);
}

static void handle_character_insertion(char *c, char *buffer, size_t *len,
                                       size_t *cursor_pos) {
  (*len)++;
  if (*cursor_pos + 1 < *len) {
    size_t cur_pos = *len;
    while (cur_pos > *cursor_pos) {
      buffer[cur_pos] = buffer[cur_pos - 1];
      cur_pos--;
    }
  }

  buffer[*cursor_pos] = *c;
  buffer[*len] = '\0';
  (*cursor_pos)++;

  redraw_line(buffer);
  move_cursor(*cursor_pos, *len);
}

static int input_state_normal(InputState *state, char *c, char *buffer,
                              size_t *len, size_t *cursor_pos, size_t max_len) {
  switch (*c) {
  // Escape
  case '\033':
    *state = STATE_ESC_SEEN;
    return 0;
  // User pressed enter
  case '\n':
  case '\r':
    return handle_enter(buffer, len, cursor_pos);
  // Backspace
  case 0x7F:
  case 0x08:
    handle_backspace(buffer, len, cursor_pos);
    return 0;
  // CTRL+D (EOF Handled only on empty prompt line)
  case 0x04:
    if (*len == 0)
      return -1;
    return 0;
  // Standard visible ASCII characters
  default:
    if (*c >= 32 && *c <= 126 && *len < max_len - 1)
      handle_character_insertion(c, buffer, len, cursor_pos);
    return 0;
  }
}

ssize_t cshell_read_line(char *buffer, size_t max_len) {
  if (buffer == NULL || max_len == 0)
    return -1;

  size_t cursor_pos = 0;
  size_t len = 0;
  buffer[0] = '\0';

  int view_index = -1;
  char saved_active_line[HISTORY_MAX_LINE_LEN] = {0};

  InputState state = STATE_NORMAL;
  char c;

  while (1) {
    ssize_t n = read(STDIN_FILENO, &c, 1);
    if (n <= 0)
      return -1;

    // State machine logic
    switch (state) {
    case STATE_ESC_SEEN:
      input_state_esc_seen(&state, &c);
      continue;
    case STATE_BRACKET_SEEN:
      input_state_bracket_seen(&state, &c, buffer, saved_active_line,
                               &view_index, &len, &cursor_pos, max_len);
      continue;
    default: {
      int status =
          input_state_normal(&state, &c, buffer, &len, &cursor_pos, max_len);
      if (status != 0)
        return status == -1 ? -1 : (ssize_t)len;
      continue;
    }
    }
  }
}
