#include "codegen.h"
#include "../testing/testing.h"
#include "../utils/utils.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG_CODEGEN 0

// sorted alphabetically.
// a lot of these are C11+, some are C2X
#define RESERVED_KEYWORDS_LEN 60
static const char *const RESERVED_KEYWORDS[RESERVED_KEYWORDS_LEN] = {
	"alignas",
	"alignof",
	"auto",
	"bool",
	"break",
	"case",
	"char",
	"const",
	"constexpr",
	"continue",
	"default",
	"do",
	"double",
	"else",
	"enum",
	"extern",
	"false",
	"float",
	"for",
	"goto",
	"if",
	"inline",
	"int",
	"long",
	"nullptr",
	"register",
	"restrict",
	"return",
	"short",
	"signed",
	"sizeof",
	"static",
	"static_assert",
	"struct",
	"switch",
	"thread_local",
	"true",
	"typedef",
	"typeof",
	"typeof_unqual",
	"union",
	"unsigned",
	"void",
	"volatile",
	"while",
	"_Alignas",
	"_Alignof",
	"_Atomic",
	"_BitInt",
	"_Bool",
	"_Complex",
	"_Decimal128",
	"_Decimal32",
	"_Decimal64",
	"_Generic",
	"_Imaginary",
	"_Noreturn",
	"_Static_assert",
	"_Thread_local",
	"NULL",
};

// OPTIMIZE: would a binary search be faster? My array can be perfectly sorted, but
// it isn't that large, and such an optimization might actually make it slower.
// It is a performance concern because most use cases are the worst case scenario.
static bool check_reserved(const char *keyword) {
	for_range (i, RESERVED_KEYWORDS_LEN) {
		if (strcmp(keyword, RESERVED_KEYWORDS[i]) == 0)
			return true;
	}

	return false;
}

CType make_ctype(void) {
	return (CType) {
		.modifiers = {
			.arr = malloc(sizeof(CTypeElement) * 2),
			.len = 0,
			.cap = 2,
		},
		.params    = {
			.arr = NULL,
			.len = 0,
		},

		.is_const    = false,
		.is_volatile = false,
		.typename    = { NULL, 0, 0 },
	};
}

void add_ptr(CType *type, bool is_const) {
	if (type->modifiers.len >= type->modifiers.cap) {
		type->modifiers.cap *= 2;
		let tmp = realloc(type->modifiers.arr, type->modifiers.cap);
		if (tmp == NULL) {
			free(tmp);
			PANIC("cannot extend array");
		}

		type->modifiers.arr = tmp;
	}

	type->modifiers.arr[type->modifiers.len++] = (CTypeElement) {
		.tag      = TYPE_PTR,
		.is_const = is_const,
	};
}

// Consumes function pointer type parameters
void make_fn_ptr(CType *type, size_t count, CType params[count]) {
	type->params.arr = malloc(sizeof(CType) * count);
	type->params.len = count;

	for_range (i, count) {
		type->params.arr[i] = params[i];
		// Ownership semantics exist here, but do not free. Move semantics?
		params[i] = (CType) {0};
	}
}

void add_arr(CType *type, size_t size) {
	EXTEND_ARR(CTypeElement,
			type->modifiers.arr,
			type->modifiers.len,
			type->modifiers.cap);

	type->modifiers.arr[type->modifiers.len++] = (CTypeElement) {
		.tag      = TYPE_ARR,
		.arr_size = size,
	};
}

void add_name(CType *type, cstr name) {
	type->typename = init_str(name);
}

// consumes type
Identifier make_identifier(cstr name, CType *type, bool is_static, bool is_extern) {
	static u32 uid = 0;

	Identifier to_return = {
		.name      = init_str(name),
		.uid       = uid++,
		.type      = *type,
		.is_static = is_static,
		.is_extern = is_extern,
	};

	// move type
	*type = (CType) {0};
	return to_return;
}

typedef struct {
	TargetHandle  parent;
	Identifier   *identifiers; // arr
	u32           ident_len;
	u32           ident_cap;
} Scope;

typedef struct {
	string_t    name;
	CType       return_type;
	Identifier *parameters; // arr
	u32         param_len;
	u32         param_cap;
	bool        is_static;
} Function;

typedef struct {
	string_t name;
	u32      params;
} FunctionCall;

struct TargetAST_t {
	enum {
		TAR_IDENTIFIER,
		TAR_SCOPE,
		TAR_SCOPE_END,
		TAR_STATEMENT,
		TAR_FUNCTION,
		TAR_CALL,
		TAR_EXPR,
	} type;

	union {
		Identifier   ident;
		Scope        scope;
		Scope       *scope_end; // ref
		Function     func;
		FunctionCall call;
		CExpression  expr;
	};
};

#define CODEGEN_STACK_SIZE 200
void init_CodeGen(CodeGen *cg) {
	*cg = (CodeGen) {
		.c_output     = init_str(""),
		.ast_stack    = malloc(sizeof(TargetAST) * CODEGEN_STACK_SIZE),
		.cap          = CODEGEN_STACK_SIZE,
		.stack_top    = 0,
		.active_scope = -1,
	};
}

static TargetAST *consume(CodeGen *cg) {
	if (cg->traverse == cg->stack_top) {
		PANIC("could not consume element, at end of stack");
	}

	return &cg->ast_stack[cg->traverse++];
}

// MARK: memory will be freed as the generated blocks are consumed
static void generate(CodeGen *cg, TargetAST *node);

static void indent(CodeGen *cg) {
	string_t *const cout = &cg->c_output;

	if (cout->size > 0 && cout->str[cout->size - 1] != '\n') {
		while (cout->str[cout->size - 1] != '\n') {
			removechar(cout, -1);
		}
	}

	for_range (i, cg->indent_level)
		addchar(&cg->c_output, '\t');
}

static void generate_statement(CodeGen *cg) {
	concat_cstr(&cg->c_output, ";\n");
	indent(cg);
}

static void generate_type(CodeGen *cg, CType *type, cstr name) {
	// aliases
	string_t *const cout = &cg->c_output;

	if (type->is_const)    concat_cstr(cout, "const ");
	if (type->is_volatile) concat_cstr(cout, "volatile ");

	concat(cout, &type->typename);
	addchar(cout, ' ');

	if (type->params.len > 0) addchar(cout, '(');

	for (int i = 0; i < type->modifiers.len; i++) {
		if (type->modifiers.arr[i].tag == TYPE_PTR) {
			addchar(cout, '*');

			if (type->modifiers.arr[i].is_const)
				concat_cstr(cout, "const ");
		}

		// TODO: array types
	}

	if (name != NULL) concat_cstr(cout, name);
	if (type->params.len > 0) addchar(cout, ')');

	// if type is function pointer
	if (type->params.len > 0) {
		addchar(cout, '(');

		for (int i = 0; i < type->params.len; i++) {
			generate_type(cg, &type->params.arr[i], NULL);
			if (i != type->params.len - 1) addchar(cout, ',');
		}

		addchar(cout, ')');
	}

	// consume
	freestr(&type->typename);
	free(type->modifiers.arr);
	free(type->params.arr);

	type->modifiers.len = 0;
	type->modifiers.cap = 0;
	type->params.len    = 0;
	type->is_const      = false;
	type->is_volatile   = false;
}

static void generate_identifier(CodeGen *cg, Identifier *ident, bool isfn) {
	// aliases
	string_t *const cout = &cg->c_output;

	if (ident->is_static)  concat_cstr(cout, "static ");
	if (ident->is_extern)  concat_cstr(cout, "extern ");

	generate_type(cg, &ident->type, ident->name.str);

	// consume
	freestr(&ident->name);
	*ident = (Identifier) {0};
}

static void generate_scope(CodeGen *cg, Scope *scope) {
	// aliases
	string_t *const cout = &cg->c_output;

	concat_cstr(cout, " {\n");
	cg->indent_level++;
	indent(cg);

	for_range (i, scope->ident_len) {
		generate_identifier(cg, &scope->identifiers[i], false);
		generate_statement(cg);
	}

	// TODO: proper error handling
	while (cg->ast_stack[cg->traverse].type != TAR_SCOPE_END) {
		generate(cg, consume(cg));

		if (cg->traverse >= cg->stack_top) {
			PANIC("generated scope does not have an end");
		}
	}

	// consume scope end
	consume(cg);

	cg->indent_level--;

	indent(cg);
	concat_cstr(cout, "}\n");

	// consume scope
	free(scope->identifiers);
	scope->ident_len = 0;
	scope->ident_cap = 0;
	scope->parent    = -1;
}

static void generate_function(CodeGen *cg, Function *fn) {
	smart_string function = init_str(fn->name.str);
	string_t     cout     = cg->c_output;

	// HACK: switch the buffers here for type generation
	cg->c_output = function;

	addchar(&function, '(');
	if (fn->param_len == 0) {
		concat_cstr(&function, "void");
	}
	else for (int i = 0; i < fn->param_len; i++) {
		generate_identifier(cg, &fn->parameters[i], true);

		if (i < fn->param_len - 1)
			addchar(&function, ',');
	}
	addchar(&function, ')');

	cg->c_output = cout;
	generate_type(cg, &fn->return_type, function.str);

	// consume
	freestr(&fn->name);
	free(fn->parameters);
	*fn = (Function) {0};
}

static void generate_call(CodeGen *cg, FunctionCall *call) {
	concat(&cg->c_output, &call->name);
	addchar(&cg->c_output, '(');

	for_range (i, call->params) {
		generate(cg, consume(cg));
		
		if (i != call->params - 1)
			concat_cstr(&cg->c_output, ", ");
	}

	addchar(&cg->c_output, ')');
	freestr(&call->name);
}

static void generate_expression(CodeGen *cg, CExpression expr) {
	const size_t LEN = strlen(expr);

	for_range (i, LEN) {
		const char ch = expr[i];

		if (ch == '$')
			generate(cg, consume(cg));
		else
			addchar(&cg->c_output, ch);
	}
}

static void generate(CodeGen *cg, TargetAST *node) {
	switch (node->type) {
		case TAR_FUNCTION:
			generate_function(cg, &node->func);
			break;

		case TAR_CALL:
			generate_call(cg, &node->call);
			break;

		case TAR_SCOPE:
			generate_scope(cg, &node->scope);
			break;

		case TAR_SCOPE_END:
			PANIC("TAR_SCOPE_END should be handled in [generate_scope]");
			break;

		case TAR_STATEMENT:
			generate_statement(cg);
			break;

		case TAR_IDENTIFIER:
			generate_identifier(cg, &node->ident, false);
			break;

		case TAR_EXPR:
			generate_expression(cg, node->expr);
			break;
	}
}

static void debug_print(CodeGen *cg) {
	for_range(i, cg->stack_top) {
		TargetAST *const node = &cg->ast_stack[i];

		switch (node->type) {
			case TAR_FUNCTION:
				logger("[FUNC]: %s", node->func.name.str);
				break;
			case TAR_CALL:
				logger("[CALL]: %s", node->call.name.str);
				break;
			case TAR_SCOPE:
				logger("[SCOPE]");
				break;
			case TAR_SCOPE_END:
				logger("[END]");
				break;
			case TAR_STATEMENT:
				logger("[STATEMENT]");
				break;
			case TAR_IDENTIFIER:
				logger("[IDENT]: %s", node->ident.name.str);
				break;
			case TAR_EXPR:
				logger("[EXPR]: %s", node->expr);
				break;
		}
	}
}

string_t get_generated(CodeGen *cg) {
	if (cg->stack_top > 0) {
		cg->traverse     = 0;
		cg->indent_level = 0;

		if (DEBUG_CODEGEN) {
			debug_print(cg);
		}

		// NOTE: is this needed? the global scope should take care of things
		while (cg->traverse < cg->stack_top) {
			generate(cg, consume(cg));
		}
	}

	free(cg->ast_stack);

	// NOTE: This could be done with a clear -> do I want to?
	cg->ast_stack    = NULL;
	cg->cap          = 0;
	cg->stack_top    = 0;
	cg->active_scope = -1;

	string_t compiled     = cg->c_output;
	cg->c_output.str      = NULL;
	cg->c_output.capacity = 0;
	cg->c_output.size     = 0;

	return compiled;
}

static void grow_stack(CodeGen *cg) {
	if (cg->stack_top >= cg->cap) {
		cg->cap *= 2;
		let tmp = realloc(cg->ast_stack, cg->cap);
		if (tmp == NULL) {
			free(tmp);
			PANIC("could not grow ast stack");
		}

		cg->ast_stack = tmp;
	}
}

void emit_scope(CodeGen *cg) {
	grow_stack(cg);

	cg->ast_stack[cg->stack_top++] = (TargetAST) {
		.type  = TAR_SCOPE,
		.scope = {
			.parent = cg->active_scope,

			.identifiers = malloc(sizeof(Identifier) * 4),
			.ident_len   = 0,
			.ident_cap   = 4,
		}
	};

	cg->active_scope = cg->stack_top - 1;
}

void emit_scope_end(CodeGen *cg) {
	grow_stack(cg);
	cg->ast_stack[cg->stack_top++] = (TargetAST) { .type = TAR_SCOPE_END };

	cg->active_scope = cg->ast_stack[cg->active_scope].scope.parent;
}

void emit_statement(CodeGen *cg) {
	grow_stack(cg);
	cg->ast_stack[cg->stack_top++] = (TargetAST) { .type = TAR_STATEMENT };
}

// consumes type
void emit_identifier(
		CodeGen *cg,
		bool     is_static,
		bool     is_extern,
		CType    type,
		cstr     name)
{
	if (cg->active_scope < 0 ||
			cg->ast_stack[cg->active_scope].type != TAR_SCOPE ||
			check_reserved(name))
	{
		PANIC("could not find active scope");
	}

	let scope = &cg->ast_stack[cg->active_scope].scope;

	if (scope->ident_len >= scope->ident_cap) {
		scope->ident_cap *= 2;
		let tmp = realloc(scope->identifiers, scope->ident_cap);
		if (tmp == NULL) {
			free(tmp);
			PANIC("cannot expand identifier vector");
		}

		scope->identifiers = tmp;
	}

	scope->identifiers[scope->ident_len++] =
		make_identifier(name, &type, is_static, is_extern);
}

void emit_function(
		CodeGen   *cg,
		bool       is_static,
		CType     *type,
		cstr       name,
		size_t     count,
		Identifier params[count])
{
	grow_stack(cg);

	cg->ast_stack[cg->stack_top++] = (TargetAST) {
		.type = TAR_FUNCTION,
		.func = {
			.name        = init_str(name),
			.return_type = *type,
			.parameters  = malloc(sizeof(Identifier) * count),
			.param_len   = count,
			.is_static   = is_static,
		},
	};

	Function *const func = &cg->ast_stack[cg->stack_top - 1].func;

	memmove(func->parameters, params, sizeof(Identifier) * count);
	*type = (CType) {0};
}

void emit_call(CodeGen *cg, const char *name, u32 params) {
	grow_stack(cg);

	cg->ast_stack[cg->stack_top++] = (TargetAST) {
		.type = TAR_CALL,
		.call = {
			.name   = init_str(name),
			.params = params,
		},
	};
}

void emit_expression(CodeGen *cg, CExpression expr) {
	grow_stack(cg);

	cg->ast_stack[cg->stack_top++] = (TargetAST) {
		.type = TAR_EXPR,
		.expr = expr,
	};
}
