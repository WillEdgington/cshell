#ifndef CSHELL_PARSER_H
#define CSHELL_PARSER_H

#define MAX_ARGS 64

typedef struct {
  char *input_redirect;
  char *output_redirect;
  int arg_count;
  int is_background;
  char *args[MAX_ARGS + 1];
} Command;

int cshell_parse_line(char *line, Command *cmd);

#endif
