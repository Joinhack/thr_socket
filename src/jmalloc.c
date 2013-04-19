#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "common.h"
#include "atomic.h"
#include "jmalloc.h"

//if jemalloc please compile the jemalloc use --with-jemalloc-prefix=je_
#ifdef USE_JEMALLOC
#include <jemalloc/jemalloc.h>
#define malloc(size) je_malloc(size)
#define realloc(ptr, size) je_realloc(ptr, size)
#define free(ptr) je_free(ptr)
#define HAD_MEM_SIZE
#define mem_size(ptr) je_malloc_usable_size(ptr)
#else
#define malloc(size) malloc(size)
#define realloc(ptr, size) realloc(ptr, size)
#define free(ptr) free(ptr)
#endif

#ifndef HAD_MEM_SIZE
#define MEM_PREFIX_SIZE 4
#define mem_size(ptr) *((uint32_t*)ptr - MEM_PREFIX_SIZE)
#endif


#define update_used_mem(size) atomic_add_uint64(&_used_mem, size)
#define oom_test(ptr, size) \
if(ptr == NULL) { \
	fprintf(stderr, "Out of memory trying to allocate %ld\n", size); \
	abort(); \
}

static uint64_t _used_mem = 0;

void *jmalloc(size_t s) {
	void *ptr;
#ifndef HAD_MEM_SIZE
	ptr = malloc(s + MEM_PREFIX_SIZE);
	oom_test(ptr, s);
	*((uint32_t*)ptr) = s;
	update_used_mem(s + MEM_PREFIX_SIZE);
	return (char*)ptr + MEM_PREFIX_SIZE;
#else
	ptr = malloc(s);
	oom_test(ptr, s);
	update_used_mem(mem_size(ptr));
	return ptr;
#endif
}

void *jrealloc(void *ptr, size_t s) {
	void *new_ptr;
	uint32_t old_size = 0;
	uint32_t size = 0;
#ifndef HAD_MEM_SIZE
	void *real_ptr = NULL;
	if(ptr != NULL) {
		real_ptr = (char*)ptr - MEM_PREFIX_SIZE;
		old_size = *((uint32_t*)real_ptr);
		size = s - old_size;
	} else
		size = s + MEM_PREFIX_SIZE;
	new_ptr = realloc(real_ptr, s);
	oom_test(new_ptr, s);
	*((uint32_t*)new_ptr) = s;
	update_used_mem(size);
	return (char*)new_ptr + MEM_PREFIX_SIZE;
#else
	old_size = mem_size(ptr);
	new_ptr = realloc(ptr, s);
	oom_test(new_ptr, s);
	update_used_mem(mem_size(new_ptr) - old_size);
	return new_ptr;
#endif
}

void jfree(void* ptr) {
	size_t size;
#ifndef HAD_MEM_SIZE
	void *real_ptr;
	real_ptr = (char*)ptr - MEM_PREFIX_SIZE;
	size = *((uint32_t*)real_ptr);
	update_used_mem(-(size + MEM_PREFIX_SIZE));
	free(real_ptr);
#else
	size = mem_size(ptr);
	update_used_mem(-mem_size(ptr));
	free(ptr);
#endif
}

uint64_t used_mem() {
	return _used_mem;
}

#if defined(USE_PROCFILE)
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
uint64_t total_mem() {
	char path[1024], data[4096], *ptr, *end;
	int fd, c;
	uint64_t size;
	char val;
	int page;
	memset(path, 0, sizeof(path));
	memset(data, 0, sizeof(data));
	snprintf(path, sizeof(path), "/proc/%d/stat", getpid());
	if((fd = open(path, O_RDONLY)) < 0) {
		fprintf(stderr, "error open stat\n");
		return 0;
	}
	if(read(fd, data, sizeof(data)) < 0) {
		close(fd);
		return 0;
	}
	close(fd);
	//the 24th colum is the size
	c = 23;
	ptr = data;
	while(ptr && c--) {
		ptr = strchr(ptr, ' ');
		if(ptr)
			ptr++;
	}
	if(!ptr)
		return 0;
	end = strchr(ptr, ' ');
	end[0] = 0x0;
	page = sysconf(_SC_PAGESIZE);
	size = strtoll(ptr, NULL, 10);
	return size * page;
}
#elif defined(USE_TASKINFO)
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <mach/task.h>
#include <mach/mach_init.h>

uint64_t total_mem() {
	task_t task = MACH_PORT_NULL;
	struct task_basic_info t_info;
	mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;

	if (task_for_pid(current_task(), getpid(), &task) != KERN_SUCCESS)
		return 0;
	task_info(task, TASK_BASIC_INFO, (task_info_t)&t_info, &t_info_count);

	return t_info.resident_size;
}
#else
uint64_t total_mem() {
	return _used_mem;
}
#endif




