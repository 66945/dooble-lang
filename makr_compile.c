#define MAKR_IMPL
#include "makr/makr.h"

#define TESTING   "testing/testing.c", "testing/logging.c"
#define STR_UTILS "strutils/str.c", "strutils/template/template.c"
#define UTILS     "utils/err.c", "utils/file.c", "utils/hash.c", "utils/input.c"

#define C_GEN "codegen/codegen.c"

#define DEPLICATE            \
	"deplicate/lexer.c",     \
	"deplicate/parse.c",     \
	"deplicate/printpass.c", \
	"deplicate/pass.c",      \
	"deplicate/compile.c",   \
	"deplicate/tests.c"

#define DOOBLE            \
	"dooble/lexer.c",     \
	"dooble/parse.c",     \
	"dooble/print_ast.c", \
	"dooble/tests.c",     \
	"dooble/type.c"

#define WARNINGS                   \
	"-Wconditional-uninitialized", \
	"-Wmissing-prototypes",        \
	"-Wimplicit-fallthrough",      \
	"-Wcomma",                     \
	"-Wunreachable-code-break",    \
	"-Wall"

#define EXTENSIONS "-fblocks"

#define DEBUG_SYMBOLS "-g -O0 -fuse-ld=lld -DDEBUGGER"

#define WRAP    "-Wl,--wrap=malloc,--wrap=calloc,--wrap=free"
#define VERSION "-std=c2x"

struct {
	bool deplicate: 1;
	bool unit_test: 1;
	bool release:   1;
	bool run:       1;
	bool debug:     1;
} build_flags;

void build(int argc, char *argv[]) {
	FOR_FLAGS {
		CHECK_FLAG(build_flags, deplicate);
		CHECK_FLAG(build_flags, unit_test);
		CHECK_FLAG(build_flags, release);
		CHECK_FLAG(build_flags, run);
		CHECK_FLAG(build_flags, debug);
	}

	ADD_FILES("main.c");

	// maybe
	ADD_FILES("tests/tests.c");

	ADD_FILES(TESTING, UTILS, STR_UTILS);
	ADD_FILES(C_GEN);
	ADD_FLAGS(WARNINGS, EXTENSIONS, VERSION);

	if (build_flags.deplicate) {
		ADD_FILES(DEPLICATE);
		ADD_FLAGS("-DDEPLICATE");
	} else {
		ADD_FILES(DOOBLE);
	}

	if (build_flags.debug) ADD_FLAGS(DEBUG_SYMBOLS);

	if      (build_flags.unit_test) RUN("unit_test");
	else if (build_flags.run)       RUN("");
}
