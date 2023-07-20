#include "type.h"
#include <stdint.h>
#include <stdlib.h>
#define DOOBLE_IMPL
#include "dooble.h"
#include "internal.h"

// IMPORTANT: add documentation to the whole module

typedef struct {
	const char *const buffer;
	size_t            position;
	bool              parse_error; // TODO: implement parse error tracking

	struct {
		Node   *arr; // arr
		size_t  len;
		size_t  cap;
	} ast_pool;

	struct {
		DoobleToken *arr; // arr
		size_t       len;
	} tokens;

	TypeTree *type_tree;
} Parse;

static Node *append_node(Parse *p, Node *node) {
	EXTEND_ARR(Node, p->ast_pool.arr, p->ast_pool.len, p->ast_pool.cap);

	p->ast_pool.arr[p->ast_pool.len++] = *node;
	return &p->ast_pool.arr[p->ast_pool.len - 1];
}

static DoobleToken *advance(Parse *p) {
	if (p->position >= p->tokens.len) {
		return NULL;
	}

	return &p->tokens.arr[p->position++];
}

static int peek_num(Parse *p, u8 num) {
	if (p->position + num >= p->tokens.len) {
		return -1;
	}

	return p->tokens.arr[p->position + num].token;
}

inline static int peek(Parse *p) {
	return peek_num(p, 0);
}

static size_t peekline(Parse *p) {
	if (p->position >= p->tokens.len) {
		return 0;
	}

	return p->tokens.arr[p->position].linenum;
}

static bool match(Parse *p, u8 tok) {
	if (p->position >= p->tokens.len) {
		return false;
	}

	if (p->tokens.arr[p->position].token == tok) {
		p->position++;
		return true;
	}

	return false;
}

static DoobleToken *consume(Parse *p, u8 tok, cstr message) {
	if (peek(p) == tok) return advance(p);

	p->parse_error = true;
	error_file("expected token: %s",
			p->tokens.arr[p->position].linenum, p->buffer, message);
	return NULL;
}

static bool expect(Parse *p, u8 tok, cstr message) {
	if (match(p, tok)) return true;

	p->parse_error = true;
	error_file("expected token: %s",
			p->tokens.arr[p->position].linenum, p->buffer, message);
	return false;
}

// =================================
// === PRIVATE RULE DECLARATIONS ===
// =================================

// TODO: struct & fn literals
// assignment syntax
// += -= *= /= etc.
// mod syntax

typedef enum : u16 {
	TS_NONE   = 1,
	TS_OPT    = 2,
	TS_RES    = 4,
	TS_PTR    = 8,
	TS_ARR    = 16,
	TS_FUNC   = 32,
	TS_NAME   = 64,
	TS_STRUCT = 128,
	TS_SUM    = 256,
} TypeState;

enum : u16 {
	NONE_SET   = TS_OPT  | TS_RES | TS_PTR | TS_ARR  | TS_FUNC | TS_NAME | TS_STRUCT,
	RES_SET    = TS_NONE | TS_OPT | TS_PTR | TS_ARR  | TS_FUNC | TS_NAME | TS_STRUCT,
	OPT_SET    = TS_NONE | TS_PTR | TS_ARR | TS_FUNC | TS_NAME | TS_STRUCT,
	PTR_SET    = TS_OPT  | TS_PTR | TS_ARR | TS_FUNC | TS_NAME | TS_STRUCT,
	ARR_SET    = TS_OPT  | TS_PTR | TS_ARR | TS_FUNC | TS_NAME | TS_STRUCT,
	FUNC_SET   = TS_NONE,
	NAME_SET   = TS_NONE,
	STRUCT_SET = TS_NONE,
};

#define GET_LEAF(...) \
		get_leaf(p->type_tree, leaf, &(TypeLeaf) { __VA_ARGS__ })

static typeid parse_type(Parse *p);

static TypeLeaf *parse_arr(Parse *p, TypeLeaf *leaf) {
	DoobleToken *tok = advance(p);

	switch (tok->token) {
		case DB_RSQUARE:
			return GET_LEAF(DBLTP_SLICE);
		case DB_VEC:
			expect(p, DB_RSQUARE, "expected ']'");
			return GET_LEAF(DBLTP_VEC);
		case DB_NUM:
			expect(p, DB_RSQUARE, "expected ']'");
			return GET_LEAF(.tag = DBLTP_ARR, .size = tok->vali);

		default:
			error_file("unexpeced token in array type", peekline(p), p->buffer);
			break;
	}

	return leaf;
}

// (int, *hello)

static TypeLeaf *parse_fntype(Parse *p, TypeLeaf *leaf) {
	TypeLeaf fn = {
		.tag = DBLTP_FN,
		.fn = {
			.arr = make(typeid, 4),
			.len = 0,
			.cap = 4,
		},
	};

	while (peek(p) != DB_RPAREN) {
		EXTEND_ARR(typeid, fn.fn.arr, fn.fn.len, fn.fn.cap);
		fn.fn.arr[fn.fn.len++] = parse_type(p);

		if (!match(p, DB_COMMA)) break;
	}

	expect(p, DB_RPAREN, "expected ')'");
	fn.fn.ret = match(p, DB_ARROW) ? parse_type(p) : NULL;

	return get_leaf(p->type_tree, leaf, &fn);
}

/*
Pose :: struct {
	x: dooble
	y: dooble
}

-- possible syntax
Player :: struct {
	pos := Pose {0, 0}
}
*/

// NOTE: is it appropriate to do the parsing as part of the type parse?
// it makes sense, but declaration syntax makes things more complex.
// It might be worth it to have an entirely seperate AST node dedicated
// to structs, that builds the type when it is visited.

/* parses a struct starting at the first brace.
 * assumes that the 'struct' keyword has already been consumed.
 * for now it assumes that all default values are 0 */
static TypeLeaf *parse_struct(Parse *p, TypeLeaf *leaf, bool is_union) {
	expect(p, DB_LBRACE, "expected '{' after struct");

	TypeLeaf ztruct = {
		.tag     = is_union ? DBLTP_UNION : DBLTP_STRUCT,
		.members = {
			.arr = make(Member, 5),
			.len = 0,
			.cap = 5,
		},
	};

	// WARN: this is not flexable if I wanted to add default values
	while (!match(p, DB_RBRACE)) {
		let name = consume(p, DB_IDENT, "expected identifier as member of struct");
		if (name == NULL) return NULL;

		bool colon = expect(p, DB_COLON, "expected colon after member in struct");
		if (!colon) return NULL;

		typeid type = parse_type(p);
		if (type == NULL) {
			error_file("expected valid type after struct member",
					name->linenum, p->buffer);
			return NULL;
		}

		expect(p, DB_SEMI, "expected ';' or newline character");

		EXTEND_ARR(Member, ztruct.members.arr, ztruct.members.len, ztruct.members.cap);
		ztruct.members.arr[ztruct.members.len++] = (Member) {
			.type = type,
			.name = copy_str(&name->str),
		};
	}

	return get_leaf(p->type_tree, leaf, &ztruct);
}

static typeid parse_type(Parse *p) {
	TypeState  current = TS_NONE;
	TypeLeaf  *leaf    = NULL;

	loop {
		DoobleToken *tok  = advance(p);
		TypeState    next = TS_NONE;

		if (tok == NULL) {
			error_file("type collides with EOF", peekline(p), p->buffer);
			return NULL;
		}

		switch (tok->token) {
			case DB_QUEST:   next = TS_OPT;    break;
			case DB_BANG:    next = TS_RES;    break;
			case DB_STAR:    next = TS_PTR;    break;
			case DB_LSQUARE: next = TS_ARR;    break;
			case DB_LPAREN:  next = TS_FUNC;   break;
			case DB_IDENT:   next = TS_NAME;   break;
			case DB_STRUCT:  next = TS_STRUCT; break;
			case DB_SUMTYPE: next = TS_SUM;    break;
			
			case DB_EQUAL:
			case DB_COLON:
			case DB_SEMI:
			case DB_EOF:
				p->position--;
				next = TS_NONE;
				break;

			default:
				// ...
				break;
		}

		bool is_valid = false;
		switch (current) {
			case TS_NONE:   is_valid = (NONE_SET   & next); break;
			case TS_OPT:    is_valid = (OPT_SET    & next); break;
			case TS_RES:    is_valid = (RES_SET    & next); break;
			case TS_PTR:    is_valid = (PTR_SET    & next); break;
			case TS_ARR:    is_valid = (ARR_SET    & next); break;
			case TS_FUNC:   is_valid = (FUNC_SET   & next); break;
			case TS_NAME:   is_valid = (NAME_SET   & next); break;
			case TS_STRUCT: is_valid = (STRUCT_SET & next); break;
			case TS_SUM:    is_valid = (STRUCT_SET & next); break;
		}

		if (is_valid != 0) {
			error_file("invalid type", peekline(p), p->buffer);
			return NULL;
		}

		switch (next) {
			case TS_PTR: leaf = GET_LEAF(DBLTP_PTR); break;
			case TS_OPT: leaf = GET_LEAF(DBLTP_OPT); break;
			case TS_RES: leaf = GET_LEAF(DBLTP_ERR); break;

			// NOTE: tokens do not need to immediately get consumed, they could
			// be processed in the functions below.

			case TS_ARR:    leaf = parse_arr(p, leaf);           break;
			case TS_FUNC:   leaf = parse_fntype(p, leaf);        break;
			case TS_STRUCT: leaf = parse_struct(p, leaf, false); break;
			case TS_SUM:    leaf = parse_struct(p, leaf, true);  break;

			case TS_NAME:
				leaf = GET_LEAF(.tag = DBLTP_NAME, .name = copy_str(&tok->str));
				break;

			case TS_NONE:
				return leaf;
		}

		current = next;
	}
}

// == statements ==
static Node *statement(Parse *p);
static Node *ifstmt(Parse *p);
static Node *forstmt(Parse *p);
static Node *dostmt(Parse *p, bool dont);
static Node *block(Parse *p);

// == declarations ==
static Node *declaration(Parse *p);

// == expressions ==
static Node *range(Parse *p);
static Node *expression(Parse *p);
static Node *logic(Parse *p);
static Node *equality(Parse *p);
static Node *comparison(Parse *p);
static Node *bit_wise(Parse *p);
static Node *sum(Parse *p);
static Node *factor(Parse *p);
static Node *unary(Parse *p);
static Node *call(Parse *p);
static Node *atom(Parse *p);

AstResult get_ast(size_t N, DoobleToken tokens[N], TypeTree *tree, const char *const buf) {
	const u8 POOL_INIT_SIZE = 100;

	Parse parse = {
		.buffer   = buf,
		.position = 0,
		.ast_pool = {
			.arr = malloc(sizeof(Node) * POOL_INIT_SIZE),
			.len = 0,
			.cap = POOL_INIT_SIZE,
		},
		.tokens = {
			.arr = tokens,
			.len = N,
		},
		.type_tree = tree,
	};

	// will always be the first element
	let global_scope = append_node(&parse, &(Node) {
		.tag   = EX_BLOCK,
		.block = {
			.arr = make(Node *, 10), // OPTIM: find ideal scope size
			.len = 0,
			.cap = 10,
		},
	});

	while (!match(&parse, DB_EOF)) {
		EXTEND_ARR(Node *,
				global_scope->block.arr,
				global_scope->block.len,
				global_scope->block.len);

		Node *expr = statement(&parse);
		if (expr != NULL) {
			global_scope->block.arr[global_scope->block.len++] = expr;
		}

		expect(&parse, DB_SEMI, "expected ';' or newline character");
	}

	// free tokens block
	free_tokens(N, tokens);
	parse.tokens.arr = NULL;

	// might be unnecessary copying
	return (AstResult) {
		.err       = parse.parse_error,
		.pool      = global_scope,
		.pool_size = parse.ast_pool.len,
	};
}

// === STATEMENTS ===

static Node *statement(Parse *p) {
	Node *node = NULL;

	switch (peek(p)) {
		case DB_IF:
			node = ifstmt(p);
			break;
		case DB_FOR:
			node = forstmt(p);
			if      (node == NULL)             return NULL;
			else if (node->tag == EX_FOREACH)  node->foreach.stmt  = statement(p);
			else if (node->tag == EX_FORWHILE) node->forwhile.stmt = statement(p);
			break;
		case DB_DO:
			node = dostmt(p, false);
			break;
		case DB_DONT:
			node = dostmt(p, true);
			break;

		case DB_INCLUDE: // comptime directive
		case DB_RETURN:
		case DB_YIELD:
		case DB_DEFER:
		case DB_CONTINUE:
		case DB_BREAK:
		case DB_FALL:
		case DB_FREE:
		case DB_MATCH:
		case DB_PACKAGE: // comptime directive
			UNIMPLEMENTED();
			break;

		case DB_LBRACE:
			node = block(p);
			break;
		case DB_IDENT:
			if (peek_num(p, 1) == DB_COLON) {
				node = declaration(p);
				break;
			}
			fallthrough;

		default:
			node = expression(p);
	}

	return node;
}

static Node *ifstmt(Parse *p) {
	if (!match(p, DB_IF)) return NULL;

	let cond = logic(p); // higher level expressions such as ranges don't make sense.
						 // EX: if 0 .. 10 print('hello world')
	let stmt = statement(p);

	let expr = append_node(p, &(Node) {
		.tag = EX_IF,
		.ifstmt = {
			.condition = cond,
			.stmt      = stmt,
			.else_case = NULL,
		},
	});

	// TODO: add support for elif instead of just else if
	if (match(p, DB_ELSE)) {
		expr->ifstmt.else_case = statement(p);
	}

	return expr;
}

// WARN: this is probably a poor way of implementing this. Consider refactoring
static Node *forstmt(Parse *p) {
	if (!match(p, DB_FOR)) return NULL;

	bool by_ref = match(p, DB_AMPER);

	// for &i in array[:-1] ...
	if (peek(p) == DB_IDENT && peek_num(p, 1) == DB_IN) {
		string_t  ident = copy_str(&advance(p)->str);
		Node     *range = expression(p);

		return append_node(p, &(Node) {
			.tag     = EX_FOREACH,
			.foreach = {
				.by_reference = by_ref,
				.ident        = ident,
				.range        = range,
				.stmt         = NULL,
			},
		});
	}
	else {
		if (by_ref) p->position--; // deconsume '&' so 'for &i != p ...' can be parsed

		Node *condition = logic(p);

		if (condition == NULL) {
			error_file("expected condition after 'for'", peekline(p), p->buffer);
		}

		return append_node(p, &(Node) {
			.tag      = EX_FORWHILE,
			.forwhile = {
				.condition = condition,
				.stmt      = NULL,
			},
		});
	}
}

// if a == b Name :: struct { ...

static Node *dostmt(Parse *p, bool dont) {
	if (!match(p, DB_DO)) return NULL;

	size_t mark = p->position;
	Node  *stmt = statement(p);

	if (peek(p) != DB_FOR) {
		// do to how the pool allocator works, the prior 'stmt' will not get
		// freed until the entire pool is dumped, but it will get freed.

		p->position    = mark;
		p->parse_error = true;
		error_file("expected 'for'", peekline(p), p->buffer);
		return NULL;
	}

	let fornode = forstmt(p);
	if (fornode == NULL) {
		p->parse_error = true;
		error_file("expected for statement", peekline(p), p->buffer);
		return NULL;
	}

	if (fornode->tag == EX_FOREACH) {
		fornode->tag          = dont ? EX_DONTEACH : EX_DOEACH;
		fornode->foreach.stmt = stmt;
	}
	else if (fornode->tag == EX_FORWHILE) {
		fornode->tag           = dont ? EX_DONTWHILE : EX_DOWHILE;
		fornode->forwhile.stmt = stmt;
	}

	return fornode;
}

static Node *block(Parse *p) {
	if (!match(p, DB_LBRACE)) return NULL;

	let expr = append_node(p, &(Node) {
		.tag = EX_BLOCK,
		.block = {
			.arr = make(Node *, 2), // OPTIM: find the optimal number for a block
			.len = 0,
			.cap = 2,
		},
	});

	while (!match(p, DB_RBRACE)) {
		if (peek(p) == DB_EOF) {
			error_file("expected '}'", peekline(p), p->buffer);
		}

		EXTEND_ARR(Node *, expr->block.arr, expr->block.len, expr->block.cap);
		Node *stmt = statement(p);
		if (stmt != NULL) {
			expr->block.arr[expr->block.len++] = stmt;
		}

		if (!expect(p, DB_SEMI, "expected ';' xor newline character")) {
			free(expr->block.arr);
			return NULL;
		}
	}

	return expr;
}

// === DECLARATIONS ===

static Node *declaration(Parse *p) {
	if (peek(p) != DB_IDENT) return NULL;

	Node *expr = append_node(p, &(Node) { .tag = EX_DECL });
	expr->declare.name = copy_str(&advance(p)->str);

	loop {
		let tok = advance(p)->token;
		if (tok == DB_COLON) break;
		if (tok == DB_EOF) {
			p->parse_error = true;
			error_file("expected colon after identifier %s",
					peekline(p), p->buffer, expr->declare.name.str);
			return NULL;
		}

		expr->declare.quals.is_static  |= tok == DB_STATIC;
		expr->declare.quals.is_pub     |= tok == DB_PUB;
		expr->declare.quals.is_co      |= tok == DB_CO;
		expr->declare.quals.is_protect |= tok == DB_PROTECT;
		expr->declare.quals.is_final   |= tok == DB_FINAL;
	}

	expr->declare.type = parse_type(p);

	int tok = peek(p);
	if (tok == DB_COLON || tok == DB_EQUAL) {
		expr->declare.is_const = advance(p)->token == DB_COLON;

		if (peek(p) == DB_STRUCT
				|| peek(p) == DB_SUMTYPE
				|| match(p, DB_ALIAS)) // consume the alias token, but leave sum & struct
		{
			TypeLeaf named_leaf = {
				.tag  = DBLTP_NAME,
				.name = expr->declare.name,
			};

			if (!leaf_exists(p->type_tree, NULL, &named_leaf)) {
				error_file("type %s is already defined", peekline(p),
						p->buffer, expr->declare.name);
				p->parse_error = true;
				return NULL;
			}

			typeid named_type = get_leaf(p->type_tree, NULL, &named_leaf);
			freestr(&named_leaf.name);

			typeid type = parse_type(p);
			if (type == VOID_ID) {
				error_file("type %s has invalid type", peekline(p),
						p->buffer, expr->declare.name);
				p->parse_error = true;
				return NULL;
			}

			add_typedef(p->type_tree, named_type, type);
			return NULL;
		}
		else expr->declare.assign = expression(p);
	}

	return expr;
}

// === EXPRESSIONS ===

static inline Node *expression(Parse *p) {
	return range(p);
}

static Node *range(Parse *p) {
	let expr = logic(p);

	if (match(p, DB_DOTDOT)) {
		let left = expr;
		expr = append_node(p, &(Node) {
			.tag   = EX_BINOP,
			.binop = {
				.operator = DB_DOTDOT,
				.expra    = left,
				.exprb    = logic(p),
			},
		});
	}

	return expr;
}

static Node *logic(Parse *p) {
	let expr = equality(p);

	while (peek(p) == DB_AND || peek(p) == DB_OR) {
		let left = expr;
		expr = append_node(p, &(Node) {
			.tag   = EX_BINOP,
			.binop = {
				.operator = advance(p)->token,
				.expra    = left,
				.exprb    = equality(p),
			},
		});
	}

	return expr;
}

static Node *equality(Parse *p) {
	let expr = comparison(p);

	while (match(p, DB_IS)) {
		Node *left   = expr;
		bool  is_not = match(p, DB_NOT);

		expr = append_node(p, &(Node) {
			.tag   = EX_BINOP,
			.binop = {
				.operator = is_not ? DB_NOT : DB_IS,
				.expra    = left,
				.exprb    = comparison(p),
			},
		});
	}

	return expr;
}

static Node *comparison(Parse *p) {
	let expr = bit_wise(p);

	// call peek(p) for some weird C optimization reason
	int tok = peek(p);
	while (tok == DB_LESS
			|| tok == DB_LESSEQ
			|| tok == DB_GREATER
			|| tok == DB_GREATEREQ)
	{
		let left = expr;
		expr = append_node(p, &(Node) {
			.tag   = EX_BINOP,
			.binop = {
				.operator = advance(p)->token,
				.expra    = left,
				.exprb    = bit_wise(p),
			},
		});

		tok = peek(p);
	}

	return expr;
}

static Node *bit_wise(Parse *p) {
	let expr = sum(p);

	while (peek(p) == DB_BITOR || peek(p) == DB_AMPER) {
		let left = expr;
		expr = append_node(p, &(Node) {
			.tag   = EX_BINOP,
			.binop = {
				.operator = advance(p)->token,
				.expra    = left,
				.exprb    = sum(p),
			},
		});
	}

	return expr;
}

static Node *sum(Parse *p) {
	let expr = factor(p);

	while (peek(p) == DB_PLUS || peek(p) == DB_MINUS) {
		let left = expr;
		expr = append_node(p, &(Node) {
			.tag   = EX_BINOP,
			.binop = {
				.operator = advance(p)->token,
				.expra    = left,
				.exprb    = factor(p),
			},
		});
	}

	return expr;
}

static Node *factor(Parse *p) {
	let expr = unary(p);

	while (peek(p) == DB_STAR || peek(p) == DB_SLASH) {
		let left = expr;
		expr = append_node(p, &(Node) {
			.tag   = EX_BINOP,
			.binop = {
				.operator = advance(p)->token,
				.expra    = left,
				.exprb    = unary(p),
			},
		});
	}

	return expr;
}

static Node *unary(Parse *p) {
	switch (peek(p)) {
		case DB_MINUS:
		case DB_NOT:
		case DB_STAR:
		case DB_AMPER:
			return append_node(p, &(Node) {
				.tag = EX_UNARY,
				.unary = {
					.operator = advance(p)->token,
					.expr     = unary(p),
				},
			});
	}

	return call(p);
}

// call -> primary ( '(' arguments? ')' | '.' identifier )* ;
static Node *call(Parse *p) {
	let expr = atom(p);

	loop {
		if (match(p, DB_LPAREN)) {
			size_t cap = 3;
			Call call = {
				.caller = expr,
				.params = make(Node *, cap),
				.len    = 0,
			};

			for_range (i, 127) {
				let param = expression(p);
				if (param == NULL) break;

				EXTEND_ARR(Node *, call.params, call.len, cap);
				call.params[call.len++] = param;

				if (!match(p, DB_COMMA) || peek(p) == DB_RPAREN) break;
			}

			if (call.len >= 127) {
				error_file("function call may only contain 127 arguments",
						peekline(p), p->buffer);
			}

			expect(p, DB_RPAREN, "function call must have a closing ')'");

			expr = append_node(p, &(Node) {
				.tag  = EX_CALL,
				.call = call,
			});
		}

		else if (match(p, DB_DOT)) {
			DoobleToken *token = consume(p, DB_IDENT, "expected identifier");
			if (token == NULL) return NULL;

			expr = append_node(p, &(Node) {
				.tag    = EX_SUBMEMBER,
				.member = {
					.expr = expr,
					.name = copy_str(&token->str),
				},
			});
		}

		else return expr;
	}
}

static Arguments arguments(Parse *p) {
	Arguments args = {
		.arr = malloc(3),
		.len = 0,
		.cap = 3,
	};

	for (int i = 0; true; i++) {
		if (i > 127) {
			p->parse_error = true;
			error_file("function may only contain 127 arguments",
					peekline(p), p->buffer);
			break;
		}

		Node *arg = declaration(p);
		if (arg == NULL) break;

		EXTEND_ARR(Node *, args.arr, args.len, args.cap);
		args.arr[args.len++] = arg;

		if (!match(p, DB_COMMA)) break;
	}

	return args;
}

static Node *atom(Parse *p) {
	// To solve the problem of parsing functions vs constants that use parenthesis I
	// have decided to make a small temporary sacrifice in syntax.

	// TODO: make the temp syntax less temporary.
	// I have some ideas, currently recorded in my CodeWars notepad that could achieve
	// this without a *great* sacrifice to performance.
	if (match(p, DB_LPAREN)) {
		i16  lparen_count = 0;
		bool is_function  = false;

		for (int i = p->position; i < p->tokens.len - 1; i++) {
			if (p->tokens.arr[i].token == DB_RPAREN && lparen_count == 0) {
				const u8 after_paren = p->tokens.arr[i + 1].token;
				
				is_function = after_paren == DB_LBRACE || after_paren == DB_ARROW;
				break;
			}

			else if (p->tokens.arr[i].token == DB_LPAREN) lparen_count++;
			else if (p->tokens.arr[i].token == DB_RPAREN) lparen_count--;
		}

		if (is_function) {
			Function fn = {0};
			fn.args = arguments(p);

			if (fn.args.len == 0) {
				free(fn.args.arr);
				fn.args.arr = NULL;
				fn.args.cap = 0;
			}

			expect(p, DB_RPAREN, "missing ')' at end of argument list");

			if (match(p, DB_ARROW)) {
				fn.ret_type = parse_type(NULL);
			}

			if (peek(p) != DB_LBRACE) {
				error_file("expected '{' after function signature",
						peekline(p), p->buffer);
				return NULL;
			}

			fn.block = block(p);

			return append_node(p, &(Node) {
				.tag      = EX_FUNCTION,
				.function = fn,
			});
		}

		else {
			let expr = expression(p);
			expect(p, DB_RPAREN, "missing ')'");
			return expr;
		}
	}

	DoobleToken *tok = advance(p);
	Literal      lit = {0};

	// NOTE: I might be able to clean this up a bit
	switch (tok->token) {
		case DB_NUM:
			lit.tag  = LIT_NUM;
			lit.numi = tok->vali;
			break;
		case DB_FLOAT:
			lit.tag  = LIT_FLT;
			lit.numf = tok->valf;
			break;
		case DB_NIL:
			lit.tag = LIT_NIL;
			break;
		case DB_TRUE:
			lit.tag     = LIT_BOOL;
			lit.boolean = true;
			break;
		case DB_FALSE:
			lit.tag     = LIT_BOOL;
			lit.boolean = false;
			break;
		case DB_STR:
			lit.tag = LIT_STR;
			lit.str = copy_str(&tok->str);
			break;
		case DB_IDENT:
			lit.tag = LIT_IDENT;
			lit.str = copy_str(&tok->str);
			break;
		default:
			p->position--;
			return NULL;
	}

	return append_node(p, &(Node) {
		.tag     = EX_LITERAL,
		.literal = lit,
	});
}

void free_ast(size_t N, Node pool[N]) {
	for_range (i, N) {
		switch (pool[i].tag) {
			case EX_FOREACH:
			case EX_DOEACH:
			case EX_DONTEACH:
				freestr(&pool[i].foreach.ident);
				break;

			case EX_BLOCK:
				free(pool[i].block.arr); // assumes that the dynamic array is in
										 // the heap instead of a pool allocator.
				break;
			case EX_DECL:
				freestr(&pool[i].declare.name);
				break;
			case EX_CALL:
				free(pool[i].call.params); // again, might need a refactor if I use
										   // a pool in the future.
				break;
			case EX_SUBMEMBER:
				freestr(&pool[i].member.name);
				break;
			case EX_FUNCTION:
				free(pool[i].function.args.arr); // pool again
				break;
			case EX_LITERAL:
				if (pool[i].literal.tag == LIT_STR) {
					freestr(&pool[i].literal.str);
				}
				break;

			default:
				break;
		}
	}

	free(pool);
}
