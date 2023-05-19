#include "parse.h"
#include "lexer.h"
#include "../utils/utils.h"
#include "../utils/err.h"
#include "../testing/testing.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// private forward declarations
static Expr *rule(Parse *parse);
static Expr *grammar(Parse *parse);
static Expr *or(Parse *parse);
static Expr *star(Parse *parse);
static Expr *plus(Parse *parse);
static Expr *option(Parse *parse);
static Expr *atom(Parse *parse);
static Expr *group(Parse *parse);

static int	advance(Parse *parse);
static int	peek(Parse *parse, int num);
static bool	match(Parse *parse, int type);
static bool	consume(Parse *parse, int type);

#define TOKEN parse->tokens[parse->pos]
Expr *parse(Scan *scan, Parse **parse_out) {
	if (scan->tokens == NULL) return NULL;

	Parse *parse = malloc(sizeof(Parse));
	parse->source	= scan->str;
	parse->pos		= 0;
	parse->tokens	= scan->tokens;
	parse->length	= scan->token_len;
	parse->ast_top	= 0;

	// take ownership of scan
	scan->tokens	= NULL;
	scan->token_len	= 0;
	scan->token_cap	= 0;

	*parse_out = parse;
	return rule(parse);
}

bool free_parse(Parse *parse) {
	if (parse == NULL) return false;

	if (!free_tokens(parse->tokens, parse->length)) return false;
	parse->tokens = NULL;
	parse->length = 0;

	for (int i = 0; i < parse->ast_top; i++) {
		switch (parse->ast_pool[i].tag) {
			case DEP_RULE:
				freestr(&parse->ast_pool[i].rule.name);
				free(parse->ast_pool[i].rule.cases);
				break;
			case DEP_GRAMMAR:
				free(parse->ast_pool[i].grammar.tokens);
				break;
			case DEP_OR:
				free(parse->ast_pool[i].ornode.options);
				break;
			case DEP_REGEX: fallthrough;
			case DEP_IDENT: fallthrough;
			case DEP_STRING:
				freestr(&parse->ast_pool[i].token.value);
				break;
			default:
				break;
		}
	}

	free(parse);
	return true;
}

static Expr *new_ast(Parse *parse) {
	if (parse->ast_top > sizeof(parse->ast_pool)) {
		error_file("Exceeded maximum AST nodes", TOKEN.line, parse->source->str);
		return NULL;
	}

	return &parse->ast_pool[parse->ast_top++];
}

// Not a parsing rule, but does create more AST
static Expr *token(Parse *parse, int type) {
	Expr *expr = new_ast(parse);
	*expr = (Expr) {
		.tag	= type,
		.token	= (DepToken) {
			.value	= copy_str(&TOKEN.value),
			.line	= TOKEN.line,
		},
	};

	advance(parse); // consume token
	return expr;
}

static Expr *rule(Parse *parse) {
	if (peek(parse, 0) != TOK_IDENTIFIER) {
		error_file("Expected identifier", TOKEN.line, parse->source->str);
		return NULL;
	}

	string_t rule = copy_str(&TOKEN.value);
	advance(parse);

	uint32_t len     = 0;
	uint32_t cap     = 2;
	Expr   **options = malloc(sizeof(Expr *) * cap);

	while (!match(parse, TOK_SEMI)) {
		if (!consume(parse, TOK_ARROW))
			return NULL;

		if (len >= cap) {
			cap *= 2;
			Expr **tmp = realloc(options, sizeof(Expr *) * cap);
			if (tmp == NULL) PANIC("Reallocation failure\n");
		}

		options[len++] = grammar(parse);
	}

	Expr *expr = new_ast(parse);
	*expr = (Expr) {
		.tag	= DEP_RULE,
		.rule	= (Rule) {
			.name	= rule,
			.cases	= options,
			.length	= len,
		},
	};

	return expr;
}

static Expr *grammar(Parse *parse) {
	int cap = 10;

	Expr *grammar = new_ast(parse);
	*grammar = (Expr) {
		.tag		= DEP_GRAMMAR,
		.grammar	= (Grammar) {
			.tokens	= malloc(sizeof(Expr *) * cap),
			.length	= 0,
		},
	};

	Expr **dtokens = grammar->grammar.tokens;

	int token;
	while ((token = peek(parse, 0)) != TOK_SEMI &&
			token != TOK_RPAREN && token != TOK_END)
	{
		dtokens[grammar->grammar.length++] = or(parse);

		// dynamic array capabilities
		if (grammar->grammar.length >= cap) {
			cap *= 2;
			grammar->grammar.tokens = realloc(dtokens, sizeof(Expr *) * cap);
			dtokens = grammar->grammar.tokens;

			if (dtokens == NULL) {
				PANIC("Reallocation failure");
			}
		}
	}

	return grammar;
}

// WARN: This code is very poorly structured but I think it will work.
// I need to redo this whole function and when I do I should try to do it
// according to the grammar rules I come up with.

static Expr *or(Parse *parse) {
	Expr *expr = star(parse);

	if (match(parse, TOK_OR)) {
		Expr *expra = expr;

		int capacity = 2;
		expr = new_ast(parse);
		*expr = (Expr) {
			.tag    = DEP_OR,
			.ornode = {
				.options = malloc(sizeof(Expr *) * 2),
				.length  = capacity,
			},
		};
		expr->ornode.options[0] = expra;

		int i = 1;
		do {
			if (i >= capacity) {
				capacity *= 2;
				Expr **opt = realloc(expr->ornode.options, sizeof(Expr *) * capacity);
				if (opt == NULL) {
					PANIC("Reallocation failure");
				}
			}

			expr->ornode.options[i] = star(parse);
			expr->ornode.length     = i++;
		} while (match(parse, TOK_OR));
	}

	return expr;
}

static Expr *star(Parse *parse) {
	Expr *expr = plus(parse);

	if (match(parse, TOK_STAR)) {
		Expr *expra = expr;

		expr = new_ast(parse);
		*expr = (Expr) {
			.tag	= DEP_STAR,
			.star	= expra,
		};
	}

	return expr;
}

static Expr *plus(Parse *parse) {
	Expr *expr = option(parse);

	if (match(parse, TOK_PLUS)) {
		Expr *expra = expr;

		expr = new_ast(parse);
		*expr = (Expr) {
			.tag	= DEP_PLUS,
			.plus	= expra,
		};
	}

	return expr;
}

static Expr *option(Parse *parse) {
	Expr *expr = atom(parse);

	if (match(parse, TOK_OPT)) {
		Expr *expra = expr;

		expr = new_ast(parse);
		*expr = (Expr) {
			.tag	= DEP_OPT,
			.option	= expra,
		};
	}

	return expr;
}

static Expr *atom(Parse *parse) {
	switch (peek(parse, 0)) {
		case TOK_REGEX:			return token(parse, DEP_REGEX);
		case TOK_IDENTIFIER:	return token(parse, DEP_IDENT);
		case TOK_STRING:		return token(parse, DEP_STRING);
		case TOK_LPAREN:		return group(parse);

		default:
			error_file("Unexpected atomic token", TOKEN.line, parse->source->str);
			return NULL;
	}
}

static Expr *group(Parse *parse) {
	advance(parse); // consume '('

	Expr *group = new_ast(parse);
	*group = (Expr) {
		.tag	= DEP_GROUP,
		.group	= grammar(parse),
	};

	if (!consume(parse, TOK_RPAREN)) {
		error_file("expected closing paren", TOKEN.line, parse->source->str);
		return NULL;
	}

	return group;
}

static int advance(Parse *parse) {
	if (parse->pos >= parse->length) return -1;
	return parse->tokens[parse->pos++].type;
}

static int peek(Parse *parse, int num) {
	if (parse->pos >= parse->length) return -1;
	return parse->tokens[parse->pos + num].type;
}

static bool match(Parse *parse, int type) {
	if (peek(parse, 0) == type) {
		parse->pos++;
		return true;
	}

	return false;
}

static bool consume(Parse *parse, int type) {
	if (peek(parse, 0) == type) {
		parse->pos++;
		return true;
	} else {
		error_file("Expected token: ", TOKEN.line, parse->source->str);
		error("\tI really need to add varags and printf formatting\n");
		return false;
	}
}

#undef TOKEN
