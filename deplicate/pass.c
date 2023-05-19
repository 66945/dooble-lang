#include "passes.h"
#include "../utils/utils.h"
#include "../utils/err.h"
#include "../strutils/template/template.h"
#include "../testing/testing.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RULE_SIZE UINT16_MAX
static RuleNode	rule_pool[RULE_SIZE];
static int		rule_top = 0;

static RuleNode *rule_alloc(void) {
	if (rule_top >= RULE_SIZE) {
		error("Daniel, you idiot. You didn't allocate enough memory for the rules\n");
		return NULL;
	}
	return &rule_pool[rule_top++];
}

// basically a macro, but with c syntax
static inline void free_rule_pool(void) {
	rule_top = 0;
}

void free_rule_tree(RuleNode *node, bool root) {
	switch (node->tag) {
		case RULE_STR:   fallthrough;
		case RULE_EXPR:  fallthrough;
		case RULE_COUNT: fallthrough;
		case RULE_BOOL: return;

		case RULE_ARR:
			if (node->branch != NULL) {
				free_rule_tree(node->branch, false);
			}
			break;
		case RULE_UNION: fallthrough;
		case RULE_STRUCT:
			for (int i = 0; i < node->nodes_len; i++) {
				free_rule_tree(node->members[i], false);
			}

			free(node->members);
			break;
	}

	if (root) free_rule_pool();
}

static void register_dep_regex(string_t *str, SymbolPass *pass) {
	add_item(&pass->captures, str->str);
}

static void register_dep_token(string_t *str, SymbolPass *pass) {
	add_item(&pass->tokens, str->str);
}

// Visitor pattern

/* I am messing with the visitor pattern a bit,
 * which is fine because I have some knowlege about
 * the AST at this point
 *
 * In order to combine the symbolic pass with the generation pass
 * I sometimes split up the visitors to different roots */
static RuleNode *visit_in_rule(Expr *expr, SymbolPass *pass);

static RuleNode *visit_grammar(Grammar *grmr, SymbolPass *pass) {
	if (grmr->length == 0) return NULL;

	int len = 0;
	RuleNode **members = malloc(sizeof(RuleNode *) * grmr->length);

	for (int i = 0; i < grmr->length; i++) {
		RuleNode *node = visit_in_rule(grmr->tokens[i], pass);
		if (node == NULL) continue;

		members[len++] = node;
	}

	if (len == 0) {
		free(members);
		return NULL;
	} else if (len == 1) {
		RuleNode *node = members[0];
		free(members);
		return node;
	}

	RuleNode *grmr_node = rule_alloc();
	*grmr_node = (RuleNode) {
		.tag		= RULE_STRUCT,
		.members	= members,
		.nodes_len	= len,
	};

	return grmr_node;
}

// NOTE: I would love to have a way to make this more concise
static RuleNode *visit_or(OrNode *or, SymbolPass *pass) {
	if (or->length > 15) {
		error("Or node length %d is > than 15", or->length);
		return NULL;
	}

	RuleNode **members      = malloc(sizeof(RuleNode *) * 15);
	int        member_count = 0;

	for (int i = 0; i < or->length; i++) {
		RuleNode *rule = visit_in_rule(or->options[i], pass);
		members[member_count++] = rule;
	}

	if (member_count == 1) {
		RuleNode *rule = members[0];
		free(members);
		return rule;
	}

	RuleNode *or_union = rule_alloc();
	*or_union = (RuleNode) {
		.tag		= RULE_UNION,
		.members	= members,
		.nodes_len	= member_count,
	};

	RuleNode *or_struct = rule_alloc();
	*or_struct = (RuleNode) {
		.tag		= RULE_STRUCT,
		.members	= malloc(sizeof(RuleNode) * 2),
		.nodes_len	= 2,
	};

	RuleNode *count = rule_alloc();
	*count = (RuleNode) { .tag = RULE_COUNT };

	or_struct->members[0] = or_union;
	or_struct->members[1] = count;

	return or_struct;
}

static RuleNode *visit_star(Expr *star, SymbolPass *pass) {
	if (star->star == NULL) return NULL;

	RuleNode *node = rule_alloc();
	*node = (RuleNode) {
		.tag	= RULE_ARR,
		.branch	= visit_in_rule(star->star, pass),
	};

	return node;
}

static RuleNode *visit_plus(Expr *plus, SymbolPass *pass) {
	if (plus->plus == NULL) return NULL;

	RuleNode *node = rule_alloc();
	*node = (RuleNode) {
		.tag	= RULE_ARR,
		.branch	= visit_in_rule(plus->plus, pass),
	};

	return node;
}

static RuleNode *visit_option(Expr *option, SymbolPass *pass) {
	if (option->option == NULL) return NULL;

	RuleNode *branch = visit_in_rule(option->option, pass);
	if (branch == NULL) return NULL;

	RuleNode *node = rule_alloc();
	*node = (RuleNode) {
		.tag	= RULE_BOOL,
		.branch	= branch,
	};

	return node;
}

static RuleNode *visit_ident(DepToken *ident, SymbolPass *pass) {
	RuleNode *node = rule_alloc();
	node->tag = RULE_EXPR;
	return node;
}

static RuleNode *visit_regex(DepToken *regex, SymbolPass *pass) {
	register_dep_regex(&regex->value, pass);

	RuleNode *node = rule_alloc();
	node->tag = RULE_STR;
	return node;
}

static RuleNode *visit_string(DepToken *str, SymbolPass *pass) {
	register_dep_token(&str->value, pass);
	return NULL;
}

static RuleNode *visit_in_rule(Expr *expr, SymbolPass *pass) {
	switch (expr->tag) {
		case DEP_GRAMMAR:	return visit_grammar(&expr->grammar, pass);
		case DEP_OR:		return visit_or(&expr->ornode, pass);
		case DEP_STAR:		return visit_star(expr->star, pass);
		case DEP_PLUS: 		return visit_plus(expr->plus, pass);
		case DEP_OPT: 		return visit_option(expr->option, pass);
		case DEP_IDENT: 	return visit_ident(&expr->token, pass);
		case DEP_REGEX:		return visit_regex(&expr->token, pass);
		case DEP_STRING:	return visit_string(&expr->token, pass);
		default: return NULL;
	}
}

static void build_rule_struct(RuleNode *rule_tree, bool root, string_t *out) {
	switch (rule_tree->tag) {
		case RULE_STRUCT: fallthrough;
		case RULE_UNION: {
			static int capnum = 0;
			concat_cstr(out, rule_tree->tag == RULE_STRUCT ? "struct {" : "union {");

			for (int i = 0; i < rule_tree->nodes_len; i++) {
				// so that inner structs can start with cap00
				int temp_capnum = capnum;
				build_rule_struct(rule_tree->members[i], false, out);
				capnum = temp_capnum;

				concatf_cstr(out, "cap%d;", capnum);
				capnum++;
			}
			concat_cstr(out, "}");
			break;
		}

		case RULE_ARR:
			build_rule_struct(rule_tree->branch, false, out);
			concat_cstr(out, "*");
			break;

		case RULE_STR:
			concat_cstr(out, "string_t ");
			break;
		case RULE_EXPR:
			concat_cstr(out, "Expr *");
			break;
		case RULE_BOOL:
			concat_cstr(out, "bool ");
			break;
		case RULE_COUNT:
			concat_cstr(out, "int ");
			break;
	}
}

static void visit_rule(Rule *rule, SymbolPass *pass) {
	if (rule->length > 0 || rule->cases[0]->tag != DEP_GRAMMAR) {
		error("rule was not followed by a grammar\n");
		return;
	}

	RuleNode *rule_trees[rule->length];
	for (int i = 0; i < rule->length; i++) {
		rule_trees[i] = visit_in_rule(rule->cases[i], pass);
	}

	smart_string fwd_decl = init_str("static Expr *");
	concat(&fwd_decl, &rule->name);
	concat_cstr(&fwd_decl, "(Parse *parse);\n");
	insert(&pass->rule_funcs, &fwd_decl, 0);

	// build structs
	for (int i = 0; i < rule->length; i++) {
		concat_cstr(&pass->struct_defs, "typedef ");

		build_rule_struct(rule_trees[i], true, &pass->struct_defs);

		concat(&pass->struct_defs, &rule->name);
		concatf_cstr(&pass->struct_defs, "%d;\n", i);
	}
}

static void visit(Expr *expr, SymbolPass *pass) {
	switch (expr->tag) {
		case DEP_RULE: visit_rule(&expr->rule, pass);            break;
		default:       error("Visited wrong node in 'visit'\n"); break;
	}
}

void symbolic_pass(Expr **expr, uint32_t num) {
	SymbolPass pass = {
		.asts     = expr,
		.current  = 0,
		.tokens   = init_hashset(),
		.captures = init_hashset(),

		.struct_defs = init_str(""),
		.rule_funcs  = init_str(""),
	};

	for (pass.current = 0; pass.current < num; pass.current++) {
		logvar(pass.current);
		visit(pass.asts[pass.current], &pass);
	}

	write_lexer(&pass, "lex.h", "lex.c");
	printf("---  RULE DECLARATIONS   ---\n%s\n", pass.rule_funcs.str);
	printf("---  STRUCT DEFINITIONS  ---\n%s\n", pass.struct_defs.str);

	// TODO: move free to different location
	free_hashset(&pass.tokens);
	free_hashset(&pass.captures);
	freestr(&pass.struct_defs);
	freestr(&pass.rule_funcs);
}
