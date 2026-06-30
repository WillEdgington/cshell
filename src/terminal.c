#include "cshell/terminal.h"

#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

static struct termios orig_termios;
static int terminal_initialised = 0;
static int in_raw_mode = 0;

// terminal driver state is explicitly recovered if the program exits
// (abnormally or calls exit())
static void terminal_atexit_cleanup(void) {
  if (terminal_initialised && in_raw_mode)
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

int cshell_terminal_init(void) {
  if (terminal_initialised)
    return 0;
  if (!isatty(STDIN_FILENO))
    return -1;
  if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)
    return -1;
  if (atexit(terminal_atexit_cleanup) != 0)
    return -1;

  terminal_initialised = 1;
  return 0;
}

void cshell_terminal_mode_raw(void) {
  if (!terminal_initialised || in_raw_mode)
    return;

  struct termios raw = orig_termios;

  // ~ICANON: turn off canonical processing. Input becomes character-driven
  // ~ECHO: turn off automatic visual echo of incoming characters
  raw.c_lflag &= ~(ICANON | ECHO);

  // VMIN: min number of bytes to read before read() returns. 1 makes read block
  // until at least a single byte is ready VTIME: timeout config in tenths of a
  // second. 0 for infinite blocking (standard interactive loop)
  raw.c_cc[VMIN] = 1;
  raw.c_cc[VTIME] = 0;

  // TCSANOW: apply instantly
  if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != -1)
    in_raw_mode = 1;
}

void cshell_terminal_mode_canonical(void) {
  if (!terminal_initialised || !in_raw_mode)
    return;

  // TCSAFLUSH: tells terminal subsystem to wait for any unwritten output to
  // finish rendering and discard any unread input before altering modes
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) != -1)
    in_raw_mode = 0;
}
