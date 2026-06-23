#ifndef CSHELL_PROMPT_H
#define CSHELL_PROMPT_H

#define PROMPT_CLR_PINK "\033[38;5;211m"
#define PROMPT_CLR_RESET "\033[0m"
#define PROMPT_PATH_MAX 256
#define HOME_ENV_KEY "HOME"

void cshell_display_prompt(void);

#endif