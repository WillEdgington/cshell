#include "cshell/parser.h"
#include <stdio.h>
#include <string.h>

static void command_init(Command *cmd) {
  cmd->arg_count = 0;
  cmd->next = NULL;
  cmd->input_redirect = NULL;
  cmd->output_redirect = NULL;
}

static int command_cap(Command *cmd) {
  if (cmd == NULL || cmd->arg_count == 0)
    return -1;

  cmd->args[cmd->arg_count] = NULL;
  cmd->next = NULL;
  return 0;
}

// If *str is a delimeter (whitespace, \n, \t, \r) return 1 else 0
static int is_delim(char *str) {
  if (*str == ' ' || *str == '\n' || *str == '\t' || *str == '\r')
    return 1;
  return 0;
}

// If *str is a valid shell symbol (&, <, >) return 1 else 0
static int is_symbol(char *str) {
  if (*str == '&' || *str == '>' || *str == '<' || *str == '|')
    return 1;
  return 0;
}

static char *skip_delimeters(char *str) {
  while (is_delim(str)) {
    *str = '\0';
    str++;
  }
  return str;
}

static char *skip_to_end_of_token(char *str) {
  char *ptr = str;
  while (!(is_delim(ptr) || is_symbol(ptr) || *ptr == '\0'))
    ptr++;
  return ptr;
}

static char *skip_to_closing_quote(char *str) {
  char *ptr = str;

  int has_escapes = 0;
  int escaped = 0;

  while ((escaped || *ptr != '\"') && *ptr != '\0') {
    if (escaped && (*ptr == '\"' || *ptr == '\\'))
      has_escapes = 1;
    escaped = !escaped && *ptr == '\\' ? 1 : 0;
    ptr++;
  }

  if (has_escapes) {
    escaped = 0;
    char *orig = str;
    char *changed = str;

    while (orig < ptr) {
      if (escaped && (*orig == '\"' || *orig == '\\'))
        changed--;
      escaped = !escaped && *orig == '\\' ? 1 : 0;

      *changed = *orig;
      orig++;
      changed++;
    }

    while (changed < ptr) {
      *changed = '\0';
      changed++;
    }
  }
  return ptr;
}

static char *parse_redirect(char *str, Command *cmd) {
  char op = *str;
  char *start = skip_delimeters(str + 1);
  char *end = skip_to_end_of_token(start);
  if (end == start) {
    fprintf(stderr, "cshell: syntax error near unexpected token '%c'\n", op);
    return NULL;
  }

  if (op == '>') {
    cmd->output_redirect = start;
  } else if (op == '<') {
    cmd->input_redirect = start;
  }

  *str = '\0';
  return end;
}

static char *parse_is_background(char *str, Pipeline *pipe) {
  *str = '\0';
  str++;

  pipe->is_background = 1;
  return str;
}

static char *parse_new_cmd(char *str, Pipeline *pipe) {
  *str = '\0';
  str++;

  if (command_cap(pipe->tail) == -1) {
    fprintf(stderr, "cshell: syntax error near unexpected token '|'\n");
    return NULL;
  }

  Command *next_cmd = arena_alloc(pipe->arena, sizeof(Command));
  if (next_cmd == NULL) {
    fprintf(stderr, "cshell: memory allocation failure inside arena\n");
  }

  pipe->tail->next = next_cmd;
  pipe->tail = pipe->tail->next;
  command_init(pipe->tail);
  pipe->command_count++;
  return str;
}

static char *parse_arg(char *str, Command *cmd) {
  if (cmd->arg_count == MAX_ARGS) {
    fprintf(stderr, "cshell: maximum argument limit exceeded (%d)\n", MAX_ARGS);
    return NULL;
  }

  if (*str == '\"') {
    *str = '\0';
    str++;

    cmd->args[cmd->arg_count] = str;
    cmd->arg_count++;
    char *end = skip_to_closing_quote(str);

    if (*end == '\0') {
      fprintf(stderr, "cshell: syntax error: unmatched double quote\n");
      return NULL;
    }

    *end = '\0';
    end++;
    if (is_delim(end) || is_symbol(end) || *end == '\0')
      return end;

    fprintf(
        stderr,
        "cshell: syntax error: missing space delimiter after closing quote\n");
    return NULL;
  }

  cmd->args[cmd->arg_count] = str;
  cmd->arg_count++;
  return skip_to_end_of_token(str);
}

static char *parse_token(char *str, Pipeline *pipe) {
  switch (*str) {
  case '>':
  case '<':
    return parse_redirect(str, pipe->tail);
  case '&':
    return parse_is_background(str, pipe);
  case '|':
    return parse_new_cmd(str, pipe);
  default:
    return parse_arg(str, pipe->tail);
  }
}

void pipeline_init(Pipeline *pipe, Arena *a) {
  pipe->arena = a;
  pipe->head = (Command *)arena_alloc(a, sizeof(Command));
  pipe->tail = pipe->head;
  pipe->is_background = 0;
  pipe->command_count = 1;
  command_init(pipe->head);
}

int cshell_parse_line(char *line, Pipeline *pipe) {
  char *ptr = skip_delimeters(line);

  if (*ptr == '\0') {
    pipe->command_count = 0;
    pipe->head->args[0] = NULL;
    return 0;
  }

  while (*ptr != '\0') {
    if (pipe->is_background == 1) {
      fprintf(stderr, "cshell: syntax error: non-trailing '&'\n");
      return -1;
    }

    ptr = parse_token(ptr, pipe);
    if (ptr == NULL) {
      return -1;
    }
    ptr = skip_delimeters(ptr);
  }

  if (command_cap(pipe->tail) == -1) {
    fprintf(stderr,
            "cshell: syntax error: dangling pipeline modifier sequence\n");
    return -1;
  }
  return 0;
}
