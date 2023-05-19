#include "../testing/testing.h"
#include "lexer.h"
#include "parse.h"
#include "passes.h"
#include "../strutils/str.h"
#include "../codegen/codegen.h"
#include <stdio.h>

#ifdef UNIT_TEST
static UnitTest_t scanTest(void) {
	smart_string rule = init_str("rule -> ( root | not ) * ( 'ID' ) ? /[0-9]/ ;");
	int save_alloc = allocated;

	Scan sc;
	scan(&rule, &sc);
	ASSERT(free_scan(&sc) == true, "Failed to free scan");

	ASSERT(save_alloc = allocated, "Allocations did not line up");

	END_UNIT_TEST();
}

static UnitTest_t parseTest(void) {
	smart_string rule = init_str("rule -> ( root | not ) * ( 'ID' ) ? /[0-9]/ ;");
	int save_alloc = allocated;

	Scan sc;
	scan(&rule, &sc);

	Parse *p;
	parse(&sc, &p);

	ASSERT(free_scan(&sc) == true,      "Failed to free scan");
	ASSERT(free_parse(p)  == true,      "Failed to free parse");
	ASSERT(save_alloc     == allocated, "Allocations did not line up");

	END_UNIT_TEST();
}

static UnitTest_t commentTest(void) {
	smart_string rule = init_str(
			"rule -> ( root | not ) * ( 'ID' ) ? /[0-9]/ ;\n"
			"=== comment ===\n");

	Scan sc;
	scan(&rule, &sc);

	ASSERT(free_scan(&sc) == true, "Failed to free parse");

	END_UNIT_TEST();
}

MAKE_TEST unitTests(void) {
	setupUnitTests();
	ADD_TEST(scanTest);
	ADD_TEST(parseTest);
	ADD_TEST(commentTest);
}
#endif
