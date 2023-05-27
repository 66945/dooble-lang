#pragma once

// force memchecks
#include "../utils/utils.h"
#include "type.h"

// package management
#ifdef DOOBLE_EXTERNAL
#	error "internal.h cannot be included outside main package"
#endif

#define DOOBLE_INTERNAL

// A parsed token
typedef struct {
	enum : u8 {
		// keywords
		DB_ALLOC,   DB_AND,      DB_BREAK,    DB_CASE,
		DB_CO,      DB_CONTINUE, DB_DEFER,    DB_DO,
		DB_DONT,    DB_ELSE,     DB_ELIF,     DB_FALL,
		DB_FALSE,   DB_FINAL,    DB_FOR,      DB_FREE,
		DB_IF,      DB_IN,       DB_IS,       DB_INCLUDE,
		DB_MAP,     DB_MATCH,    DB_NIL,      DB_NOT,
		DB_OR,      DB_PACKAGE,  DB_PROTOCOL, DB_PROTECT,
		DB_PUB,     DB_RETURN,   DB_STATIC,   DB_STRUCT,
		DB_SUMTYPE, DB_TEST,     DB_TRUE,     DB_VEC,
		DB_YIELD,

		// operators
		DB_AMPER, DB_BITOR,  DB_BITNOT,  DB_DOTDOT, DB_DOTDOTDOT,
		DB_LESS,  DB_LESSEQ, DB_GREATER, DB_GREATEREQ,

		// symbols
		DB_EQUAL,   DB_COLON,   DB_LBRACE, DB_RBRACE,
		DB_LSQUARE, DB_RSQUARE, DB_LPAREN, DB_RPAREN,
		DB_STAR,    DB_PLUS,    DB_SLASH,  DB_MINUS,
		DB_QUEST,   DB_DOT,     DB_COMMA,  DB_SEMI,
		DB_ARROW,   DB_BANG,

		// tokens
		DB_NUM, DB_FLOAT, DB_STR, DB_IDENT,

		DB_EOF = UINT8_MAX,
	} token;

	union {
		string_t str;
		intmax_t vali;
		double   valf;
	};

	size_t linenum;
} DoobleToken;

// returns the size of tokens, and fills `tokens` with an array
u32  get_tokens(cstr buffer, DoobleToken **tokens /* ref to array */);
void print_tokens(size_t N, DoobleToken tokens[N]);
void free_tokens(u32 len, DoobleToken tokens[len]);

// ========================================================   
// ================= Abstract Syntax Tree ================= 
// ========================================================   

typedef struct Node_t Node;

// statements
typedef struct {
	Node *condition; // ref
	Node *stmt;      // ref
	Node *else_case; // ref
} IfStmt;

typedef struct {
	bool      by_reference;
	string_t  ident;
	Node     *range; // ref
	Node     *stmt;  // ref
} ForEach;

typedef struct {
	Node *condition; // ref
	Node *stmt;      // ref
} ForWhile;

typedef VEC(Node *) Block;

// declarations
typedef struct {
	string_t  name;
	bool      is_const;
	typeid    type;
	Node     *assign; // ref
	struct {
		bool is_static:  1;
		bool is_pub:     1;
		bool is_co:      1;
		bool is_protect: 1;
		bool is_final:   1;
	} quals;
} Declaration;

// expressions

// TODO: figure out a way to keep the parameter arr in the same arena as everything else
typedef struct {
	Node  *caller; // ref
	Node **params; // arr of ref
	u32    len;
} Call;

typedef struct {
	Node     *expr; // ref
	string_t  name;
} SubMember;

typedef struct {
	u8    operator;
	Node *expr; // ref
} Unary;

typedef struct {
	u8    operator;
	Node *expra; // ref
	Node *exprb; // ref
} BinOp;

// literal
typedef VEC(Node *) Arguments;

// A function is an atomic value but it is not a literal. The difference lies mostly in
// an arbitrary decision I made while bored. It doesn't really affect anything other
// than literals are held within the Literal union
typedef struct {
	typeid     ret_type;
	Arguments  args;
	Node      *block;
} Function;

// THING :: OTHER
// OTHER :: THING
//
// BLOB :: THING
//
// FINE :: JOHN
// JOHN :: BOB + TOM
// BOB  :: 0
// TOM  :: BOB
//
// dependencies
// THING -> OTHER -> THING    -> ...
// OTHER -> THING -> OTHER    -> ...
// BLOB  -> THING -> OTHER    -> THING -> ...
// FINE  -> JOHN  -> BOB, TOM -> BOB
// JOHN  ->
// BOB   -> :)
// TOM   -> BOB

typedef struct {
	enum {
		LIT_STR,
		LIT_IDENT,
		LIT_BOOL,
		LIT_NUM,
		LIT_FLT,
		LIT_NIL,
	} tag;

	union {
		string_t str;
		bool     boolean;
		intmax_t numi;
		double   numf;
	};
} Literal;

// sum of all nodes
struct Node_t {
	enum {
		EX_PASS,
		EX_IF,
		EX_FOREACH,
		EX_FORWHILE,
		EX_DOEACH,
		EX_DOWHILE,
		EX_DONTEACH,
		EX_DONTWHILE,
		EX_BLOCK,
		EX_DECL,
		EX_BINOP,
		EX_UNARY,
		EX_CALL,
		EX_SUBMEMBER,
		EX_FUNCTION,
		EX_LITERAL,
	} tag;

	union {
		IfStmt      ifstmt;
		ForEach     foreach;
		ForWhile    forwhile;
		Block       block;
		Declaration declare;
		BinOp       binop;
		Unary       unary;
		Call        call;
		SubMember   member;
		Function    function;
		Literal     literal;
	};
};

typedef enum {
	PARSE_OK  = 0b0000000000000000,
	PARSE_ERR = 0b0000000000000001,
} ParseError;

typedef struct {
	Node       *pool;
	size_t      pool_size;
	ParseError  err;
} AstResult;

/**
 * Builds an abstract syntax tree
 * (frees the token list after building the tree)
 *
 * @param N the length of tokens
 * @param tokens the list of tokens to parse (takes ownership)
 * @param buf the original file the tokens were produced from
 * @return an abstract syntax tree
 * */
AstResult get_ast(size_t N, DoobleToken tokens[N], TypeTree *tree, const char *const buf);
void      print_ast(Node *node);
void      free_ast(size_t N, Node pool[N]);
