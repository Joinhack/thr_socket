#ifndef CSTR_H
#define CSTR_H

#include "common.h"

#define cstr char *

cstr cstr_create(size_t len);
cstr cstr_new(const char *c, size_t len);
void cstr_destroy(cstr s);
cstr cstr_dup(cstr s);
cstr cstr_ncat(cstr s, const char *b, size_t l);
cstr* cstr_split(char *s, size_t len, char *b, size_t slen, size_t *l);
void cstr_clear(cstr s);
void cstr_tolower(cstr s);
void cstr_toupper(cstr s);
cstr cstr_range(cstr s, int b, int e);

typedef struct {
    uint32_t len;
    uint32_t free;
    char buf[];
} cstrhdr;

#define HLEN sizeof(cstrhdr)
#define CSTR_REALPTR(s) ((char*)(s - HLEN))
#define CSTR_HDR(s) ((cstrhdr*)(s - HLEN))
#define cstr_len(s) CSTR_HDR(s)->len
#define CSTR_HDR_USED(s) (s->len - s->free)
#define cstr_used(s) (CSTR_HDR_USED(CSTR_HDR(s)))

#endif /*end common str define*/
