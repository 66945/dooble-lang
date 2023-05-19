#pragma once

#include "../strutils/str.h"
#include <stdint.h>

// TODO: rename to DEP_
typedef struct {
	enum {
		TOK_ARROW,  TOK_OR,
		TOK_LPAREN, TOK_RPAREN,
		TOK_STAR,   TOK_PLUS,
		TOK_SEMI,   TOK_STRING,
		TOK_REGEX,  TOK_IDENTIFIER,
		TOK_BAD,    TOK_END,
		TOK_OPT,
	} type;

	string_t value;
	uint32_t line;
} Token;

typedef struct {
	uint32_t	pos;
	uint32_t	line;
	string_t	*str;
	Token		*tokens;
	uint32_t	token_cap;
	uint32_t	token_len;
} Scan;

void scan(string_t *source, Scan *scan);
Token get_token(Scan *scan);
void print_tokens(Scan *scan);

bool free_scan(Scan *scan);
bool free_tokens(Token *tokens, uint32_t token_len);
