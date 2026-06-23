#include "cshell/prompt.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char *resolve_displayed_path(char *cwd) {
  char *home = getenv(HOME_ENV_KEY);
  if (home == NULL)
    return cwd;

  size_t h_len = strlen(home);
  size_t cwd_len = strlen(cwd);
  if (h_len > cwd_len || h_len == 0)
    return cwd;

  for (size_t i = 0; i < h_len; i++) {
    if (*(home + i) != *(cwd + i))
      return cwd;
  }

  char *display_path = cwd + h_len;
  if (*display_path != '\\' && *display_path != '\0' && *display_path != '/')
    return cwd;

  display_path--;
  *display_path = '~';
  return display_path;
}

void cshell_display_prompt(void) {
  char cwd[PROMPT_PATH_MAX];

  if (getcwd(cwd, sizeof(cwd)) == NULL) {
    printf("cshell> ");
    fflush(stdout);
    return;
  }

  char *displayed_path = resolve_displayed_path(cwd);
  printf("cshell:" PROMPT_CLR_PINK "%s" PROMPT_CLR_RESET "> ", displayed_path);
  fflush(stdout);
}
