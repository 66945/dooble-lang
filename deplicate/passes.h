#pragma once

#include "../codegen/codegen.h"
#include "../strutils/str.h"
#include "../utils/hash.h"
#include "../utils/utils.h"
#include "parse.h"
#include <stdint.h>

typedef struct RuleNode_t RuleNode;

struct RuleNode_t {
	enum {
		RULE_STR,
		RULE_ARR,
		RULE_BOOL,  // '?'
		RULE_COUNT, // '|'
		RULE_EXPR,
		RULE_STRUCT,
		RULE_UNION, // '|'
	} tag;

	// null unless arr or struct
	union {
		RuleNode **members;
		RuleNode *branch;
	};

	u16 nodes_len;
};

void free_rule_tree(RuleNode *node, bool root);

typedef struct {
	Expr    **asts;
	u16       current;
	HashSet   tokens;
	HashSet   captures;

	string_t struct_defs;
	string_t rule_funcs;
} SymbolPass;

void print_tree(Expr *expr);
void symbolic_pass(Expr **expr, u32 num);

/* This struct is kind of messy and doesn't need most of these elements.
 * I need a seperate thing for checks and captures. */
typedef struct {
	RuleNode **rules; // *[]
	u16        current; // not needed
	u16        nesting;
	u16		   check_num;
	u16		   cap_num;
	HashSet    tokens;
	HashSet    captures;

	CodeGen   cg;
	string_t  rule_defs;
} CompilePass;

void write_lexer(SymbolPass *pass, const char *lex_h, const char *lex_c);

// compilation visitor helpers
void compile_pass(Expr **exprs, RuleNode **rules, SymbolPass *pass, u32 num);
