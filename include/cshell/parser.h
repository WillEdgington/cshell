#ifndef CSHELL_PARSER_H
#define CSHELL_PARSER_H

#define MAX_ARGS 64

#include "clib/arena.h"

typedef enum {
  LOGICAL_OP_NONE,
  LOGICAL_OP_AND,
  LOGICAL_OP_OR
} LogicalOp;

typedef struct Command {
  char *input_redirect;
  char *output_redirect;
  struct Command *next;
  char *args[MAX_ARGS + 1];
  int arg_count;
} Command;

typedef struct Pipeline {
  Command *head;
  Command *tail;
  Arena *arena;
  struct Pipeline *next;
  LogicalOp next_op;
  int is_background;
  int command_count;
} Pipeline;

void pipeline_init(Pipeline *pipe, Arena *a);
int cshell_parse_line(char *line, Pipeline *pipe);

#endif
