#pragma once

#include "lexer.h"
#include "../strutils/str.h"
#include "../utils/utils.h"

/*
 * Deplicate is a parser generator that compiles to c code
 *
 * The syntax is basically as follows
 * rule -> other_rule | ( group token | hi ) * ( token ) + ;
 *
 * defining the syntax with itself
 * rule		-> '[a-zA-Z]+' ( '->' grammar )+ ';' ;
 * grammar	-> ( '(' grammar ')' )
 * 			-> or
 * 			;
 * or		-> star ( '|' star )+
 * 			-> star
 * 			;
 * star		-> plus '*'? ;
 * plus		-> option '+'? ;
 * option	-> atom '?'? ;
 * atom		-> /[a-zA-Z]+/
 * 			-> /\'[a-zA-Z]+\'/
 * 			-> /\/[a-zA-Z]+\//
 * 			-> grammar
 * 			; */

typedef struct Expr_t Expr;

typedef struct {
	string_t	name;
	Expr		**cases;
	u32			length;
} Rule;

typedef struct {
	Expr	**tokens; // WARN: Bad name
	u32		length;
} Grammar;

typedef struct {
	Expr	**options;
	u32		length;
} OrNode;

typedef struct {
	string_t	value;
	u32			line; // WARN: this information is never used
} DepToken;

struct Expr_t {
	enum {
		DEP_RULE, DEP_GRAMMAR, DEP_OR,
		DEP_GROUP, DEP_STAR, DEP_PLUS, DEP_OPT,
		DEP_IDENT, DEP_REGEX, DEP_STRING,
	} tag;

	union {
		Rule		rule;
		Grammar		grammar;
		OrNode		ornode;
		Expr		*star;
		Expr		*plus;
		Expr		*option;
		Expr		*group;
		DepToken	token;
	};
};

typedef struct {
	string_t	*deplicate_tokens;
	u16			len;
	u16			cap;
} GenTokens;

// FIXME: use a dynamic pool instead of this hot garbage.
// Any sufficiently advanced grammar will contain a hell of a lot more than
// 1024 AST nodes
typedef struct {
	string_t	*source;
	u32			pos;
	Token		*tokens;
	u32			length;
	Expr		ast_pool[1024];
	u16			ast_top;
	GenTokens	gtokens;
} Parse;

Expr *parse(Scan *scan, Parse **parse);
bool free_parse(Parse *parse);
