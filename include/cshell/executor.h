#ifndef CSHELL_EXECUTOR_H
#define CSHELL_EXECUTOR_H

#include "cshell/parser.h"

#define SHELL_STATUS_EXIT 2

typedef enum {
  CMD_TYPE_EMPTY,
  CMD_TYPE_EXIT,
  CMD_TYPE_CD,
  CMD_TYPE_EXTERNAL
} CommandType;

CommandType cshell_resolve_command(const Command *cmd);
int cshell_execute(Command *cmd);

#endif
