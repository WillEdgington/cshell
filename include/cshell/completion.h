#ifndef CSHELL_COMPLETION_H
#define CSHELL_COMPLETION_H

#include <stddef.h>

int cshell_get_completion(const char *prefix, int is_command,
                          char *output_match, size_t max_len);

#endif