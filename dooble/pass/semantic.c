#include "internal.h"
#include <corecrt_memory.h>
#include <string.h>

// MARK: will provide a way to do global symbol lookup
[[maybe_unused]]
static HashMap module_info;

// MARK:
// =========================================================
// ===================== Semantic Pass =====================
// =========================================================

typedef VEC(SymbolHash) TopologicalOrder;

// represents dependencies for symbols.
// ex: Hello :: A + B * C
// requires A, B, & C to be evaluated before Hello can.
//
// if A :: 0, B :: D, then D also gets added to the list
typedef struct {
	// general symbol information
	const char *name_ref; // string reference (does not free)
	typeid      type;
	Node       *rvalue; // ref

	// info for sorting
	bool visited;           // if the node has been visited at all
	bool active_visitation; // if the node is currently in the dfs stack
	u32  parent_count;

	VEC(SymbolHash) symbols; // links to the hash of strings (dependencies)
} SymbolInfo;

static void free_symbolinfo(SymbolInfo *s) {
	if (s->symbols.cap != 0) {
		free(s->symbols.arr);
		s->symbols.arr = NULL;
		s->symbols.cap = 0;
		s->symbols.len = 0;
	}
}

Semantics init_semantics(void) {
	const size_t STACK_SIZE = sizeof(ScopeMap) + 50 * sizeof(ScopeItem);

	ScopeStack symbol_stack = {
		.cap    = STACK_SIZE,
		.excess = 0,
		.maps   = clean_malloc(STACK_SIZE),
		.top    = NULL,
	};
	symbol_stack.top = symbol_stack.maps;

	return (Semantics) {
		.ast_blocks = {
			.arr = make(Node *, 10),
			.len = 0,
			.cap = 10,
		},
		.all_types    = init_TypeTree(),
		.symbol_table = BUILD_MAP(SymbolInfo, hash_str, free_symbolinfo),
		.symbol_stack = symbol_stack,
	};
}

static void add_symbol_dep(HashMap *symbols, SymbolInfo *symbol_info, string_t *dep) {
	SymbolInfo *s = GET_PAIR(SymbolInfo, symbols, dep->str);

	if (s == NULL) {
		set_pair(symbols, dep->str, &(SymbolInfo) {0});
		s = GET_PAIR(SymbolInfo, symbols, dep->str);
	}

	if (s->symbols.cap == 0) {
		s->symbols.arr = make(size_t, 3);
		s->symbols.len = 0;
		s->symbols.cap = 3;
	}

	EXTEND_ARR(size_t, s->symbols.arr, s->symbols.len, s->symbols.cap);
	s->symbols.arr[s->symbols.len++] = symbols->hashfn(dep->str);

	symbol_info->parent_count++;
}

// visits a node to build all possible restrictions
static void visit_symbol_deps(HashMap *symbols, SymbolInfo *symbol_info, Node *n) {
	if (symbols == NULL || symbol_info == NULL || n == NULL) return;

	// does not visit function literal because infinite loops and recursion
	// makes sense in that context
	switch (n->tag) {
		case EX_BINOP:
			visit_symbol_deps(symbols, symbol_info, n->binop.expra);
			visit_symbol_deps(symbols, symbol_info, n->binop.exprb);
			break;
		case EX_UNARY:
			visit_symbol_deps(symbols, symbol_info, n->unary.expr);
			break;
		case EX_CALL:
			// don't evaluate the parameters because it is not necessary for
			// type inference or compile time optimizations.
			// (unless I have op overloading)
			visit_symbol_deps(symbols, symbol_info, n->call.caller);
			break;
		case EX_SUBMEMBER:
			visit_symbol_deps(symbols, symbol_info, n->member.expr);
			break;
		case EX_LITERAL:
			if (n->literal.tag == LIT_IDENT) {
				add_symbol_dep(symbols, symbol_info, &n->literal.str);
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

		SymbolInfo symbol_info = {
			.type     = block_ref->arr[i]->declare.type,
			.name_ref = block_ref->arr[i]->declare.name.str,
			.rvalue   = block_ref->arr[i]->declare.assign,

			.visited      = false,
			.parent_count = 0,
			.symbols      = {
				.arr = NULL,
				.cap = 0,
				.len = 0,
			},
		};

		visit_symbol_deps(&s->symbol_table, &symbol_info, block_ref->arr[i]);
		set_pair(&s->symbol_table,
				block_ref->arr[i]->declare.name.str,
				&symbol_info);
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

	// add the first element of the pool (result->pool == &result->pool[0])
	add_symbols(semantics, &result->pool[0]);
	semantics->ast_blocks.arr[semantics->ast_blocks.len++] = result->pool;
	free(result->pool);
}

static bool topo_DFS_util(HashMap *symbols, SymbolInfo *s, TopologicalOrder *order);

static TopologicalOrder find_symbol_topological_order(Semantics *semantics) {
	TopologicalOrder order = {
		.arr = make(SymbolHash, semantics->symbol_table.table.len),
		.len = 0,
		.cap = semantics->symbol_table.table.len,
	};

	SymbolInfo *iter = HASH_ITER(SymbolInfo, &semantics->symbol_table, true);
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
		iter = HASH_ITER(SymbolInfo, &semantics->symbol_table, false);
	}

	return order;
}

static bool topo_DFS_util(HashMap *symbols, SymbolInfo *info, TopologicalOrder *order) {
	typedef struct {
		SymbolInfo *info;
		size_t      count;
	} DFSWalkElement;

	const size_t    TABLE_LEN = symbols->table.len;
	DFSWalkElement *rec_stack = make(DFSWalkElement, TABLE_LEN); // probably way more hashes than the depth
	size_t          rec_size  = 0;                             	 // of each tree, but a dynamic array requires
																 // more allocations total so I don't know
													   			 // which I prefer

	rec_stack[rec_size++] = (DFSWalkElement) { info, 0 };

	for (int i = 0; true; i++) {
		if (i >= info->symbols.len) {
			if (rec_size == 0) break;

			info->active_visitation = false;

			rec_size--;
			info = rec_stack[rec_size].info;
			i    = rec_stack[rec_size].count;
		}

		SymbolHash  hash  = info->symbols.arr[i];
		SymbolInfo *child = GET_PAIRH(SymbolInfo, symbols, hash);
		if (child == NULL) PANIC("dependant child does not exist");

		if (child->active_visitation) {
			error("circular variable dependency: %s referenced in %s",
					info->name_ref, child->name_ref);

			// need to reset visited before cross edges
			return false;
		}
		else if (!child->visited) {
			DYNAMIC_ASSERT(rec_size < TABLE_LEN, "rec_size is greater than expected");

			// move down recursion stack
			rec_size++;
			rec_stack[rec_size - 1].info = child;
			rec_stack[rec_size - 1].count = i;

			DYNAMIC_ASSERT(order->len < order->cap,
					"order should not be able to insert more symbols than in table");
			order->arr[order->len++] = hash;

			child->visited           = true;
			child->active_visitation = true;
			i                        = 0;
			info                = child;
		}
	}

	return true;
}

// type inference pass
// resolve global types -> error on circular references

static void push_scope(ScopeStack *stack) {
	const size_t minimum_excess =
		sizeof(ScopeMap) +
		sizeof(ScopeItem) * stack->top->cap;

	if (stack->excess < minimum_excess) {
		size_t growth_size = minimum_excess + sizeof(ScopeItem) * stack->top->cap;
		size_t new_cap     = stack->cap + growth_size;

		// realloc?
		ScopeMap *new_maps = clean_malloc(new_cap);
		memmove_s(new_maps, new_cap, stack->maps, stack->cap);
		free(stack->maps);

		ptrdiff_t top_diff = stack->top - stack->maps;

		stack->excess = minimum_excess;
		stack->maps   = new_maps;
		stack->cap    = new_cap;
		stack->top    = ((void *) stack->maps) + (stack->cap - growth_size);

		// set new map up
		*stack->top = (ScopeMap) {
			.cap  = minimum_excess,
			.full = 0,
			.prev = ((void *) stack->maps) + top_diff,
			.data = {},
		};
	}
	else {
		ScopeMap *old_top = stack->top;

		stack->excess -= minimum_excess;
		stack->top     = (ScopeMap *) &stack->top->data[stack->top->cap];

		*stack->top = (ScopeMap) {
			.cap  = minimum_excess,
			.full = 0,
			.prev = old_top,
			.data = {},
		};
	}
}

static void pop_scope(ScopeStack *stack) {
	stack->top = stack->top->prev;
}

// rescales hashmap to a new hashmap
static void rescale(ScopeItem from[], size_t from_size, ScopeItem to[], size_t to_size) {
	for_range (i, from_size) {
		const typeid TYPE = from[i].type;
		const size_t HASH = from[i].hash;
		if (HASH == 0) continue;

		to[HASH % to_size].hash = HASH;
		to[HASH % to_size].type = TYPE;
	}
}

static bool insert_symbol(ScopeStack *stack, const SymbolHash hash, const typeid type) {
	ScopeMap  *map     = stack->top;
	size_t     address = hash % map->cap;
	ScopeItem *item    = &map->data[address];

	// resize stack
	const float  load          = (float) map->full / (float) map->cap;
	const size_t resize_amount = map->cap * 2 + map->full;

	// includes interim buffer
	const size_t resized_cap = resize_amount * sizeof(ScopeItem);

	if (load > HASH_MAX_LOAD && resized_cap <= stack->excess) {
		size_t new_cap = map->cap * 2;

		// move elements to secondary buffer
		ScopeItem *interim_buffer = &map->data[map->cap];
		size_t     interim_count  = 0;
		for_range (i, map->cap) where (map->data[i].hash != 0) {
			interim_buffer[interim_count++] = map->data[i];
		}

		rescale(interim_buffer, map->full, map->data, new_cap);

		stack->excess -= new_cap * sizeof(ScopeItem);
		map->cap       = new_cap;
	}
	else if (load > HASH_MAX_LOAD) {
		ScopeMap *old_maps = stack->maps;
		size_t    old_cap  = stack->cap;
		size_t    new_cap  = old_cap
			- stack->excess
			+ resize_amount * sizeof(ScopeItem);

		ScopeMap *new_maps = clean_malloc(new_cap);
		memmove_s(new_maps, new_cap, old_maps, old_cap);

		ScopeMap  *old_top  = stack->top;
		ptrdiff_t  top_diff = old_top - old_maps;
		ScopeMap  *new_top  = ((void *) old_maps) + top_diff;

		rescale(map->data, map->cap, new_top->data, resize_amount);

		// end
		free(old_maps);
		stack->maps   = new_maps;
		stack->top    = new_top;
		stack->cap    = new_cap;
		stack->excess = 0;
		map           = new_top;
		map->cap      = resize_amount;
	}

	if (item->hash == 0) {
		item->hash = hash;
		item->type = type;
		return true;
	}

	// open addressing style
	loop {
		address = (address + 1) % map->cap;
		if (map->data[address].hash == 0) {
			item       = &map->data[address];
			item->hash = hash;
			item->type = type;
			return true;
		}
	}
}

static typeid get_scoped_symbol_type(ScopeStack *stack, const SymbolHash hash) {
	ScopeMap *map     = stack->top;
	size_t    address = hash % map->cap;

	while (map != NULL) {
		loop {
			if (map->data[address].hash == hash) {
				return map->data[address].type;
			} else if (map->data[address].type == VOID_ID) {
				break;
			}

			address = (address + 1) % map->cap;
		}

		map = map->prev;
	}

	return VOID_ID;
}

static typeid resolve_type(Node *expr, Semantics *semantics);

static typeid resolve_fntype(Function *func, Semantics *semantics) {
	TypeLeaf fn = {
		.tag = DBLTP_FN,
		.fn  = {
			.arr = make(typeid, 4),
			.len = 0,
			.cap = 4,
		},
	};

	for_range (i, func->args.len) {
		EXTEND_ARR(typeid, fn.fn.arr, fn.fn.len, fn.fn.cap);
		fn.fn.arr[fn.fn.len++] = resolve_type(func->args.arr[i], semantics);
	}

	return get_leaf(&semantics->all_types, NULL, &fn);
}

static typeid resolve_binop(BinOp *bin, Semantics *semantics) {
	typeid type_a = resolve_type(bin->expra, semantics);
	typeid type_b = resolve_type(bin->exprb, semantics);

	switch (bin->operator) {
		// boolean
		case DB_AND:
		case DB_OR:
			if (type_a != type_b) {
				error("mismatched types in expression");
				return VOID_ID;
			} else if (type_a != basic_type(&semantics->all_types, BOOL_INDEX)) {
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
			} else if (type_a != basic_type(&semantics->all_types, INT_INDEX)
					&& type_a != basic_type(&semantics->all_types, FLOAT_INDEX)
					&& type_a != basic_type(&semantics->all_types, DOOBLE_INDEX))
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

static typeid resolve_unary(Unary *unary, Semantics *semantics) {
	typeid type = resolve_type(unary->expr, semantics);

	switch (unary->operator) {
		case DB_NOT:
			if (type != basic_type(&semantics->all_types, BOOL_INDEX)) {
				error("'not' operator must be followed by a boolean expression");
				return VOID_ID;
			}
			return type;
		case DB_MINUS:
			if (type != basic_type(&semantics->all_types, INT_INDEX)
					&& type != basic_type(&semantics->all_types, FLOAT_INDEX)
					&& type != basic_type(&semantics->all_types, DOOBLE_INDEX))
			{
				error("'not' operator must be followed by a boolean expression");
				return VOID_ID;
			}
			return type;
		case DB_STAR:
			return as_pointer(&semantics->all_types, type);
		case DB_AMPER:
			return as_address(&semantics->all_types, type);

		default:
			PANIC("some other token. figure out later");
			break;
	}

	return VOID_ID;
}

// IMPORTANT: should this be a basically functional procedure, with the only state change
// being to add to the type tree?
// If yes, then that affects how I approach things in verify_types, especially with Function

// TODO: finish this function. certain pass elements, such as evaluating
// struct members need to be completed first.

// NOTE: resolve type needs to perform the function of making sure typedefs exist for identifiers
static typeid resolve_type(Node *expr, Semantics *semantics) {
	switch (expr->tag) { // should this only care about expressions?
		case EX_BINOP:
			return resolve_binop(&expr->binop, semantics);
		case EX_UNARY:
			return resolve_unary(&expr->unary, semantics);
		case EX_CALL:
			// FIXME: does this make any sense? don't I need to figure out the
			// return value?
			return resolve_type(expr->call.caller, semantics);
		case EX_SUBMEMBER:
			PANIC("submember type inference is not available yet");
			break;

		case EX_FUNCTION:
			return resolve_fntype(&expr->function, semantics);

		case EX_LITERAL:
			if (expr->literal.tag == LIT_IDENT) {
				char      *str        = expr->literal.str.str;
				size_t     size       = expr->literal.str.size;
				SymbolHash ident_hash = hash_str(str, size);

				return get_scoped_symbol_type(&semantics->symbol_stack, ident_hash);
			}

			else return basic_type(&semantics->all_types,
					expr->literal.tag == LIT_STR  ? STRING_INDEX :
					expr->literal.tag == LIT_BOOL ? BOOL_INDEX   :
					expr->literal.tag == LIT_NUM  ? INT_INDEX    :
					expr->literal.tag == LIT_FLT  ? DOOBLE_INDEX : NULL_INDEX);

		default: return VOID_ID;
	}

	return VOID_ID;
}


static bool verify_types(Semantics *semantics, Node *expr);

void semantic_pass(Semantics *semantics) {
	if (semantics->ast_blocks.len == 0) return;

	TopologicalOrder symbol_eval_order = find_symbol_topological_order(semantics);
	if (symbol_eval_order.arr == NULL) {
		PANIC("could not find symbol order and I don't want to deal w/ the error now");
	}

	// TODO: order for full semantic information
	// 1. evaluate type definitions (aliases, structs, etc.)                            :: done
	// 2. evaluate types for global symbols                                             :: done
	// 3. evaluate types for elements in blocks (should code be allowed outside funcs?) :: not
	// 4. prove types exist                                                             :: not
	// 5. perform type checks                                                           :: not
	// 6. anything that might pop up when writing the compiler pass                     :: ???

	// evaluate types for global symbols
	for_range (i, symbol_eval_order.len) {
		SymbolInfo *symbol = GET_PAIRH(SymbolInfo,
				&semantics->symbol_table,
				symbol_eval_order.arr[i]);

		if (symbol->type == VOID_ID) {
			typeid type = resolve_type(symbol->rvalue, semantics);
			if (type == VOID_ID) {
				error("symbol %s cannot have a type of 'void'", symbol->name_ref);
				continue;
			}

			symbol->type = type;
		}
	}

	// go through every declaration to resolve all symbols
	if (!verify_types(semantics, semantics->ast_blocks.arr[0])) {
		error("types do are not consistant");
		return;
	}
}

// NOTE: the type tree will pretend that types that consist of identifiers always exist
// even when there is no declaration or alias.
// To combat this I need to check if every symbol has a type alias, unless it is a primative

// verifies and adds all type info within any node type.
static bool verify_types(Semantics *semantics, Node *expr) {
	switch (expr->tag) {
		case EX_IF:
		{
			bool valid_cond = verify_types(semantics, expr->ifstmt.condition);
			bool valid_stmt = verify_types(semantics, expr->ifstmt.stmt);
			bool valid_else = expr->ifstmt.else_case != NULL &&
				verify_types(semantics, expr->ifstmt.else_case);

			return valid_cond && valid_stmt && valid_else;
		}

		case EX_FORWHILE:
		case EX_DOWHILE:
		case EX_DONTWHILE:
		{
			bool valid_cond = verify_types(semantics, expr->forwhile.condition);
			bool valid_stmt = verify_types(semantics, expr->forwhile.stmt);
			return valid_cond && valid_stmt;
		}

		case EX_FOREACH:
		case EX_DOEACH:
		case EX_DONTEACH:
		{
			if (expr->foreach.range->tag != EX_BINOP) return false;
			BinOp *range = &expr->foreach.range->binop;
			if (range->operator != DB_DOTDOT) return false;

			bool valid_stmt  = verify_types(semantics, expr->foreach.stmt);
			bool valid_range = verify_types(semantics, expr->foreach.range);
			return valid_stmt && valid_range;
		}

		case EX_BLOCK:
			push_scope(&semantics->symbol_stack);
			for_range (i, expr->block.len) {
				if (!verify_types(semantics, expr->block.arr[i])) {
					return false;
				}
			}
			pop_scope(&semantics->symbol_stack);
			return true;

		case EX_DECL:
		{
			Declaration *decl = &expr->declare;
			typeid       type = resolve_type(decl->assign, semantics);
			SymbolHash   hash = hash_str(decl->name.str, decl->name.size);

			insert_symbol(&semantics->symbol_stack, hash, type);
			return verify_types(semantics, decl->assign);
		}

		// expressons
		case EX_FUNCTION: // does this *really* apply to `Function`s?
			verify_types(semantics, expr->function.block);
			fallthrough;
		case EX_BINOP:
		case EX_UNARY:
		case EX_CALL:
		case EX_SUBMEMBER:
			return resolve_type(expr, semantics) != VOID_ID;

		// single units cannot violate type
		case EX_LITERAL:
		case EX_PASS:
			return true;
	}
}

void free_semantics(Semantics *semantics) {
	free(semantics->ast_blocks.arr);
	semantics->ast_blocks.arr = NULL;
	semantics->ast_blocks.cap = 0;
	semantics->ast_blocks.len = 0;

	freetree(&semantics->all_types);
	free_map(&semantics->symbol_table);

	free(semantics->symbol_stack.maps);
	semantics->symbol_stack.maps   = NULL;
	semantics->symbol_stack.top    = NULL;
	semantics->symbol_stack.cap    = 0;
	semantics->symbol_stack.excess = 0;
}
