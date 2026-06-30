#include "cshell/linereader.h"
#include "cshell/history.h"
#include "cshell/prompt.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef enum { STATE_NORMAL, STATE_ESC_SEEN, STATE_BRACKET_SEEN } InputState;

static void redraw_line(const char *line) {
  // \r: Move the cursor back to the exact start of the line (column 0)
  // \033[K: ANSI escape code to erase everything from the cursor to end of the
  // line
  const char clear_and_reset[] = "\r\033[K";
  write(STDOUT_FILENO, clear_and_reset, strlen(clear_and_reset));

  cshell_display_prompt();
  write(STDOUT_FILENO, line, strlen(line));
}

ssize_t cshell_read_line(char *buffer, size_t max_len) {
  if (buffer == NULL || max_len == 0)
    return -1;

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
    if (state == STATE_ESC_SEEN) {
      state = c == '[' ? STATE_BRACKET_SEEN : STATE_NORMAL;
      continue;
    }

    if (state == STATE_BRACKET_SEEN) {
      state = STATE_NORMAL;

      // Up arrow
      if (c == 'A') {
        int history_size = cshell_history_get_size();
        if (history_size > 0 && view_index < history_size - 1) {
          if (view_index == -1) {
            // Save the active line before moving through history
            strncpy(saved_active_line, buffer, HISTORY_MAX_LINE_LEN - 1);
            saved_active_line[HISTORY_MAX_LINE_LEN - 1] = '\0';
          }

          view_index++;
          const char *hist_entry = cshell_history_get(view_index);
          if (hist_entry != NULL) {
            strncpy(buffer, hist_entry, max_len - 1);
            buffer[max_len - 1] = '\0';
            len = strlen(buffer);
            redraw_line(buffer);
          }
        }
      } else if (c == 'B') { // Down arrow
        if (view_index > 0) {
          view_index--;
          const char *hist_entry = cshell_history_get(view_index);
          if (hist_entry != NULL) {
            strncpy(buffer, hist_entry, max_len - 1);
            buffer[max_len - 1] = '\0';
            len = strlen(buffer);
            redraw_line(buffer);
          }
        } else if (view_index == 0) {
          // Back to active line
          strncpy(buffer, saved_active_line, max_len - 1);
          buffer[max_len - 1] = '\0';
          len = strlen(buffer);
          redraw_line(buffer);
        }
      }
      continue;
    }

    // Escape
    if (c == '\033') {
      state = STATE_ESC_SEEN;
      continue;
    }

    // User pressed enter
    if (c == '\n' || c == '\r') {
      write(STDOUT_FILENO, "\n", 1);
      buffer[len] = '\0';
      return (ssize_t)len;
    }

    // Backspace
    if (c == 0x7F || c == 0x08) {
      if (len > 0) {
        len--;
        buffer[len] = '\0';
        // move cursor left ('\b'), overwrite with space (' '), move left again
        // ('\b')
        write(STDOUT_FILENO, "\b \b", 3);
      }
      continue;
    }

    // CTRL+D (EOF Handled only on empty prompt line)
    if (c == 0x04) {
      if (len == 0)
        return -1;
      continue;
    }

    // Standard visible ASCII characters
    if (c >= 32 && c <= 126) {
      if (len < max_len - 1) {
        buffer[len] = c;
        len++;
        buffer[len] = '\0';
        write(STDOUT_FILENO, &c, 1);
      }
    }
  }
}
