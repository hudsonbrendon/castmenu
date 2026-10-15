/* Tiny line-oriented INI reader: callback per key. Sections tracked. */
#pragma once
typedef int (*ini_cb)(void *user, const char *section, const char *key, const char *value);
int ini_parse_file(const char *path, ini_cb cb, void *user);
int ini_parse_text(char *text, ini_cb cb, void *user); /* mutates text */
