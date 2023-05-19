#pragma once

#include "../strutils/str.h"
#include "../utils/utils.h"

typedef struct TargetAST_t  TargetAST;
typedef struct CType_t      CType;

/* Fantastic Types and How to Store Them:
 *
 * NOTE: This is wrong, but I need it for reference right now
 * ============================================================
 * As far as I can tell there are 3 main things I have to store
 * regarding type information: the name, the pointer level and
 * if it is a defined pointer size.
 *
 * Dooble will have custom array structs to represent bounds checking,
 * but statically sized arrays still need to be defined in the C export.
 *
 * I forgot about function pointers >:(
 *
 * Data types can be thought of in a sequential order. pointer -> array -> type and
 * so on.
 *
 * 2-D and N-D array representation is still up in the air at this point. */
typedef struct {
	enum {
		TYPE_ARR,
		TYPE_PTR,
	} tag;

	union {
		u32  arr_size;
		bool is_const;
	};
} CTypeElement;

struct CType_t {
	struct {
		CTypeElement *arr;
		u32           len;
		u32           cap;
	} modifiers;

	struct {
		CType *arr;
		u32    len;
	} params;

	bool     is_const;
	bool     is_volatile;
	string_t typename;
};

typedef struct {
	string_t name;
	u32      uid;
	CType    type;
	bool     is_static;
	bool     is_extern;
} Identifier;

CType make_ctype(void);
void  add_ptr(CType *type, bool is_const);
void  make_fn_ptr(CType *type, size_t count, CType params[count]);
void  add_arr(CType *type, size_t size);
void  add_name(CType *type, cstr name);

// consumes type properly
Identifier make_identifier(cstr name, CType *type, bool is_static, bool is_extern);

/* CExpression formatting:
 * emit '$ + $'
 * emit '$ + $'
 * emit 4
 * emit 2
 * emit 5
 * 4 + 2 + 5 */
typedef char *CExpression;
typedef i64   TargetHandle;

// NOTE: cannot obscure this type because I need to know what size they are
// in another file
typedef struct {
	string_t      c_output;
	TargetAST    *ast_stack; // arr
	size_t        cap;
	size_t        stack_top;
	size_t        traverse;
	u8            indent_level;
	TargetHandle  active_scope;
} CodeGen;

// NOTE: although the only reason is that I don't want to deal with freeing CodeGen
// or the extra allocation.

// NOTE: which, to be fair, is a pretty good reason

void     init_CodeGen(CodeGen *cg);
string_t get_generated(CodeGen *cg);

// The macro system assumes that all CodeGen variables are named 'cg' and are pointers.
// `EMIT_SETUP` sets up a literal alias. Make sure that codegen_lit is not a pointer, or
// named 'cg'
#define EMIT_SETUP(codegen_lit) CodeGen *cg = &(codegen_lit)

// basic functions: aliased macros for the actual calls, but don't add functionality
#define EMIT_SCOPE()           emit_scope(cg)
#define EMIT_SCOPE_END()       emit_scope_end(cg)
#define EMIT_STATEMENT()       emit_statement(cg)
#define EMIT_PARAM(name, type) emit_parameter(cg, type, name)
#define EMIT_EXPR(expr)        emit_expression(cg, expr)

#define EMIT_IDENT(name, is_static, is_extern, type) \
	emit_identifier(cg, is_static, is_extern, type, name)

// advanced functions: aliased 'emit_expression' calls with prebuilt expressions
#define EMIT_BINOP(op)   emit_expression(cg, "$ " #op " $")
#define EMIT_IF()        emit_expression(cg, "if ($)")
#define EMIT_FOR()       emit_expression(cg, "for ($; $; $)")
#define EMIT_RETURN()    emit_expression(cg, "return ")
#define EMIT_CALL(fn, n) emit_call(cg, #fn, n)
#define EMIT_ASSIGN(v)   emit_expression(cg, #v " = ")

#define EMIT_ATOMIC(atom) emit_expression(cg, #atom)

#define EMIT_FUNC(name, is_static, type, size, arr)           \
	do {                                                      \
		emit_function(cg, is_static, type, #name, size, arr); \
		EMIT_SCOPE();                                         \
	} while (0)

#define EMIT_STRUCT()                    \
	do {                                 \
		emit_expression(cg, "struct ");  \
		EMIT_SCOPE();                    \
	} while (0)
#define EMIT_TYPE_END()   \
	do {                  \
		EMIT_SCOPE_END(); \
		EMIT_STATEMENT(); \
	} while (0)

#define EMIT_RETVAL(val)                     \
	do {                                     \
		emit_expression(cg, "return " #val); \
		emit_statement(cg);                  \
	} while (0)

#define EMIT_TYPEDEF()

void emit_scope(CodeGen *cg);
void emit_scope_end(CodeGen *cg);
void emit_statement(CodeGen *cg);
void emit_expression(CodeGen *cg, CExpression expr);
void emit_call(CodeGen *cg, cstr name, u32 params);

void emit_identifier(
		CodeGen *cg,
		bool     is_static,
		bool     is_extern,
		CType    type,
		cstr     name);

void emit_function(
		CodeGen   *cg,
		bool       is_static,
		CType     *type,
		cstr       name,
		size_t     count,
		Identifier params[count]); // arr, uses * because of obscured type
