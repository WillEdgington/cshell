#ifndef CSHELL_LINEREADER_H
#define CSHELL_LINEREADER_H

#include <sys/types.h>

ssize_t cshell_read_line(char *buffer, size_t max_len);

#endif
