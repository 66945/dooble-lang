#include "internal.h"
#include "../utils/input.h"
#include "../utils/utils.h"
#include "type.h"

static UnitTest_t lexer_test(void) {
	cstr buffer =
		"my_func :: (a: int, b: int) int {\n"
		"	return a + b\n"
		"}\n";

	DoobleToken *tokens;
	u32 len = get_tokens(buffer, &tokens);
	ASSERT(len == 22, "lexed tokens did not meet expected length");

	free_tokens(len, tokens);
	END_UNIT_TEST();
}

static UnitTest_t lexer_num_test(void) {
	cstr buffer =
		"1293342\t"
		"123_45.6\t"
		"0xFF_00_00\t"
		"0b101010\n";

	DoobleToken *tokens;
	u32 len = get_tokens(buffer, &tokens);
	ASSERT(len == 6, "lexed tokens did not meet expected length");

	ASSERT(tokens[0].token == DB_NUM,   "token is not an integer");
	ASSERT(tokens[1].token == DB_FLOAT, "token is not an float");
	ASSERT(tokens[2].token == DB_NUM,   "token is not an integer");

	ASSERT(tokens[0].vali == 1293342,  "number does not hold expected value");
	ASSERT_FLT(tokens[1].valf, 12345.6, "number does not hold expected value");
	ASSERT(tokens[2].vali == 0xFF0000, "number does not hold expected value");
	ASSERT(tokens[3].vali == 0b101010, "number does not hold expected value");

	free_tokens(len, tokens);
	END_UNIT_TEST();
}

static UnitTest_t num_greedy_test(void) {
	cstr buffer = "1,2\n";

	DoobleToken *tokens;
	u32 len = get_tokens(buffer, &tokens);

	ASSERT(len == 5, "number has consumed additional token");

	free_tokens(len, tokens);
	END_UNIT_TEST();
}

static UnitTest_t parse_call_test(void) {
	cstr buffer = "hello_world(1,2,)(3)(4,5)(6,).hi(7,8,9)\n";

	DoobleToken *tokens = NULL;
	u32          len    = get_tokens(buffer, &tokens);
	TypeTree     tree   = init_TypeTree();
	AstResult    ast    = get_ast(len, tokens, &tree, buffer);

	print_ast(ast.pool);
	free_ast(ast.pool_size, ast.pool);
	freetree(&tree);

	END_UNIT_TEST();
}

static UnitTest_t parse_test(void) {
	cstr buffer = "(1 + 2) * -3 >= 1 and hi(1, 2,)()(1, 3) is not false\n";

	DoobleToken *tokens = NULL;
	u32          len    = get_tokens(buffer, &tokens);
	TypeTree     tree   = init_TypeTree();
	AstResult    ast    = get_ast(len, tokens, &tree, buffer);

	// print_tokens(len, tokens);
	print_ast(ast.pool);
	free_ast(ast.pool_size, ast.pool);
	freetree(&tree);

	END_UNIT_TEST();
}

static UnitTest_t parse_fn(void) {
	cstr buffer = "func :: () {}\n";

	DoobleToken *tokens = NULL;
	u32          len    = get_tokens(buffer, &tokens);
	TypeTree     tree   = init_TypeTree();
	AstResult    ast    = get_ast(len, tokens, &tree, buffer);

	print_ast(ast.pool);
	free_ast(ast.pool_size, ast.pool);
	freetree(&tree);

	END_UNIT_TEST();
}

#ifdef UNIT_TEST
MAKE_TEST dooble_tests(void) {
	setupUnitTests();
	ADD_TEST(lexer_test);
	ADD_TEST(lexer_num_test);
	ADD_TEST(num_greedy_test);
	ADD_TEST(parse_call_test);
	ADD_TEST(parse_test);
	ADD_TEST(parse_fn);
}
#endif
