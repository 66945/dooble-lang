#include "logging.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <time.h>

static FILE   *logFile;
static time_t  prevtime = 0;

void openLog(const char *file) {
	fopen_s(&logFile, file, "a"); // handle err code
	fputs("===== STARTING NEW LOG SESSION =====\n", logFile);
}

void writeLine(const char *str, ...) {
	time_t rawtime;
	time(&rawtime);
	va_list args;

	va_start(args, str);
	size_t formattedLen = vsnprintf(NULL, 0, str, args);

	va_start(args, str);
	char formatted[formattedLen];
	vsprintf_s(formatted, formattedLen, str, args);

	if(rawtime - prevtime > 1) {
		struct tm timeinfo;
		char buf[26];

		localtime_s(&timeinfo, &rawtime);
		asctime_s(buf, 26, &timeinfo);
		fprintf(logFile, "%s\t%s\n", buf, formatted);

		prevtime = rawtime;
	} else {
		fprintf(logFile, "\t%s\n", formatted);
	}

	va_end(args);
}

void closeLog(void) {
	fclose(logFile);
}
