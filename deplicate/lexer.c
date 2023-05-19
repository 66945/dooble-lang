#include "lexer.h"
#include "../strutils/str.h"
#include "../utils/err.h"
#include "../utils/utils.h"
#include "../testing/testing.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// private forward declarations
static char advance(Scan *scan);
static char peek(Scan *scan, int num);
static bool match(Scan *scan, char ch);

static Token string_token(Scan *scan, int type, char end);
static Token identifier_token(Scan *scan);
static void emit_token(Scan *scan, Token type);

#define TOKEN_INIT_CAPACITY 50
void scan(string_t *source, Scan *scan) {
	*scan = (Scan) {
		.pos		= 0,
		.line		= 0,
		.str		= source,
		.tokens		= malloc(sizeof(Token) * TOKEN_INIT_CAPACITY),
		.token_cap	= TOKEN_INIT_CAPACITY,
		.token_len	= 0,
	};

	char ch = 0;
	while (scan->pos < scan->str->size) {
		switch (ch = advance(scan)) {
			case '\n': scan->line++; fallthrough;
			case ' ': fallthrough;
			case '\t': break;
			case '|': emit_token(scan, (Token) { TOK_OR,     .line = scan->line }); break;
			case '(': emit_token(scan, (Token) { TOK_LPAREN, .line = scan->line }); break;
			case ')': emit_token(scan, (Token) { TOK_RPAREN, .line = scan->line }); break;
			case '+': emit_token(scan, (Token) { TOK_PLUS,   .line = scan->line }); break;
			case '*': emit_token(scan, (Token) { TOK_STAR,   .line = scan->line }); break;
			case ';': emit_token(scan, (Token) { TOK_SEMI,   .line = scan->line }); break;
			case '?': emit_token(scan, (Token) { TOK_OPT,    .line = scan->line }); break;
			case '\'': emit_token(scan, string_token(scan, TOK_STRING, '\'')); break;
			case '/': emit_token(scan, string_token(scan, TOK_REGEX, '/')); break;
			case '-':
				if (match(scan, '>'))
				    emit_token(scan, (Token) { TOK_ARROW, .line = scan->line });
				else
				    error_file("Dangling '-' token. Did you mean '->'?",
				  		  scan->line, scan->str->str);
				break;
			case '#': fallthrough;
			case '=': // === style comments ===
				while (peek(scan, 0) != '\n') advance(scan);
				break;
			default:
				if (isalpha(ch) != 0)
				    emit_token(scan, identifier_token(scan));
				else
				    error_file("Bad token", scan->line, scan->str->str);
				break;
		}
	}

	emit_token(scan, (Token) { TOK_END, .line = scan->line });
}
#undef TOKEN_INIT_CAPACITY

void print_tokens(Scan *scan) {
	for (int i = 0; i < scan->token_len; i++) {
		switch (scan->tokens[i].type) {
			case TOK_ARROW:	printf("ARROW\n"); break;
			case TOK_OR: printf("OR\n"); break;
			case TOK_LPAREN: printf("LPAREN\n"); break;
			case TOK_RPAREN: printf("RPAREN\n"); break;
			case TOK_STAR: printf("STAR\n"); break;
			case TOK_PLUS: printf("PLUS\n"); break;
			case TOK_SEMI: printf("SEMICOLON\n"); break;
			case TOK_OPT: printf("QUESTIONMARK\n"); break;
			case TOK_STRING: printf("STRING: \"%s\"\n", scan->tokens[i].value.str); break;
			case TOK_REGEX: printf("REGEX: /%s/\n", scan->tokens[i].value.str); break;
			case TOK_IDENTIFIER:
				printf("IDENTIFIER: %s\n", scan->tokens[i].value.str); break;
			case TOK_BAD: printf("BAD\n"); break;
			case TOK_END: printf("END\n"); break;
		}
	}
}

bool free_scan(Scan *scan) {
	if (scan == NULL) return false;

	if (scan->tokens == NULL) return true; // not a fail condition

	if (free_tokens(scan->tokens, scan->token_len)) {
		scan->tokens    = NULL;
		scan->token_len = 0;
		scan->token_cap = 0;
	}
	
	return true;
}

bool free_tokens(Token *tokens, uint32_t token_len) {
	if (tokens == NULL) return false;

	for (int i = 0; i < token_len; i++) {
		if (tokens[i].type == TOK_STRING ||
				tokens[i].type == TOK_IDENTIFIER ||
				tokens[i].type == TOK_REGEX)
		{
			freestr(&tokens[i].value);
		}
	}

	free(tokens);
	return true;
}

static char advance(Scan *scan) {
	char ch = scan->str->str[scan->pos++];
	return ch;
}

// has "features"
static char peek(Scan *scan, int num) {
	if (scan->pos + num > scan->str->size) return '\0';
	return scan->str->str[scan->pos + num];
}

static bool match(Scan *scan, char ch) {
	if (peek(scan, 0) == ch) {
		scan->pos++;
		return true;
	}

	return false;
}

static Token string_token(Scan *scan, int type, char end) {
	string_t str = init_str("");

	char ch;
	while ((ch = peek(scan, 0)) != end && ch != '\0') {
		if (ch == '\n') {
			error_file("String cannot contain newline", scan->line, scan->str->str);
			scan->line++;
			advance(scan);
			break;
		}

		addchar(&str, advance(scan));
	}

	advance(scan); // consume end

	return (Token) {
		.type	= type,
		.value	= str,
		.line	= scan->line,
	};
}

static bool is_identifier(char ch) {
	if (isalnum(ch) != 0) return true;

	switch (ch) {
		case '_':
		case '?':
			return true;
		default:
			return false;
	}
}

static Token identifier_token(Scan *scan) {
	scan->pos--; // unconsume the first char
	string_t str = init_str("");

	char ch;
	while (is_identifier(ch = peek(scan, 0))) {
		addchar(&str, advance(scan));
	}

	return (Token) {
		.type	= TOK_IDENTIFIER,
		.value	= str,
		.line	= scan->line,
	};
}

static void emit_token(Scan *scan, Token token) {
	if (scan->token_len == scan->token_cap) {
		scan->token_cap *= 2;
		scan->tokens = realloc(scan->tokens, sizeof(Token) * scan->token_cap);
	}

	scan->tokens[scan->token_len++] = token;
}
