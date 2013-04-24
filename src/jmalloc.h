#ifndef JMALLOC_H
#define JMALLOC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void *jmalloc(size_t s);
void *jrealloc(void *ptr, size_t s);
void jfree(void *ptr);
uint64_t used_mem();
uint64_t total_mem();

#ifdef __cplusplus
}
#endif

#endif /*end define __JMALLOC_H**/
