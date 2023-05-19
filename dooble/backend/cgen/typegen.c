#include "internal.h"
#include <stdio.h>

static AnonStruct *create_anon_struct(GenCompiler *comp) {
	EXTEND_ARR(AnonStruct,
			comp->anon_structs.arr,
			comp->anon_structs.len,
			comp->anon_structs.cap);

	comp->anon_structs.arr[comp->anon_structs.len++] = (AnonStruct) {
		.arr = malloc(sizeof(TypePair) * 5),
		.len = 0,
		.cap = 5,
	};

	return &comp->anon_structs.arr[comp->anon_structs.len - 1];
}

static void add_anon_member(AnonStruct *a, Member *member, GenCompiler *comp) {
	EXTEND_ARR(TypePair, a->arr, a->len, a->cap);

	a->arr[a->len++] = (TypePair) {
		.a = copy_str(&member->name),
		.b = build_type(comp, member->type),
	};
}

static void add_anon_cmember(AnonStruct *a, CType *type, cstr name) {
	EXTEND_ARR(TypePair, a->arr, a->len, a->cap);

	a->arr[a->len++] = (TypePair) {
		.a = init_str(name),
		.b = *type,
	};
}

#define NAME_ANON_CTYPE(type)                                          \
	do {                                                               \
		char namebuf[10];                                              \
		snprintf(namebuf, 10, "anon%llu", comp->anon_structs.len - 1); \
		add_name(&type, namebuf);                                      \
	} while (0)

/* Type building in the 21st century:
 * Unfortunately I cannot do a 1 : 1 translation between dooble types and C types.
 * Vector / Map types need to refer to their anonymouse structs.
 * Unions and Structs need to generate anonymous structs and return the appropriate
 * typename. Then, at the beginning of code generation I need to build all the anon
 * types.
 *
 * I also do not want to build
 * */
CType build_type(GenCompiler *comp, typeid id) {
	if (id == NULL) {
		// NOTE: this just passes the buck of error handling up to the parser
		// and AST passes.
		PANIC("non existant type passed to build_type");
	}

	const TypeLeaf *type  = (TypeLeaf *) id;
	CType           ctype = make_ctype();

	do {
		switch (type->tag) { // *[30]
			case DBLTP_PTR:
				add_ptr(&ctype, false);
				break;
			case DBLTP_ARR:
				add_arr(&ctype, type->size);
				break;
			case DBLTP_FN:
				{
					CType params[type->fn.len];

					for_range (i, type->fn.len) {
						params[i] = build_type(comp, type->fn.arr[i]);
					}

					make_fn_ptr(&ctype, type->fn.len, params);
					break;
				}
			case DBLTP_NAME:
				add_name(&ctype, type->name.str);
				break;

			case DBLTP_UNION:
				fallthrough;
			case DBLTP_STRUCT: // bada bing bada boom data structures go zoom
				{
					AnonStruct *ztruct = create_anon_struct(comp);
					for_range (i, type->members.len) {
						add_anon_member(ztruct, &type->members.arr[i], comp);
					}

					ztruct->is_union = type->tag == DBLTP_UNION;
					NAME_ANON_CTYPE(ctype);
				}
				break;

			case DBLTP_ERR:
				fallthrough;
			case DBLTP_OPT:
				{
					CType cbool = make_ctype();
					add_name(&cbool, "bool");

					CType copt = ctype;
					add_ptr(&copt, false);

					AnonStruct *opt = create_anon_struct(comp);
					add_anon_cmember(opt, &cbool, "is_valid");
					add_anon_cmember(opt, &copt, "opt");

					ctype = make_ctype(); // make new type that wraps a ptr for the
										  // original
					NAME_ANON_CTYPE(ctype);
				}
				break;

			case DBLTP_SLICE:
				{
					CType clen = make_ctype();
					add_name(&clen, "size_t");

					CType carr = ctype;
					add_ptr(&carr, false);

					AnonStruct *slice = create_anon_struct(comp);
					add_anon_cmember(slice, &carr, "arr");
					add_anon_cmember(slice, &clen, "len");

					ctype = make_ctype();
					NAME_ANON_CTYPE(ctype);
				}
				break;

			case DBLTP_VEC:
				{
					CType ccap = make_ctype();
					CType clen = make_ctype();
					add_name(&ccap, "size_t");
					add_name(&clen, "size_t");

					CType carr = ctype;
					add_ptr(&carr, false);

					AnonStruct *vec = create_anon_struct(comp);
					add_anon_cmember(vec, &carr, "arr");
					add_anon_cmember(vec, &ccap, "cap");
					add_anon_cmember(vec, &clen, "len");

					ctype = make_ctype();
					NAME_ANON_CTYPE(ctype);
				}
				break;

			case DBLTP_MAP:
				PANIC("map types are not implemented");
				break;
		}

		type = type->parent;
	} while (type->parent != NULL);

	return ctype;
}
