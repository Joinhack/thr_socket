#ifndef THR_SVR_LOG_H
#define THR_SVR_LOG_H
#include "common.h"

enum LOG_LEVEL {
	LEVEL_TRACE=0,
	LEVEL_DEBUG,
	LEVEL_INFO,
	LEVEL_WARN,
	LEVEL_ERR
};

#ifdef __cplusplus
extern "C" {
#endif

void log_init(int fd);

void log_print(int level, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#define ERROR(fmt, ...) log_print(LEVEL_ERR, fmt, ##__VA_ARGS__)

#define INFO(fmt, ...) log_print(LEVEL_INFO, fmt, ##__VA_ARGS__)

#define WARN(fmt, ...) log_print(LEVEL_WARN, fmt, ##__VA_ARGS__)

#define DEBUG(fmt, ...) log_print(LEVEL_DEBUG, fmt, ##__VA_ARGS__)

#ifdef USE_TRACE
#define TRACE(fmt, ...) log_print(LEVEL_TRACE, fmt, ##__VA_ARGS__) 
#else
#define TRACE(fmt, ...)
#endif

#endif /**LOG_H*/
