#pragma once

#define DOOBLE_IMPL
#include "../dooble.h"
#include "../internal.h"

typedef struct {
	VEC(Node *) ast_blocks;
	TypeTree    all_types;
	HashMap     symbol_table;
} Semantics;
