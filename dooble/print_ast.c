#include "internal.h"
#include <stdio.h>

static int indent_level = 0;

static void indent(void) {
	for_range (i, indent_level) {
		putchar('\t');
	}
}

static void print_if(IfStmt *i) {
	printf("(if\n");
	indent_level++;

	print_ast(i->condition);
	print_ast(i->stmt);

	if (i->else_case != NULL) {
		indent();
		printf("else: \n");
		print_ast(i->else_case);
	}

	indent_level--;
	indent();
	printf(")\n");
}

static void print_foreach(ForEach *f, u8 type) {
	printf("(%s %s%s in\n",
			type == EX_FOREACH ? "for" :
			type == EX_DOEACH  ? "do"  :
			"don't",

			f->by_reference ? "&" : "",
			f->ident.str);

	indent_level++;

	print_ast(f->range);
	print_ast(f->stmt);

	indent_level--;
	indent();
	printf(")\n");
}

static void print_forwhile(ForWhile *f, u8 type) {
	printf("(%s while\n",
			type == EX_FOREACH ? "for" :
			type == EX_DOEACH  ? "do"  :
			"don't");


	indent_level++;

	print_ast(f->condition);
	print_ast(f->stmt);

	indent_level--;
	indent();
	printf(")\n");
}

static void print_block(Block *b) {
	printf("({}\n");
	indent_level++;

	for_range (i, b->len) {
		print_ast(b->arr[i]);
	}

	indent_level--;
	indent();
	printf(")\n");
}

// hello.hi.call().test()().hi -- valid

// hi, bye := 'hello world', 0

// PI         :: 3.141592653 -- pass 0
// TypeAlias  :: [3]Hello    -- pass 0
// TypeAlias3 :: TypeAlias2  -- pass 1
// TypeAlias2 :: TypeAlias   -- pass 2

static void print_decl(Declaration *d) {
	printf("(%s %s\n", d->is_const ? "::" : ":=", d->name.str);
	indent_level++;

	if (d->quals.is_static)  { indent(); printf("static\n");  }
	if (d->quals.is_pub)     { indent(); printf("pub\n");     }
	if (d->quals.is_co)      { indent(); printf("co\n");      }
	if (d->quals.is_protect) { indent(); printf("protect\n"); }
	if (d->quals.is_final)   { indent(); printf("final\n");   }

	indent();
	printf("type: %p\n", d->type);

	print_ast(d->assign);

	indent_level--;
	indent();
	printf(")\n");
}

static void print_binop(BinOp *b) {
	const char *op = 
		b->operator == DB_STAR      ? "*"      :
		b->operator == DB_SLASH     ? "/"      :
		b->operator == DB_PLUS      ? "+"      :
		b->operator == DB_MINUS     ? "-"      :
		b->operator == DB_AMPER     ? "&"      :
		b->operator == DB_BITOR     ? "|"      :
		b->operator == DB_LESS      ? "<"      :
		b->operator == DB_LESSEQ    ? "<="     :
		b->operator == DB_GREATER   ? ">"      :
		b->operator == DB_GREATEREQ ? ">="     :
		b->operator == DB_IS        ? "is"     :
		b->operator == DB_NOT       ? "is not" :
		b->operator == DB_AND       ? "and"    :
		b->operator == DB_OR        ? "or"     :
		b->operator == DB_DOTDOT    ? ".."     :
		b->operator == DB_DOT       ? "."      :
		"invalid";

	printf("(binop: %s\n", op);
	indent_level++;
	print_ast(b->expra);
	print_ast(b->exprb);
	indent_level--;
	indent();
	printf(")\n");
}

static void print_unary(Unary *u) {
	const char *op =
		u->operator == DB_MINUS ? "-"   :
		u->operator == DB_NOT   ? "not" :
		u->operator == DB_STAR  ? "*"   :
		u->operator == DB_AMPER ? "&"   :
		"invalid";

	printf("(unary: %s\n", op);
	indent_level++;
	print_ast(u->expr);
	indent_level--;
	indent();
	printf(")\n");
}

static void print_call(Call *c) {
	printf("(call()\n");
	indent_level++;
	print_ast(c->caller);
	
	indent();
	printf("args:\n");

	for_range (i, c->len) {
		print_ast(c->params[i]);
	}

	indent_level--;
	indent();
	printf(")\n");
}

static void print_submember(SubMember *s) {
	printf("(.%s\n", s->name.str);

	indent_level++;
	print_ast(s->expr);
	indent_level--;
	indent();
	printf(")\n");
}

static void print_function(Function *f) {
	printf("(fn() -> 0x%p\n", f->ret_type);

	indent_level++;
	for_range (i, f->args.len) {
		print_ast(f->args.arr[i]);
	}

	indent();
	printf("body:\n");

	print_ast(f->block);

	indent_level--;
	indent();
	printf(")\n");
}

static void print_literal(Literal *l) {
	switch (l->tag) {
		case LIT_NUM:
			printf("%jd", l->numi);
			break;
		case LIT_FLT:
			printf("%f", l->numf);
			break;
		case LIT_BOOL:
			printf(l->boolean ? "true" : "false");
			break;
		case LIT_STR:
			printf("'%s'", l->str.str);
			break;
		case LIT_IDENT:
			printf("%s", l->str.str);
			break;
		case LIT_NIL:
			printf("nil");
			break;
	}

	putchar('\n');
}

void print_ast(Node *node) {
	indent();

	switch (node->tag) {
		case EX_PASS:
			printf("...");
			break;
		case EX_IF:
			print_if(&node->ifstmt);
			break;
		case EX_FOREACH:
		case EX_DOEACH:
		case EX_DONTEACH:
			print_foreach(&node->foreach, node->tag);
			break;
		case EX_FORWHILE:
		case EX_DOWHILE:
		case EX_DONTWHILE:
			print_forwhile(&node->forwhile, node->tag);
			break;
		case EX_DECL:
			print_decl(&node->declare);
			break;
		case EX_BLOCK:
			print_block(&node->block);
			break;
		case EX_BINOP:
			print_binop(&node->binop);
			break;
		case EX_UNARY:
			print_unary(&node->unary);
			break;
		case EX_CALL:
			print_call(&node->call);
			break;
		case EX_SUBMEMBER:
			print_submember(&node->member);
			break;
		case EX_FUNCTION:
			print_function(&node->function);
			break;
		case EX_LITERAL:
			print_literal(&node->literal);
			break;
	}
}

// ====================================================
// ================= Type Printing ====================
// ====================================================

void print_typetree(TypeTree *tree) {
	// ...
}
