#include "../testing/testing.h"
#include "../utils/hash.h"
#include "../codegen/codegen.h"
#include <stdbool.h>
#include <stdio.h>

// code generation tests
static UnitTest_t code_gen(void) {
	CodeGen codegen;
	init_CodeGen(&codegen);
	EMIT_SETUP(codegen);

	cstr EXPECTED =
		"int hello(void) {\n"
		"	printf(\"hello world\\n\");\n"
		"	return 5;\n"
		"}\n";

	CType type = make_ctype();
	add_name(&type, "int");
	ASSERT_STR(type.typename.str, "int", "typenames are not correct");

	EMIT_FUNC(hello, false, &type, 0, NULL);
	EMIT_CALL(printf, 1);
	EMIT_ATOMIC("hello world\n");
	EMIT_STATEMENT();
	EMIT_RETVAL(5);
	EMIT_SCOPE_END();

	smart_string generated = get_generated(&codegen);

	ASSERT_STR(generated.str, EXPECTED, "Generated code does not match expected");
	END_UNIT_TEST();
}

static UnitTest_t hash_map(void) {
	typedef struct {
		int a, b, c;
	} TestData;

	int a = 1;
	int b = 2;
	int c = 3;

	HashMap map = BUILD_MAP(TestData, NULL, NULL);

	set_pair(&map, &a, &(TestData) { 0, 0, 4 });
	set_pair(&map, &b, &(TestData) { 3, 0, 2 });
	set_pair(&map, &c, &(TestData) { 2, 9, 1 });

	ASSERT(GET_PAIR(TestData, &map, &a)->a == 0, "hash item does not match expected val");
	ASSERT(GET_PAIR(TestData, &map, &a)->b == 0, "hash item does not match expected val");
	ASSERT(GET_PAIR(TestData, &map, &a)->c == 4, "hash item does not match expected val");

	ASSERT(GET_PAIR(TestData, &map, &b)->a == 3, "hash item does not match expected val");
	ASSERT(GET_PAIR(TestData, &map, &b)->b == 0, "hash item does not match expected val");
	ASSERT(GET_PAIR(TestData, &map, &b)->c == 2, "hash item does not match expected val");

	ASSERT(GET_PAIR(TestData, &map, &c)->a == 2, "hash item does not match expected val");
	ASSERT(GET_PAIR(TestData, &map, &c)->b == 9, "hash item does not match expected val");
	ASSERT(GET_PAIR(TestData, &map, &c)->c == 1, "hash item does not match expected val");

	free_map(&map);

	END_UNIT_TEST();
}

#ifdef UNIT_TEST
MAKE_TEST general_unit_tests(void) {
	setupUnitTests();
	ADD_TEST(code_gen);
	ADD_TEST(hash_map);
}
#endif
