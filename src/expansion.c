#include "cshell/expansion.h"
#include "clib/arena.h"
#include "cshell/parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cshell_expand_pipeline(Pipeline *pipeline) {
  for (Command *cmd = pipeline->head; cmd != NULL; cmd = cmd->next) {
    for (int i = 0; i < cmd->arg_count; i++) {
      char *arg = cmd->args[i];

      if (arg != NULL && arg[0] == '$') {
        char *var_name = arg + 1;
        char *env_val = getenv(var_name);

        if (env_val == NULL) {
          fprintf(stderr, "cshell: environment variable %s does not exist\n",
                  var_name);
          return -1;
        }

        char *expanded_str = arena_alloc(pipeline->arena, strlen(env_val) + 1);
        if (expanded_str == NULL) {
          fprintf(stderr, "cshell: memory allocation failure inside arena\n");
          return -1;
        }

        strcpy(expanded_str, env_val);
        cmd->args[i] = expanded_str;
      }
    }
  }

  return 0;
}
