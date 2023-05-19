#include "passes.h"
#include "parse.h"
#include "../testing/testing.h"
#include <stdio.h>

static void print_rule(Rule *rule) {
	printf("( RULE %s ", rule->name.str);
	for (int i = 0; i < rule->length; i++)
		print_tree(rule->cases[i]);
	printf(") ");
}

static void print_grammar(Grammar *grmr) {
	printf("( -> ");
	for (int i = 0; i < grmr->length; i++)
		print_tree(grmr->tokens[i]);
	printf(") ");
}

static void print_or(OrNode *or) {
	printf("( | ");
	for (int i = 0; i < or->length; i++)
		print_tree(or->options[i]);
	printf(") ");
}

static void print_star(Expr *star) {
	printf("( * ");
	print_tree(star->star);
	printf(") ");
}

static void print_plus(Expr *plus) {
	printf("( + ");
	print_tree(plus->plus);
	printf(") ");
}

static void print_option(Expr *option) {
	printf("( ? ");
	print_tree(option->option);
	printf(") ");
}

static void print_ident(DepToken *token) {
	printf("%s ", token->value.str);
}

static void print_regex(DepToken *token) {
	printf("/%s/ ", token->value.str);
}

static void print_string(DepToken *token) {
	printf("'%s' ", token->value.str);
}

void print_tree(Expr *expr) {
	switch (expr->tag) {
		case DEP_RULE:		print_rule(&expr->rule);		break;
		case DEP_GRAMMAR:	print_grammar(&expr->grammar);	break;
		case DEP_OR:		print_or(&expr->ornode);		break;
		case DEP_STAR:		print_star(expr->star);			break;
		case DEP_PLUS: 		print_plus(expr->plus);			break;
		case DEP_OPT: 		print_option(expr->option);		break;
		case DEP_IDENT: 	print_ident(&expr->token);		break;
		case DEP_REGEX:		print_regex(&expr->token);		break;
		case DEP_STRING:	print_string(&expr->token);		break;
		default: printf("This might be a group\n"); break;
	}
}
