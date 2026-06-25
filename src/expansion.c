#include "cshell/expansion.h"
#include "clib/arena.h"
#include "cshell/parser.h"
#include "cshell/runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cshell_expand_pipeline(Pipeline *pipeline) {
  for (Command *cmd = pipeline->head; cmd != NULL; cmd = cmd->next) {
    for (int i = 0; i < cmd->arg_count; i++) {
      char *arg = cmd->args[i];

      if (arg != NULL && arg[0] == '$') {
        char *var_name = arg + 1;
        
        char *expanded_str;
        if (*var_name == '?' && *(var_name + 1) == '\0') {
          expanded_str = arena_alloc(pipeline->arena, 12);
          snprintf(expanded_str, 12, "%d", shell_r.last_exit_status);
        } else {
          char *env_val = getenv(var_name);
          if (env_val == NULL) {
            *cmd->args[i] = '\0';
            return 0;
          }

          expanded_str = arena_alloc(pipeline->arena, strlen(env_val) + 1);
          if (expanded_str == NULL) {
            fprintf(stderr, "cshell: memory allocation failure inside arena\n");
            return -1;
          }
          strcpy(expanded_str, env_val);
        }

        cmd->args[i] = expanded_str;
      }
    }
  }

  return 0;
}
