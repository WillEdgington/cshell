#ifndef CSHELL_EXECUTOR_H
#define CSHELL_EXECUTOR_H

#include "cshell/parser.h"

#define SHELL_STATUS_EXIT 256
#define CHILD_EXEC_FAILURE 127

typedef enum {
  CMD_TYPE_EMPTY,
  CMD_TYPE_EXIT,
  CMD_TYPE_CD,
  CMD_TYPE_EXTERNAL,
  CMD_TYPE_EXPORT
} CommandType;

CommandType cshell_resolve_command(const Command *cmd);
int cshell_execute_command(Command *cmd);
int cshell_execute_pipeline(Pipeline *pipe);

void cshell_init_signals(void);

#endif
