#include "internal.h"
#include "../utils/utils.h"
#include "../utils/hash.h"
#include <ctype.h>
#include <stdlib.h>

#define KEYWORDS_LEN 37
static const char *const KEYWORDS[] = {
	[DB_ALLOC]    = "alloc",
	[DB_AND]      = "and",
	[DB_BREAK]    = "break",
	[DB_CASE]     = "case",
	[DB_CO]       = "co",
	[DB_CONTINUE] = "continue",
	[DB_DEFER]    = "defer",
	[DB_DO]       = "do",
	[DB_DONT]     = "don't",
	[DB_ELSE]     = "else",
	[DB_ELIF]     = "elif",
	[DB_FALL]     = "fall",
	[DB_FALSE]    = "false",
	[DB_FINAL]    = "final",
	[DB_FOR]      = "for",
	[DB_FREE]     = "free",
	[DB_IF]       = "if",
	[DB_IN]       = "in",
	[DB_IS]       = "is",
	[DB_INCLUDE]  = "include",
	[DB_MAP]      = "map",
	[DB_MATCH]    = "match",
	[DB_NIL]      = "nil",
	[DB_NOT]      = "not",
	[DB_OR]       = "or",
	[DB_PACKAGE]  = "package",
	[DB_PROTOCOL] = "protocol",
	[DB_PROTECT]  = "protect",
	[DB_PUB]      = "pub",
	[DB_RETURN]   = "return",
	[DB_STATIC]   = "static",
	[DB_STRUCT]   = "struct",
	[DB_SUMTYPE]  = "sumtype",
	[DB_TEST]     = "test",
	[DB_TRUE]     = "true",
	[DB_VEC]      = "vec",
	[DB_YIELD]    = "yield",
};

static HashSet KEYWORD_SET;

typedef struct {
	const char *buffer;
	size_t      bufsize;
	u32         pos;
	u32         line;
	u32         line_start;

	struct {
		DoobleToken *arr;
		u32          cap;
		u32          len;
	} tokens;
} Lexer;

static char advance(Lexer *l) {
	if (l->pos >= l->bufsize)
		return 0;

	return l->buffer[l->pos++];
}

static char peek(Lexer *l, u16 ahead) {
	if (l->pos + ahead >= l->bufsize) return 0;
	return l->buffer[l->pos + ahead];
}

static bool match(Lexer *l, char ch) {
	if (peek(l, 0) == ch) {
		l->pos++;
		return true;
	}

	return false;
}

static void append_token(Lexer *l, u8 toknum) {
	if (l->tokens.len >= l->tokens.cap) {
		l->tokens.cap *= 2;
		let tmp = realloc(l->tokens.arr, l->tokens.cap);

		if (tmp == NULL) {
			free(tmp);
			PANIC("could not extend tokens");
		}

		l->tokens.arr = tmp;
	}

	let tok = (DoobleToken) {
		.token   = toknum,
		.linenum = l->line,
	};

	l->tokens.arr[l->tokens.len++] = tok;
}

/* This should allow for the preservation of some syntax items:
 * builder.set_hello().
 *     add_param().
 *     add_param().
 *     add_param().
 *     build() +;
 *
 * item := map[str,int] {
 * } +;
 * */
static void auto_semicolon(Lexer *l) {
	if (l->tokens.len == 0) return;

	let tok = l->tokens.arr[l->tokens.len - 1].token;
	if (tok != DB_DOT &&
		tok != DB_COMMA &&
		tok != DB_LBRACE &&
		tok != DB_LPAREN &&
		tok != DB_LSQUARE &&
		tok != DB_SEMI)
	{
		append_token(l, DB_SEMI);
	}
}

static void lex_string(Lexer *l) {
	bool     is_valid = true;
	string_t str      = init_str("");

	while (l->pos < l->bufsize && peek(l, 0) != '\'') {
		if (peek(l, 0) == '\n') {
			error_file("string cannot contain new lines", l->line, l->buffer);
			l->line++;
			l->line_start = l->pos;
			is_valid      = false;
		}

		if (is_valid) addchar(&str, advance(l));
	}

	if (l->pos >= l->bufsize) {
		error_file("string must end with a '", l->line, l->buffer);
	} else {
		advance(l); // consume '
	}

	append_token(l, DB_STR);
	l->tokens.arr[l->tokens.len - 1].str = str;
}

static void lex_identifier(Lexer *l) {
	l->pos -= 1; // backtrack

	string_t ident = init_str("");

	while (l->pos < l->bufsize) {
		char ch = peek(l, 0);

		if (isalnum(ch) || ch == '_') {
			addchar(&ident, advance(l));
		}
		else break;
	}

	if (has_item(&KEYWORD_SET, ident.str)) {
		append_token(l, get_item(&KEYWORD_SET, ident.str));
		freestr(&ident);
		return;
	}

	append_token(l, DB_IDENT);
	l->tokens.arr[l->tokens.len - 1].str = ident;
}

// handle differently formatted numbers:
// - 0xFF0fd (hex with multiple caps)
// - 0o12722 (oct)
// - 0b01011 (binary)
// - 123456f (floating point)
// - 123.456 (floating point)
// - 123_456 (int with seperators)
static void lex_number(Lexer *l) {
	enum {
		NUM_UNDECIDED,
		NUM_BIN,
		NUM_OCT,
		NUM_HEX,
		NUM_FLOAT,
	} numfmt = NUM_UNDECIDED;

	l->pos--; // get first digit
	string_t numstr = init_str("");
	char     ch     = 0;

	for (int i = 0; (ch = advance(l)) != 0; i++) {
		if (i == 1 && numstr.str[0] == '0') {
			if (ch == 'b') numfmt = NUM_BIN;
			if (ch == 'o') numfmt = NUM_OCT;
			if (ch == 'x') numfmt = NUM_HEX;
		}

		else if (numfmt == NUM_UNDECIDED && ch == '.') {
			numfmt = NUM_FLOAT;
			addchar(&numstr, ch);
		} else if (ch == '.') {
			error_file("number has invalid decimal", l->line, l->buffer);
			goto invalid;
		}

		else if (ch == '_') continue;

		else if (numfmt == NUM_BIN && ch != '0' && ch != '1' && isalnum(ch)) {
			error_file("invalid digit '%c' in binary literal",
					l->line, l->buffer, ch);
			goto invalid;
		}

		else if (numfmt == NUM_OCT && (ch < '0' || ch > '7')) {
			error_file("invalid digit '%c' in octal literal",
					l->line, l->buffer, ch);
			goto invalid;
		}

		else if (numfmt == NUM_HEX && isalnum(ch)) {
			switch (ch) {
				case '0' ... '9':
				case 'a' ... 'f':
				case 'A' ... 'F':
					addchar(&numstr, ch);
					break;

				default:
					error_file("invalid digit '%c' in hex literal",
							l->line, l->buffer, ch);
					goto invalid;
			}
		}

		else if (ch >= '0' && ch <= '9') {
			addchar(&numstr, ch);
		}

		else {
			l->pos--; // backtrace
			break;
		}
	}

	double    numf = 0;
	long long numi = 0;

	switch (numfmt) {
		case NUM_FLOAT:
			numf = atof(numstr.str);
			append_token(l, DB_FLOAT);
			l->tokens.arr[l->tokens.len - 1].valf = numf;
			break;
		case NUM_UNDECIDED:
			numi = strtoll(numstr.str, NULL, 10);
			break;
		case NUM_BIN:
			numi = strtoll(numstr.str, NULL, 2);
			break;
		case NUM_OCT:
			numi = strtoll(numstr.str, NULL, 8);
			break;
		case NUM_HEX:
			numi = strtoll(numstr.str, NULL, 16);
			break;
	}

	if (numfmt != NUM_FLOAT) {
		append_token(l, DB_NUM);
		l->tokens.arr[l->tokens.len - 1].vali = numi;
	}

	freestr(&numstr);
	return;

invalid:
	// unwind
	do ch = advance(l); while (!isalnum(ch) && ch != '_');
	freestr(&numstr);
}

#define INIT_TOKENS_LEN 30
u32 get_tokens(cstr buffer, DoobleToken **tokens) {
	Lexer lex = {
		.buffer     = buffer,
		.bufsize    = strlen(buffer),
		.pos        = 0,
		.line       = 0,
		.line_start = 0,

		.tokens = {
			.arr = malloc(sizeof(DoobleToken) * INIT_TOKENS_LEN),
			.cap = INIT_TOKENS_LEN,
			.len = 0,
		},
	};

	for_range (i, lex.bufsize) {
		char ch;
		switch (ch = advance(&lex)) {
			case '\n':
				if (peek(&lex, 0) != '\n') {
					auto_semicolon(&lex);
				}

				lex.line++;
				lex.line_start = lex.pos;
				fallthrough;
			case '\t':
				fallthrough;
			case ' ':
				break;

			case '&': append_token(&lex, DB_AMPER);   break;
			case '|': append_token(&lex, DB_BITOR);   break;
			case '~': append_token(&lex, DB_BITNOT);  break;
			case '=': append_token(&lex, DB_EQUAL);   break;
			case ':': append_token(&lex, DB_COLON);   break;
			case '{': append_token(&lex, DB_LBRACE);  break;
			case '}': append_token(&lex, DB_RBRACE);  break;
			case '[': append_token(&lex, DB_LSQUARE); break;
			case ']': append_token(&lex, DB_RSQUARE); break;
			case '(': append_token(&lex, DB_LPAREN);  break;
			case ')': append_token(&lex, DB_RPAREN);  break;
			case '*': append_token(&lex, DB_STAR);    break;
			case '+': append_token(&lex, DB_PLUS);    break;
			case '/': append_token(&lex, DB_SLASH);   break;
			case '?': append_token(&lex, DB_QUEST);   break;
			case ',': append_token(&lex, DB_COMMA);   break;
			case ';': append_token(&lex, DB_SEMI);    break;

			case '<':
				append_token(&lex, match(&lex, '=') ? DB_LESSEQ : DB_LESS);
				break;
			case '>':
				append_token(&lex, match(&lex, '=') ? DB_GREATEREQ : DB_GREATER);
				break;

			case '.':
				if (match(&lex, '.')) {
					append_token(&lex, match(&lex, '.') ? DB_DOTDOTDOT : DB_DOTDOT);
				}

				append_token(&lex, DB_DOT);
				break;

			case '-':
				if (match(&lex, '-')) {
					while (lex.pos < lex.bufsize && peek(&lex, 0) != '\n')
						advance(&lex);
				}

				else if (match(&lex, '>')) append_token(&lex, DB_ARROW);
				else                       append_token(&lex, DB_MINUS);
				break;

			case '\'':
				lex_string(&lex);
				break;

			default:
				if (isalpha(ch)) {
					lex_identifier(&lex);
				}

				// apparently isnum isn't a function?
				else if(ch >= '0' && ch <= '9') {
					lex_number(&lex);
				}
				break;
		}
	}

	append_token(&lex, DB_EOF);

	*tokens = lex.tokens.arr;
	return lex.tokens.len;
}

void print_tokens(size_t N, DoobleToken tokens[N]) {
	for_range (i, N) {
		logger("Tok%0d ", tokens[i].token);

		switch (tokens[i].token) {
			case DB_ALLOC ... DB_YIELD: // keywords
				logger("\t[key: \"%s\"]", KEYWORDS[tokens[i].token]);
				break;
			case DB_NUM:
				logger("\t[num: %d]", tokens[i].vali);
				break;
			case DB_FLOAT:
				logger("\t[num: %f]", tokens[i].valf);
				break;
			case DB_STR:
				fallthrough;
			case DB_IDENT:
				logger("\t[ident: \"%s\"]", tokens[i].str.str);
				break;
			default:
				break;
		}
	}
}

void free_tokens(u32 len, DoobleToken tokens[len]) {
	for_range(i, len) {
		const u8 tok = tokens[i].token;
		if (tok == DB_STR || tok == DB_IDENT) {
			freestr(&tokens[i].str);
		}
	}

	free(tokens);
}

startup static void init_keywords(void) {
	KEYWORD_SET= init_hashset();

	for_range (i, KEYWORDS_LEN) {
		add_item(&KEYWORD_SET, KEYWORDS[i]);
	}
}
