#ifndef CSHELL_COMPLETION_H
#define CSHELL_COMPLETION_H

#include <stddef.h>

#define MAX_COMPLETION_LIST_LEN 30

int cshell_get_completion(const char *prefix, int is_command,
                          char *output_match, size_t max_len);

int cshell_completion_list_matches(const char *prefix, int is_command,
                                   char **matches_out);

#endif