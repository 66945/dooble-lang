#pragma once

#define DOOBLE_IMPL
#include "../dooble.h"
#include "../internal.h"

typedef struct {
	VEC(Node *) ast_blocks;
	TypeTree    all_types;
	HashMap     symbol_table;
} Semantics;

Semantics init_semantics(void);
void      add_semantic_info(Semantics *semantics, AstResult *result); // NOTE: should support multithreading
void      free_semantics(Semantics *semantics);

/* multithreading on windows, this should be fun ...
 * */
