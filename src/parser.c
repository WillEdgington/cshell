#include "cshell/parser.h"
#include <string.h>

static void command_init(Command *cmd) {
  cmd->arg_count = 0;
  cmd->is_background = 0;
  cmd->input_redirect = NULL;
  cmd->output_redirect = NULL;
}

// If *str is a delimeter (whitespace, \n, \t, \r) return 1 else 0
static int is_delim(char *str) {
  if (*str == ' ' || *str == '\n' || *str == '\t' || *str == '\r')
    return 1;
  return 0;
}

// If *str is a valid shell symbol (&, <, >) return 1 else 0
static int is_symbol(char *str) {
  if (*str == '&' || *str == '>' || *str == '<')
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
  while (*ptr != '\"' && *ptr != '\0')
    ptr++;
  return ptr;
}

static char *parse_redirect(char *str, Command *cmd) {
  char *start = skip_delimeters(str + 1);
  char *end = skip_to_end_of_token(start);
  if (end == start)
    return NULL;

  if (*str == '>') {
    cmd->output_redirect = start;
  } else if (*str == '<') {
    cmd->input_redirect = start;
  }

  *str = '\0';
  return end;
}

static char *parse_is_background(char *str, Command *cmd) {
  *str = '\0';
  str++;

  cmd->is_background = 1;
  return str;
}

static char *parse_arg(char *str, Command *cmd) {
  if (cmd->arg_count == MAX_ARGS)
    return NULL;

  cmd->args[cmd->arg_count] = str;
  cmd->arg_count++;

  if (*str == '\"') {
    char *end = skip_to_closing_quote(str + 1);

    if (*end == '\0')
      return NULL;

    end++;
    if (is_delim(end) || is_symbol(end) || *end == '\0')
      return end;
    return NULL;
  }
  return skip_to_end_of_token(str);
}

static char *parse_token(char *str, Command *cmd) {
  switch (*str) {
  case '>':
  case '<':
    return parse_redirect(str, cmd);
  case '&':
    return parse_is_background(str, cmd);
  default:
    return parse_arg(str, cmd);
  }
}

int cshell_parse_line(char *line, Command *cmd) {
  command_init(cmd);

  char *ptr = skip_delimeters(line);
  while (*ptr != '\0') {
    ptr = parse_token(ptr, cmd);
    if (ptr == NULL) {
      return -1;
    }
    ptr = skip_delimeters(ptr);
  }

  cmd->args[cmd->arg_count] = NULL;
  return 0;
}
