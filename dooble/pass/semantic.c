#include "internal.h"
#include <stddef.h>

typedef size_t          SymbolHash;
typedef VEC(SymbolHash) TopologicalOrder;

// WARN: poor name for the struct.
// 
// represents dependencies for symbols.
// ex: Hello :: A + B * C
// requires A, B, & C to be evaluated before Hello can.
//
// if A :: 0, B :: D, then D also gets added to the list
typedef struct {
	const char *name_ref; // string reference (does not free)
	typeid      type;
	bool        visited;           // if the node has been visited at all
	bool        active_visitation; // if the node is currently in the dfs stack
	u32         parent_count;

	VEC(SymbolHash) symbols; // links to the hash of strings
} Restrictions;

static void free_restriction(Restrictions *r) {
	if (r->symbols.cap != 0) {
		free(r->symbols.arr);
		r->symbols.arr = NULL;
		r->symbols.cap = 0;
		r->symbols.len = 0;
	}
}

Semantics init_semantics(void) {
	return (Semantics) {
		.ast_blocks = {
			.arr = make(Node *, 10),
			.len = 0,
			.cap = 10,
		},
		.all_types    = init_TypeTree(),
		.symbol_table = BUILD_MAP(Restrictions, hash_str, free_restriction),
	};
}

static void add_restriction(HashMap *symbols, size_t symbol_hash, string_t *dep) {
	Restrictions *r = GET_PAIR(Restrictions, symbols, dep->str);
	
	if (r->symbols.cap == 0) {
		r->symbols.arr = make(size_t, 3);
		r->symbols.len = 0;
		r->symbols.cap = 3;
	}

	EXTEND_ARR(size_t, r->symbols.arr, r->symbols.len, r->symbols.cap);
	r->symbols.arr[r->symbols.len++] = symbols->hashfn(dep->str);

	GET_PAIRH(Restrictions, symbols, symbol_hash)->parent_count++;
}

// visits a node to build all possible restrictions
static void visit_restrictions(HashMap *symbols, size_t symbol_hash, Node *n) {
	if (symbols == NULL || n == NULL) return;

	switch (n->tag) {
		case EX_BINOP:
			visit_restrictions(symbols, symbol_hash, n->binop.expra);
			visit_restrictions(symbols, symbol_hash, n->binop.exprb);
			break;
		case EX_UNARY:
			visit_restrictions(symbols, symbol_hash, n->unary.expr);
			break;
		case EX_CALL:
			// don't evaluate the parameters because it is not necessary for
			// type inference or compile time optimizations.
			// (unless I have op overloading)
			visit_restrictions(symbols, symbol_hash, n->call.caller);
			break;
		case EX_SUBMEMBER:
			visit_restrictions(symbols, symbol_hash, n->member.expr);
			break;
		case EX_LITERAL:
			if (n->literal.tag == LIT_IDENT) {
				add_restriction(symbols, symbol_hash, &n->literal.str);
			}
			break;

		default: break;
	}
}

static void add_symbols(Semantics *s, Node *block) {
	if (block->tag != EX_BLOCK) {
		error("cannot add symbols from non-block");
		return;
	}

	const Block *const block_ref = &block->block;

	for_range (i, block_ref->len) {
		if (block_ref->arr[i]->tag != EX_DECL
				|| !block_ref->arr[i]->declare.is_const)
		{
			continue;
		}

		Restrictions restricts = {
			.type     = block_ref->arr[i]->declare.type,
			.name_ref = block_ref->arr[i]->declare.name.str,

			.visited      = false,
			.parent_count = 0,
			.symbols      = {
				.arr = NULL,
				.cap = 0,
				.len = 0,
			},
		};

		set_pair(&s->symbol_table,
				block_ref->arr[i]->declare.name.str,
				&restricts);
	}
}

void add_semantic_info(Semantics *semantics, AstResult *result) {
	if (semantics == NULL || result == NULL) {
		PANIC("semantics or result is nil"); // panic works for now
	}

	if (result->err) {
		if (result->err & PARSE_ERR) {
			error("parse error");
			return;
		}
	}

	if (result->pool_size == 0) return;

	EXTEND_ARR(Node *,
			semantics->ast_blocks.arr,
			semantics->ast_blocks.len,
			semantics->ast_blocks.cap);

	add_symbols(semantics, result->pool);
	semantics->ast_blocks.arr[semantics->ast_blocks.len++] = result->pool;
	free(result->pool);
}

static bool topo_DFS_util(HashMap *symbols, Restrictions *r, TopologicalOrder *order);

static TopologicalOrder find_symbol_topological_order(Semantics *semantics) {
	TopologicalOrder order = {
		.arr = make(SymbolHash, semantics->symbol_table.table.len),
		.len = 0,
		.cap = semantics->symbol_table.table.len,
	};

	Restrictions *iter = HASH_ITER(Restrictions, &semantics->symbol_table, true);
	while (iter != NULL) {
		if (iter->parent_count == 0) {
			bool success = topo_DFS_util(&semantics->symbol_table, iter, &order);
			// cleanup
			if (!success) {
				free(order.arr);
				return (TopologicalOrder) {0};
			}
		}

		// not do {} while because true vs. false for symbols
		iter = HASH_ITER(Restrictions, &semantics->symbol_table, false);
	}

	return order;
}

static bool topo_DFS_util(HashMap *symbols, Restrictions *restricts, TopologicalOrder *order) {
	typedef struct {
		Restrictions *rtrct;
		size_t        count;
	} DFSWalkElement;

	const size_t    TABLE_LEN = symbols->table.len;
	DFSWalkElement *rec_stack = make(DFSWalkElement, TABLE_LEN); // probably way more hashes than the depth
	size_t          rec_size  = 0;                             	 // of each tree, but a dynamic array requires
																 // more allocations total so I don't know
													   			 // which I prefer

	rec_stack[rec_size++] = (DFSWalkElement) { restricts, 0 };

	for (int i = 0; true; i++) {
		if (i >= restricts->symbols.len) {
			if (rec_size == 0) break;

			restricts->active_visitation = false;

			rec_size--;
			restricts = rec_stack[rec_size].rtrct;
			i         = rec_stack[rec_size].count;
		}

		SymbolHash    hash  = restricts->symbols.arr[i];
		Restrictions *child = GET_PAIRH(Restrictions, symbols, hash);
		if (child == NULL) PANIC("dependant child does not exist");

		if (child->active_visitation) {
			error("circular variable dependency: %s referenced in %s",
					restricts->name_ref, child->name_ref);

			// need to reset visited before cross edges
			return false;
		}
		else if (!child->visited) {
			DYNAMIC_ASSERT(rec_size < TABLE_LEN, "rec_size is greater than expected");

			// move down recursion stack
			rec_size++;
			rec_stack[rec_size - 1].rtrct = child;
			rec_stack[rec_size - 1].count = i;

			DYNAMIC_ASSERT(order->len < order->cap,
					"order should not be able to insert more symbols than in table");
			order->arr[order->len++] = hash;

			child->visited           = true;
			child->active_visitation = true;
			i                        = 0;
			restricts                = child;
		}
	}

	return true;
}

// type inference pass
// resolve global types -> error on circular references

static typeid resolve_type(Node *expr, TypeTree *tree);

static typeid resolve_binop(BinOp *bin, TypeTree *tree) {
	typeid type_a = resolve_type(bin->expra, tree);
	typeid type_b = resolve_type(bin->exprb, tree);

	switch (bin->operator) {
		// boolean
		case DB_AND:
		case DB_OR:
			if (type_a != type_b) {
				error("mismatched types in expression");
				return VOID_ID;
			} else if (type_a != basic_type(tree, BOOL_INDEX)) {
				error("both sides of 'and' or 'or' are not boolean expressions");
				return VOID_ID;
			}
			return type_a;

		// equality
		case DB_IS:
		case DB_NOT:
			if (type_a != type_b) {
				error("mismatched types in expression");
				return VOID_ID;
			}
			return type_a;

		// number
		case DB_AMPER:
		case DB_BITOR:
		case DB_BITNOT:
		case DB_DOTDOT:
		case DB_LESS:
		case DB_LESSEQ:
		case DB_GREATER:
		case DB_GREATEREQ:
		case DB_STAR:
		case DB_PLUS:
		case DB_SLASH:
		case DB_MINUS:
			if (type_a != type_b) {
				error("mismatched types in expression");
				return VOID_ID;
			} else if (type_a != basic_type(tree, INT_INDEX)
					&& type_a != basic_type(tree, FLOAT_INDEX)
					&& type_a != basic_type(tree, DOOBLE_INDEX))
			{
				error("both sides of arithmetic expression are not number expressions");
				return VOID_ID;
			}
			return type_a;

		default:
			PANIC("some other token. figure out later");
			break;
	}

	return VOID_ID;
}

static typeid resolve_unary(Unary *unary, TypeTree *tree) {
	typeid type = resolve_type(unary->expr, tree);

	switch (unary->operator) {
		case DB_NOT:
			if (type != basic_type(tree, BOOL_INDEX)) {
				error("'not' operator must be followed by a boolean expression");
				return VOID_ID;
			}
			return type;
		case DB_MINUS:
			if (type != basic_type(tree, INT_INDEX)
					&& type != basic_type(tree, FLOAT_INDEX)
					&& type != basic_type(tree, DOOBLE_INDEX))
			{
				error("'not' operator must be followed by a boolean expression");
				return VOID_ID;
			}
			return type;
		case DB_STAR:
			return as_pointer(tree, type);
		case DB_AMPER:
			return as_address(tree, type);

		default:
			PANIC("some other token. figure out later");
			break;
	}

	return VOID_ID;
}

// TODO: finish this function. certain pass elements, such as evaluating
// struct members need to be completed first.
static typeid resolve_type(Node *expr, TypeTree *tree) {
	switch (expr->tag) { // should this only care about expressions?
		case EX_BINOP:
			return resolve_binop(&expr->binop, tree);
		case EX_UNARY:
			return resolve_unary(&expr->unary, tree);
		case EX_CALL:
			return resolve_type(expr->call.caller, tree);
		case EX_SUBMEMBER:
			PANIC("submember type inference is not available yet");
			break;
		case EX_FUNCTION:
			break;
		case EX_LITERAL:
			break;

		default:
			return VOID_ID;
	}

	return VOID_ID;
}

void semantic_pass(Semantics *semantics) {
}

void free_semantics(Semantics *semantics) {
	free(semantics->ast_blocks.arr);
	freetree(&semantics->all_types);
	free_map(&semantics->symbol_table);
}
