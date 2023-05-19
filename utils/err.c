#include "err.h"
#include "../testing/testing.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

static HANDLE h;
static WORD oldAttrs;

void init_error(void) {
	h = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO info;

	GetConsoleScreenBufferInfo(h, &info);
	oldAttrs = info.wAttributes;

	_error(LOG,  "Setup error handling");
	_error(WARN, "	Warning: ON");
	_error(ERR,  "	Error: ON\n");
}

void _error_file(ErrorLevel el,
				 const char	*errstr,
				 int 		line,
				 const char	*source, ...)
{
	SetConsoleTextAttribute(h, FOREGROUND_GREEN);
	const char *linestr = source;
	int lineCount = 0;

	unsigned int ln = strlen(source);
	for (int i = 0; i < ln; i++) {
		if (source[i] == '\n') {
			lineCount++;
		} else if (lineCount == line) {
			linestr = &source[i];
			break;
		}
	}

	printf("%04d\t", line);
	for (uint32_t i = 0; i < ln && linestr[i] != '\n'; i++) {
		putchar(linestr[i]);
	}

	SetConsoleTextAttribute(h, el);
	printf("\n\t");
	va_list args;
	va_start(args, source);
	vprintf(errstr, args);
	putchar('\n');
	SetConsoleTextAttribute(h, oldAttrs);
}

void _error(ErrorLevel el, const char *errstr, ...) {
	SetConsoleTextAttribute(h, el);
	va_list args;
	va_start(args, errstr);
	vprintf(errstr, args);
	putchar('\n');
	SetConsoleTextAttribute(h, oldAttrs);
}
