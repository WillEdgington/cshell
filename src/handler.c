#include "cshell/handler.h"
#include "cshell/executor.h"
#include "cshell/runtime.h"
#include "cshell/parser.h"

int cshell_handle_execution(Pipeline *pipe) {
  shell_r.last_exit_status = cshell_execute_pipeline(pipe);

  while (pipe->next != NULL) {
    if (shell_r.last_exit_status == SHELL_STATUS_EXIT)
      return shell_r.last_exit_status;

    LogicalOp op = pipe->next_op;
    pipe = pipe->next;

    if (op == LOGICAL_OP_AND && shell_r.last_exit_status != 0)
      continue;
    if (op == LOGICAL_OP_OR && shell_r.last_exit_status == 0)
      continue;
    shell_r.last_exit_status = cshell_execute_pipeline(pipe);
  }
  return shell_r.last_exit_status;
}
