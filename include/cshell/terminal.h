#ifndef CSHELL_TERMINAL_H
#define CSHELL_TERMINAL_H

int cshell_terminal_init(void);
void cshell_terminal_mode_raw(void);
void cshell_terminal_mode_canonical(void);

#endif