#define _GNU_SOURCE

#include "cshell/completion.h"
#include "cshell/runtime.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void scan_directory(const char *dir_path, const char *prefix,
                           char *output_match, size_t max_len,
                           int *match_count) {
  DIR *dir = opendir(dir_path);
  if (dir == NULL)
    return;

  size_t prefix_len = strlen(prefix);
  struct dirent *entry;

  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;

    if (strncmp(entry->d_name, prefix, prefix_len) == 0) {
      char formatted_name[MAX_LINE_LEN];
      if (entry->d_type == DT_DIR) {
        snprintf(formatted_name, sizeof(formatted_name), "%s/", entry->d_name);
      } else {
        snprintf(formatted_name, sizeof(formatted_name), "%s ", entry->d_name);
      }

      if (*match_count == 0) {
        strncpy(output_match, formatted_name, max_len - 1);
        output_match[max_len - 1] = '\0';
        (*match_count)++;
      } else {
        size_t i = 0;
        while (output_match[i] != '\0' && formatted_name[i] != '\0' &&
               output_match[i] == formatted_name[i]) {
          i++;
        }
        output_match[i] = '\0';
        (*match_count)++;
      }
    }
  }
  closedir(dir);
}

int cshell_get_completion(const char *prefix, int is_command,
                          char *output_match, size_t max_len) {
  if (prefix == NULL || output_match == NULL || max_len == 0)
    return 0;

  int match_count = 0;
  output_match[0] = '\0';

  char target_dir[MAX_LINE_LEN] = ".";
  const char *search_fragment = prefix;

  const char *last_slash = strrchr(prefix, '/');
  if (last_slash != NULL) {
    size_t dir_len = last_slash - prefix + 1;
    if (dir_len < sizeof(target_dir)) {
      strncpy(target_dir, prefix, dir_len);
      target_dir[dir_len] = '\0';
    }
    search_fragment = last_slash + 1;
  }

  if (is_command == 1 && last_slash == NULL) {
    const char *raw_path = getenv("PATH");
    if (raw_path != NULL) {
      char *path_copy = strdup(raw_path);
      if (path_copy != NULL) {
        char *dir_path = strtok(path_copy, ":");
        while (dir_path != NULL) {
          scan_directory(dir_path, search_fragment, output_match, max_len,
                         &match_count);
          dir_path = strtok(NULL, ":");
        }
        free(path_copy);
      }
    }

    if (match_count == 0) {
      scan_directory(".", search_fragment, output_match, max_len, &match_count);
    }
  } else {
    scan_directory(target_dir, search_fragment, output_match, max_len,
                   &match_count);
  }

  if (match_count > 0 && last_slash != NULL && strlen(output_match) > 0) {
    char temp[MAX_LINE_LEN];
    snprintf(temp, sizeof(temp), "%s%s", target_dir, output_match);
    strncpy(output_match, temp, max_len - 1);
    output_match[max_len - 1] = '\0';
  }

  return (match_count > 0 && strlen(output_match) > strlen(prefix)) ? 1 : 0;
}
