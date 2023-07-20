#pragma once

#define DOOBLE_IMPL
#include "../dooble.h"
#include "../internal.h"

/* Symbol Resolution:
 * */

// hi: *Hi -> 'Hi' is a type on the tree

typedef size_t SymbolHash;

// NOTE: docs is the inspiration for this data structure, but should be updated

// should the global table be stored in the symbol_stack?
// the symbol stack does not need all the fancy SymbolInfo 
// that globals do before they get evaluated.
// SymbolInfo does not store type information, so it might be
// useful to store that in the symbol_stack to avoid cache miss,
// but it also seems pointless given how disparate the data here
// actually is.

// Ways to rethink this data structure:
// - Since symbols cannot be added to a scope map that isn't on
//   the top of the stack, it could be a contiguous block of
//   memory in a FILO stack that serves as blocks of hash maps.
//   [ ### ### ############## ### #################### ]
//     |   |   | map 1 data   |   | map 2 data
//     |   | map 1 info       | map 2 info
//     | struct info
typedef struct {
	SymbolHash hash;
	typeid     type;
} ScopeItem;

typedef struct ScopeMap ScopeMap;
struct ScopeMap {
	ScopeMap *prev;
	size_t    cap;
	size_t    full;
	ScopeItem data[];
};

typedef struct {
	size_t    cap; // represents the size in bytes
	size_t    excess; // excess memory after the top
	ScopeMap *maps; // vec
	ScopeMap *top;  // ref
} ScopeStack;

typedef struct {
	VEC(Node *) ast_blocks;
	TypeTree    all_types;
	HashMap     symbol_table; // for global symbol information, ordering related
	ScopeStack  symbol_stack;

	VEC(SymbolHash) export_symbols;
} Semantics;

Semantics init_semantics(void);
void      add_semantic_info(Semantics *semantics, AstResult *result); // multithreading options
void      semantic_pass(Semantics *semantics);
void      free_semantics(Semantics *semantics);

/* multithreading on windows, this should be fun ...
 * */
