#include "strutils/str.h"
#include "testing/testing.h"
#include "utils/err.h"
#include "testing/logging.h"
#include "utils/utils.h"
#include <stdio.h>
#include <string.h>

#ifndef DEPLICATE
#define DEPLICATE 0
#endif

// NOTE: for now I am working on the actual dooble language, not deplicate.
// While working on deplicate would be a lot of fun, I don't want to spend
// all of my time working on what was supposed to be a weeklong project.
// It was worth it, because of the tooling I produced to work on deplicate
// as well as the experience I gained. I still want to complete deplicate at
// some point.
// -------------------------------------------------------------------------
// I always have the option of continuing development on the codegen engine
// and then finishing a deplicate prototype.

int main(int argc, char **argv) {
	if (DEPLICATE) {
		printf(	" ____             _ _           _       \n"
				"|  _ \\  ___ _ __ | (_) ___ __ _| |_ ___ \n"
				"| | | |/ _ \\ '_ \\| | |/ __/ _` | __/ _ \\\n"
				"| |_| |  __/ |_) | | | (_| (_| | ||  __/\n"
				"|____/ \\___| .__/|_|_|\\___\\__,_|\\__\\___|\n"
				"           |_|                          \n");
	} else {
		printf( "________               ___.   .__          \n"
				"\\______ \\   ____   ____\\_ |__ |  |   ____  \n"
				" |    |  \\ /  _ \\ /  _ \\| __ \\|  | _/ __ \\ \n"
				" |    `   (  <_> |  <_> ) \\_\\ \\  |_\\  ___/ \n"
				"/_______  /\\____/ \\____/|___  /____/\\___  >\n"
				"        \\/                  \\/          \\/ \n");
	}

	OPENLOG();
	INIT_MEMORY_TESTS();
	init_error();

	bool unit_test_arg = argc > 1 && (strcmp(argv[1], "unit_test") == 0);
#	ifdef DEBUGGER
#		define DEBUGGER_BOOL 1
#	else
#		define DEBUGGER_BOOL 0
#	endif

	if (UNIT_TEST && (unit_test_arg || DEBUGGER_BOOL)) {
		unitTestEntry();
	}

	END_MEMORY_TESTS();
	ENDLOG();
	return 0;
}
