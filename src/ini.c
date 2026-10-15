#include "ini.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) *--e = 0;
    return s;
}

int ini_parse_text(char *text, ini_cb cb, void *user) {
    char section[32] = "";
    char *line = text, *next;
    int n = 0;
    while (line && *line) {
        next = strchr(line, '\n');
        if (next) *next++ = 0;
        char *s = trim(line);
        if (*s == '[') { char *e = strchr(s, ']'); if (e) { *e = 0; strncpy(section, s + 1, sizeof section - 1); } }
        else if (*s && *s != ';' && *s != '#') {
            char *eq = strchr(s, '=');
            if (eq) { *eq = 0; char *v = trim(eq + 1); if (!cb(user, section, trim(s), v)) return n; n++; }
        }
        line = next;
    }
    return n;
}

int ini_parse_file(const char *path, ini_cb cb, void *user) {
    FILE *f = fopen(path, "rb"); if (!f) return -1;
    fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc(len + 1); if (!buf) { fclose(f); return -1; }
    len = fread(buf, 1, len, f); buf[len] = 0; fclose(f);
    int n = ini_parse_text(buf, cb, user); free(buf); return n;
}
