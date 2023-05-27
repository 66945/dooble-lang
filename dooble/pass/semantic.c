#include "internal.h"

// WARN: poor name for the struct.
// 
// represents dependencies for symbols.
// ex: Hello :: A + B * C
// requires A, B, & C to be evaluated before Hello can.
//
// if A :: 0, B :: D, then D also gets added to the list
typedef struct {
	typeid        type;
	u8            remaining; // ??? figure out how close
							 // to evaluated each symbol is

	// PERF: I could just store the hash for each symbol
	VEC(unsigned long long) symbols;
} Restrictions;

static void free_restriction(Restrictions *r) {
	if (r->type == NULL) {
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

// visit for restrictions
static void add_restriction(Restrictions *r, string_t *name) {
	EXTEND_ARR(string_t, r->symbols.arr, r->symbols.len, r->symbols.cap);
	r->symbols.arr[r->symbols.len++] = hash_str(name->str, name->size);
}

static void visit_restrictions(Restrictions *r, Node *n) {
	if (r == NULL || n == NULL) return;

	switch (n->tag) {
		case EX_BINOP:
			visit_restrictions(r, n->binop.expra);
			visit_restrictions(r, n->binop.exprb);
			break;
		case EX_UNARY:
			visit_restrictions(r, n->unary.expr);
			break;
		case EX_CALL:
			// don't evaluate the parameters because it is not necessary for
			// type inference or compile time optimizations.
			// (unless I have op overloading)
			visit_restrictions(r, n->call.caller);
			break;
		case EX_SUBMEMBER:
			visit_restrictions(r, n->member.expr);
			break;
		case EX_LITERAL:
			if (n->literal.tag == LIT_IDENT) {
				add_restriction(r, &n->literal.str);
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

		Restrictions restricts = { .type = block_ref->arr[i]->declare.type };

		if (restricts.type == NULL) {
			restricts.symbols.arr = make(unsigned long long, 2);
			restricts.symbols.len = 0;
			restricts.symbols.cap = 2;
			visit_restrictions(&restricts, block_ref->arr[i]->declare.assign);
		}

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

// type inference pass
// resolve global types -> error on circular references

void free_semantics(Semantics *semantics) {
	free(semantics->ast_blocks.arr);
	freetree(&semantics->all_types);
	free_map(&semantics->symbol_table);
}
