#ifndef TCMG_WEBIF_FORM_H_
#define TCMG_WEBIF_FORM_H_

#include "proto.h"
#include "../../src/core/utils.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static inline int webif_form_copy(const char *body, const char *key, char *out, size_t outsz)
{
    out[0] = '\0';
    char *value = form_get_alloc(body, key);
    if (!value) return 0;
    int too_long = strlen(value) >= outsz;
    if (!too_long) tcmg_strlcpy(out, value, outsz);
    free(value);
    return too_long ? -1 : 0;
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
