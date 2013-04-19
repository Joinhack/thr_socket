#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/time.h>
#include "log.h"
#include "lock.h"
#include "cio.h"

static const char* level_array[] = {
	"TRACE",
	"DEBUG",
	"INFO",
	"WARN",
	"ERR"
};

static int logfd;

//define use mutex for log lock.
static LOCK_T lock;

static int top_level = LEVEL_TRACE;

void log_init(int fd) {
	logfd = fd;
	LOCK_INIT(&lock);
}

void set_top_level(int level) {
	top_level = level;
}

static void now(char *buf, size_t len) {
	struct timeval now;
	time_t current_tv;
	char tbuf[128];
	memset(tbuf, 0, sizeof(tbuf));
	gettimeofday(&now, NULL);
	current_tv = now.tv_sec;
	strftime(tbuf, sizeof(tbuf),"%m/%d  %H:%M:%S.",localtime(&current_tv));
	strcat(tbuf, "%ld");
	snprintf(buf, len, tbuf, now.tv_usec/1000);
}

static void log_fmt(char *buf, size_t len, int level, const char *fmt, va_list arg_list) {
	char inner_fmt[65535], nowstr[128];
	memset(inner_fmt, 0, sizeof(inner_fmt));
	memset(nowstr, 0, sizeof(nowstr));
	now(nowstr, sizeof(nowstr));
	snprintf(inner_fmt, sizeof(inner_fmt),"%s[%s]:%s", nowstr, level_array[level], fmt);
	vsnprintf(buf, len, inner_fmt, arg_list);
}

static int log_write(int fd, char *ptr, size_t len) {
	int count = 0;
	int wn = 0;
	while(count < len ) {
		wn = write(fd, ptr, len - count);
		if(wn == 0) return count;
		if(wn == -1) return -1;
		ptr += wn;
		count += wn;
	}
	return count;
}

void log_print(int level, char *fmt, ...) {
	char buf[65535];
	if(level < top_level)
		return;
	memset(buf, 0, sizeof(buf));
	va_list arg_list;
	va_start(arg_list, fmt);
	log_fmt(buf, sizeof(buf), level, fmt, arg_list);
	va_end(arg_list);
	LOCK(&lock);
	log_write(logfd, buf, strlen(buf));
	UNLOCK(&lock);
}

