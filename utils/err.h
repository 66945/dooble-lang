#pragma once

#include "../strutils/str.h"

typedef enum {
	LOG  = 0b111, // blue
	WARN = 0b110, // yellow
	ERR  = 0b100, // red
} ErrorLevel;

void init_error(void);

void _error_file(ErrorLevel el, const char *errstr, int line, const char *source, ...);
void _error(ErrorLevel el, const char *errstr, ...);

#define logvar(var) _error(LOG,                 \
		_Generic((var),                         \
			int:                #var ": %d",    \
			long long:          #var ": %lld",  \
			unsigned long long: #var ": %zu",   \
			char *:             #var ": '%s'",  \
			void *:             #var ": 0x%p"), \
		var)

#define logger(errstr, ...) _error(LOG,  errstr __VA_OPT__(,) __VA_ARGS__)
#define warn(errstr, ...)   _error(WARN, errstr __VA_OPT__(,) __VA_ARGS__)
#define error(errstr, ...)  _error(ERR,  errstr __VA_OPT__(,) __VA_ARGS__)

#define log_file(errstr, line, source, ...) \
	_error_file(WARN, errstr, line, source, __VA_ARGS__)
#define warn_file(errstr, line, source, ...) \
	_error_file(WARN, errstr, line, source, __VA_ARGS__)
#define error_file(errstr, line, source, ...) \
	_error_file(ERR, errstr, line, source, __VA_ARGS__)
