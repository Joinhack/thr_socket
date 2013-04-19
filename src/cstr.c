#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include "jmalloc.h"
#include "cstr.h"

#define CSH_USED(c) (c->len - c->free)

cstr cstr_create(size_t len) {
	char *c = jmalloc(len + HLEN + 1);
	cstrhdr *csh = (cstrhdr*)c;
	csh->len = len;
	csh->free = len;
	csh->buf[len] = 0;
	return (cstr)csh->buf;
}

cstr cstr_new(const char *c, size_t len) {
	cstr s = cstr_create(len);
	cstrhdr *csh = CSTR_HDR(s);
	memcpy(s, c, len);
	s[len] = '\0';
	csh->free = 0;
	return s;
}

void cstr_destroy(cstr s) {
	char *ptr = CSTR_REALPTR(s);
	jfree(ptr);
}

cstr cstr_extend(cstr s, size_t add) {
	cstrhdr *csh = CSTR_HDR(s);
	if(csh->free >= add) return s;
	csh->len = (CSTR_HDR_USED(csh) + add)*2;
	csh = jrealloc((void*)csh, csh->len + HLEN + 1);
	return (cstr)csh->buf;
}

cstr cstr_ncat(cstr s, const char *b, size_t l) {
	cstrhdr *csh;
	s = cstr_extend(s, l);
	csh = CSTR_HDR(s);
	memcpy(csh->buf + CSH_USED(csh), b, l);
	csh->free -= l;
	s[CSTR_HDR_USED(csh)] = '\0';
	return (cstr)csh->buf;
}

void cstr_clear(cstr s) {
	cstrhdr *csh = CSTR_HDR(s);
	csh->free = csh->len;
	csh->buf[0] = '\0';
}

cstr* cstr_split(char *s, size_t len, char *b, size_t slen, size_t *l) {
	cstr *array = NULL;
	size_t i, j, cap = 0, size = 0, beg = 0;
	for(i = 0; i < len - slen; i++) {
		if(size + 1 >= cap ) {
			cap += 5;
			array = jrealloc(array, sizeof(cstr)*cap);
		}
		if(s[i] == b[0] && memcmp(s + i, b, slen) == 0) {
			array[size] = cstr_new(s + beg, i - beg);
			beg = i + slen;
			size++;
		}
	}
	array[size++] = cstr_new(s + beg, len - beg);
	*l = size;
	return array;
}

cstr cstr_range(cstr s, int b, int e) {
	cstrhdr *csh = CSTR_HDR(s);
	int used = CSTR_HDR_USED(csh);
	int end = e, begin = b, nlen;

	if(end < 0) {
		end += used;
		if(end < 0)
			end = 0;
	}
	if(begin < 0) {
		begin += used;
		if(begin < 0)
			begin = 0;	
	}
	nlen = begin < end? end - begin : 0;
	if(nlen != 0) {
		if(begin > used)
			nlen = 0;
		else if(end > used) {
			end = used - 1;
			nlen = begin < end? end - begin : 0;
		}
	}
	if(nlen && begin) memmove(csh->buf, csh->buf + begin, nlen);
	csh->buf[nlen] = 0;
	csh->free += used - nlen;
	return s;
}

void cstr_tolower(cstr s) {
	size_t i;
	for(i = 0; i < cstr_used(s); i++) s[i] = tolower(s[i]);
}

void cstr_toupper(cstr s) {
	size_t i;
	for(i = 0; i < cstr_used(s); i++) s[i] = toupper(s[i]);
}

cstr cstr_dup(cstr s) {
	size_t used = cstr_used(s);
	cstr ns = cstr_create(used);
	cstr_ncat(ns, s, used);
	return ns;
}

