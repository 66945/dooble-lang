#include "internal.h"

Semantics init_semantics(void) {
	return (Semantics) {
		.ast_blocks = {
			.arr = make(Node *, 10),
			.len = 0,
			.cap = 10,
		},
		.all_types    = init_TypeTree(),
		.symbol_table = BUILD_MAP(string_t, (hash_t) hash_str, (delete_t) freestr),
	};
}

void add_semantic_info(Semantics *semantics, AstResult *result) {

}

void free_semantics(Semantics *semantics) {
	free(semantics->ast_blocks.arr);
	freetree(&semantics->all_types);
	free_map(&semantics->symbol_table);
}
