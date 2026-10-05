#ifndef TCMG_WEBIF_FORM_H_
#define TCMG_WEBIF_FORM_H_

#include "proto.h"
#include "../../src/core/utils.h"
#include <ctype.h>
#include <string.h>

static inline int webif_form_copy(const char *body, const char *key, char *out, size_t outsz)
{
    return form_get_copy(body, key, out, outsz) < 0 ? -1 : 0;
}

static inline int webif_form_present(const char *body, const char *key)
{
    return form_has(body, key);
}

static inline void webif_trim(char *s)
{
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
    char *p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
}

#endif
