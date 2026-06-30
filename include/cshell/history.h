#ifndef CSHELL_HISTORY_H
#define CSHELL_HISTORY_H

#define HISTORY_MAX_ENTRIES 32
#define HISTORY_MAX_LINE_LEN 4096

void cshell_history_init(void);
void cshell_history_add(const char *line);
const char *cshell_history_get(int back_offset);
int cshell_history_get_size(void);

#endif