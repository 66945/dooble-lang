#include "parse.h"
#include "passes.h"
#include "../strutils/template/template.h"
#include "../utils/file.h"
#include "../utils/err.h"
#include "../testing/testing.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define tputstrf(template, buf, size, str, ...) \
	do {                                        \
		sprintf_s(buf, size, str, __VA_ARGS__); \
		tputstr(template, buf);                 \
	} while (0)

static void match_tokens(Template *tp, HashSet *tokens, int offset, bool regex) {
	char tok_buf[15];
	for (int i = 0; i < tokens->cap; i++) {
		if (!tokens->items[i].full) continue;

		tputstr(tp, regex ? "if (match(scan, \"" :
							"if (match_regex(scan, \"");

		int tok_lit_len = strlen(tokens->items[i].key);
		for (int j = 0; j < tok_lit_len; j++) {
			tputchar(tp, tokens->items[i].key[j]);
		}

		tputstr(tp, "\")) {\n");
		tputstr(tp, "advance(scan, consume);\n"
						"emit_token(scan, (Token) {\n"
						"	.type =");
		tputstrf(tp, tok_buf, 15, "TOK%03d", tokens->items[i].ordered + offset);
		tputstr(tp, ",\n"
						"	.line = scan->line\n"
						"	});\n"
						"continue;\n");
		tputstr(tp, "}\n");
	}
}

// Compilation
void write_lexer(SymbolPass *pass, const char *lex_h, const char *lex_c) {
	// Write header
	Template header;
	init_template("deplicate/templates/lexer.ht", &header);

	char tok_buf[15]; // should not have over 999 tokens
	for (int i = 0; i < pass->tokens.cap; i++) {
		if (!pass->tokens.items[i].full) continue;
		tputstrf(&header, tok_buf, 15, "\t\tTOK%03d,\n",
				pass->tokens.items[i].ordered);
	}

	for (int i = 0; i < pass->captures.cap; i++) {
		if (!pass->captures.items[i].full) continue;

		int token_num = pass->tokens.len + pass->captures.items[i].ordered;
		tputstrf(&header, tok_buf, 15, "\t\tTOK%03d,\n", token_num);
	}

	if (!next_mark(&header)) {
		error("lexer.ht has more templates than expected\n");
		error(header.mark);
	}

	write_file(&header.output, lex_h);
	free_template(&header);

	// Write lexer
	Template lexer;
	init_template("deplicate/templates/lexer.ct", &lexer);

	match_tokens(&lexer, &pass->tokens, 0, false);
	match_tokens(&lexer, &pass->captures, pass->tokens.len, true);

	next_mark(&lexer); // free conditions
	for (int i = 0; i < pass->captures.cap; i++) {
		if (!pass->captures.items[i].full) continue;

		int token_num = pass->tokens.len + pass->captures.items[i].ordered;
		tputstr(&lexer, "scan->tokens[i] == ");
		tputstrf(&lexer, tok_buf, 15, "TOK%03d", token_num);
		tputstr(&lexer, " || ");
	}

	// so that last '|' doesn't do anything
	tputchar(&lexer, '0');

	if (!next_mark(&lexer)) {
		error("lexer.ct has more templates than expected\n");
		error(lexer.mark);
	}

	write_file(&lexer.output, lex_c);
	free_template(&lexer);
}

// compilation visitor helpers
static void compile(Expr *expr, CompilePass *pass);

// ISSUE: this thinkpiece needs to be implemented in place of the hot garbage
// I had previously written

/* Thinkpiece: 'What is the DEAL with compiler compilers?'
 *
 * === So, uh, I was really tired writing the last bit of this... ===
 *
 * Terminology:
 *     subrule:
 *         Different possible rules grouped under one function. Seperated by '->'
 *         Mostly used for recursive decent, but can be used for more complex
 *         expressions that need to be under one name.
 *     section:
 *         A portion of a rule defined by the syntax of deplicate. Represented
 *         by a single deplicate AST node.
 *
 * So the big question is of course how to turn the rule into a
 * function, but it can be split up a little bit easier.
 *
 * The 5 questions are as follows:
 *     1. What should the behavior be when multiple rules exist?
 *     2. How do loops work?
 *     3. How should the program handle fail states?
 *     4. What is the behavior for single member rules?
 *     5. How should the fail states for '|'s be handled?
 *
 * 1. multiple rules | STATUS: NOT STARTED
 *     I think that every subrule should have it's own associated rule struct,
 *     then decisions can occur.
 *
 *     Basically if any case failed to parse, it would move on to the next option.
 *     This may have some difficulties with single member rules, see item 4
 *
 * 2. loops | STATUS: NOT STARTED
 *     This seems pretty easy when looking at single tokens or structures, but it
 *     is actually pretty complicated. I think the hardest part is loading the
 *     generated struct with the correct data.
 *
 *     The main issue with the data collection is that loop elements that are defined
 *     by structures. A rule such as:
 *
 *         rule -> ( 'let' /[a-zA-Z]+/ '=' expression ) * ;
 *
 *     will produce a generated struct in the following form: */
#if 0
        typedef struct {
            struct {
                string_t  cap00;
                Expr     *cap01;
            } *cap00;
        } rule;
#endif
/*     The anonymous struct produces some problems for us. I cannot get the size
 *     of the struct for the dynamic array. If in the generation pass I had a
 *     UID for each of the grammar sections to use, that would be helpful. This
 *     would require a method of extracting this information from the node during
 *     compilation. */
#if 0
		typedef struct anon00 {
			struct anon01 { /* ... */ } *capXX;
			// ...
		} rule;

		typedef struct anon02 { /* ... */ } rule_name;
#endif
/*     Another issue is failure. A sufficiently complex rule could have a loop
 *     and then follow it up with data that partially matches the loop structure.
 *     An example of this could be a comma separated structure that requires at
 *     least one element:
 *
 *         csv -> (/[0-9]+/ ',') * /[0-9]+/ ;
 *
 *     In this example it would only fail when it tried to consume the comma.
 *     If this happens I need to unwind the parser and then proceed to the next
 *     element. This works seamlessly with star loops, which are always valid even
 *     when they contain nothing (in which case an allocation should not be made),
 *     but the plus loop needs to have another fail case if it doesn't get at least
 *     a single match.
 *
 *     Note on unwinding: unlike the rest of the the nodes, a loop will only
 *     unwind to the most recent loop start.
 *
 * 3. fail states | STATUS: NOT STARTED
 *     So I think the correct approach to this problem is unwinding. I need to
 *     mark the advancement, and when a fail state occurs I should go back to the
 *     mark and 'goto' a label at the end of the section.
 *
 *     If the fail state happens at the top level (in a subrule with nothing above)
 *     the parser must correctly choose between returning NULL and moving on to the
 *     next subrule.
 *
 *     Failure jumps or post checks? A failure jump is a direct method. If an
 *     expression doesn't parse a goto/break is called and the code jumps out of the
 *     section, possibly to some error handling code. Post checks, on the other hand,
 *     are lazily evaluated. A note is made about whether or not an element succeeded,
 *     and then the parent condition uses that information to unwind and possibly
 *     produce it's own error condition.
 *
 *     I don't think that either strategy works well on it's own, the lazy checking is
 *     just confusing, but the failure jumps won't communicate upwards to unwind.
 *     There is a bit of a dichotomy between structs/unions and everything else. These
 *     need to fail after one bad parse element, and sub elements need to signal that
 *     they failed, but not necessarily unwind.
 *
 *     Unwinding - a whole problem unto itself
 *     Unwinding occurs when a segment has been partially consumed and fails half way
 *     through. Within a single subrule this can be limited to structs and unions.
 *     When tokens, expressions, and captures fail they don't actually progress the
 *     parser forwards. (Expressions are also weird as shown soon) Outside of a single
 *     subrule, when one subrule fails it needs to progress to the next item.
 *
 *     I think all of this can be achieved using this paradigm: whenever anything fails
 *     it must leave a check variable visible for the parent to check. A struct/union
 *     will check each one afterwards and in the event of failure it will jump to
 *     the end, unwind, and leave it's own check. At the rule level, each subrule
 *     checks for failure and either returns a node or moves on to the next subrule.
 *     I don't think subrule failure needs to unwind, because it will have already been
 *     done.
 *
 *     Adding to this, I don't think that unions need to unwind either. An or node
 *     will exit on the first successful condition so it will never be in the critical
 *     state: having successfully parsed part of an element before failing.
 *
 *     While this ~is~ *was* a very simple concept, implementation can get confusing,
 *     so here are some examples of generated code:
 *
 *         rule -> 'let' /[a-zA-Z]/ '=' expression
 *              -> descent
 *              ;
 *
 *     Translates to */
#if 0
		typedef struct {
        	string_t  cap00;
            Expr     *cap01;
        } rule ;

        Expr *rule(Parse *parse) {
        	int mark = parse->pos;
            {
            	consume("let");
            	/* ... */
            }
        }
#endif
/*     Loops are weird as noted above.
 *
 * 4. single member rules/subrules STATUS: NOT STARTED
 *     I think the obvious thing to do is to return just that element. This is easy
 *     to do with rule calls, but captures and tokens should have a slightly more
 *     sophisticated system to promote them to Expr *.
 *
 *     Zero member subrules should be a compile error to prevent me from doing
 *     something stupid such as:
 *
 *         consume -> 'i' 'have' 'no' 'soul' ;
 *
 * 5. or nodes fail state | STATUS: NOT STARTED
 *     If one of the or conditions fails it should move on to the next one in the
 *     same way that the subrule decent happens. Unwinding happens to the point
 *     where the or node starts. This also needs some level of awareness to produce
 *     a proper fail when none of the options succeed.
 *
 *     Related note: optional nodes work in the obvious way, unwinding failure and
 *     wrapping it in a boolean. */

// switches calls from concatf and concat depending on vargs
#define EMIT_CODE(str, ...) \
	concat ## __VA_OPT__(f) ## _cstr(&pass->rule_defs, str __VA_OPT__(,) __VA_ARGS__)

#define EMIT_CHAR(char) addchar(&pass->rule_defs, char)

#define cg pass->cg

static void compile_rule(Rule *rule, CompilePass *pass) {
	if (rule->length <= 0 || rule->cases[0]->tag != DEP_GRAMMAR) {
		error("Rule '%s' has no grammar", rule->name.str);
		return;
	}

	// does not need to keep track of what type it is
	EMIT_CODE(	"static Expr *rf_%s(Parse *parse) {\n"
					"Expr *expr = new_ast(parse);\n",
			rule->name.str);

	pass->check_num = 0;
	for (int i = 0; i < rule->length; i++) {
		EMIT_CODE("");
		compile(rule->cases[i], pass);

		// if not check -> unwinding is already done I think
		EMIT_CODE(	"if (check%d) {\n"
						"return expr;\n"
					"}\n",
				pass->check_num);

		pass->check_num++;
	}

	EMIT_CODE(		"free_ast(expr, parse);\n"
					"return NULL;\n"
				"}\n");
}

static void compile_grammar(Grammar *grammar, CompilePass *pass) {
	if (grammar->length == 0) {
		error("Expected grammar to contain members");
		return;
	}

	u16 current_check = pass->check_num + 1;
	u16 current_cap   = pass->cap_num   + 1;
	EMIT_CODE(	"bool check%d = true;\n"
				"do {\n"
					"__auto_type cap0 = &cap%d->cap0;\n"
					"u32 unwind_pos = parse->pos;\n",
			current_check, current_cap - 1);

	pass->check_num = 0;
	pass->cap_num   = 0;

	for (int i = 0; i < grammar->length; i++) {
		u16 cap = pass->cap_num; // WARN: this is a bad name, make more clear
		compile(grammar->tokens[i], pass);

		EMIT_CODE(	"if (!check%d) {\n"
						"check%d = false;\n"
						"parse->pos = unwind_pos;\n"
						"break;\n"
					"} else {\n",
				pass->check_num, current_check);

		if (pass->cap_num > cap) {
			EMIT_CODE("cap0->cap%d = ");
		}

		EMIT_CODE("}\n");
		pass->check_num++;
	}

	EMIT_CODE("} while(0);");
	pass->check_num = current_check;
	pass->cap_num   = current_cap;
}

static void compile_or(OrNode *or, CompilePass *pass) {
	u16 current_check = pass->check_num;
	u16 current_cap   = pass->cap_num;

	EMIT_CODE(	"__auto_type cap%d = &cap%d.cap%d;\n"
				"bool check%d = false;\n"
				"do {\n",
			current_check + 1, current_check,
			current_check + 1);

	for (int i = 0; i < or->length; i++) {
		compile(or->options[i], pass);

		EMIT_CODE(	"if (check%d) {\n"
						"cap%d->cap%d = %d;\n" // TODO: add capture
						"check%d = true;\n"
						"break;\n"
					"}\n",
				current_check + 1, pass->check_num,
				or->length, i, current_check);
	}

	EMIT_CODE("} while(0);\n");

	pass->check_num = current_check + 1;
	pass->cap_num   = current_cap   + 1;
}

static void compile_star(Expr *star, CompilePass *pass) {
	UNIMPLEMENTED();
}

static void compile_ident(DepToken *ident, CompilePass *pass) {
	UNIMPLEMENTED();
}

static void compile_capture(DepToken *ident, CompilePass *pass) {
	UNIMPLEMENTED();
}

static void compile_token(DepToken *ident, CompilePass *pass) {
	UNIMPLEMENTED();
}

#undef EMIT_CODE
#undef EMIT_CHAR

static void compile(Expr *expr, CompilePass *pass) {
	switch (expr->tag) {
		case DEP_RULE:    compile_rule(&expr->rule, pass); break;
		case DEP_GRAMMAR: compile_grammar(&expr->grammar, pass); break;
		case DEP_OR:      compile_or(&expr->ornode, pass); break;

		case DEP_GROUP:   compile(expr->group, pass); break;

		case DEP_STAR:    compile_star(expr->star, pass); break;

		case DEP_IDENT:   compile_ident(&expr->token, pass); break;
		case DEP_REGEX:   compile_capture(&expr->token, pass); break;
		case DEP_STRING:  compile_token(&expr->token, pass); break;
		default:
			PANIC("Tried to compile unimplemented node");
	}
}

void compile_pass(Expr *exprs[], RuleNode *rules[], SymbolPass *pass, uint32_t num) {
	CompilePass compile_pass = {
		// .asts		= exprs,
		.rules		= rules,
		.current	= 0,
		.nesting	= 0,
		.check_num	= 0,
		.tokens		= pass->tokens,
		.captures	= pass->captures,
		.rule_defs	= init_str(""),
	};

	pass->tokens = (HashSet) {
		.cap	= 0,
		.len	= 0,
		.items	= NULL,
	};

	pass->captures = (HashSet) {
		.cap	= 0,
		.len	= 0,
		.items	= NULL,
	};

	for (int i = 0; i < num; i++) {
		Expr *const expr = exprs[i];

		compile(expr, &compile_pass);
		compile_pass.current++;
	}
}
