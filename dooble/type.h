#pragma once

#include "../utils/utils.h"
#include "../utils/hash.h"
#include "../codegen/codegen.h"

// a very useful abstraction
typedef void *typeid;

typedef struct TypeBranch_t TypeBranch;

/* Process for generation:
 * ?[10]int
 *
 * CType 0 (empty)
 * CType 0 (name: int)
 * CType 0 (name: int, arr: 10)
 *
 * CType 1 (name: opt0X)
 *
 * {
 *     CType 2 (anon struct) {
 *         bool opt
 *         CType 1
 *     }
 * }
 * */
typedef struct { // NOTE: could use a hashmap
	typeid from;
	typeid to;
} TypeAlias;

typedef struct {
	string_t name;
	typeid   type;
} Member;

typedef struct TypeLeaf_t TypeLeaf;
struct TypeLeaf_t {
	enum : u8 {
		DBLTP_PTR,
		DBLTP_OPT,
		DBLTP_ERR,
		DBLTP_ARR,
		DBLTP_SLICE,
		DBLTP_VEC,
		DBLTP_MAP,
		DBLTP_FN,
		DBLTP_NAME,
		DBLTP_STRUCT,
		DBLTP_UNION,
	} tag;

	union {
		size_t   size;
		string_t name;

		struct {
			typeid key;
			typeid val;
		} map;

		struct {
			typeid  ret;
			typeid *arr; // arr
			u16     cap;
			u16     len;
		} fn;

		struct {
			Member *arr; // arr
			u16     cap;
			u16     len;
		} members;
	};

	TypeLeaf   *parent;
	TypeBranch *next;
};

struct TypeBranch_t {
	TypeLeaf *arr; // arr
	size_t    cap;
	size_t    len;
};

typedef struct {
	VEC(TypeBranch);
	VEC(TypeAlias) aliases; // NOTE: distinct?
} TypeTree;

typedef enum : u8 {
	INT_INDEX,
	FLOAT_INDEX,
	DOOBLE_INDEX,
	BOOL_INDEX
} PrimativeIndex;

TypeTree init_TypeTree(void);
typeid   get_leaf(TypeTree *tree, TypeLeaf *base, TypeLeaf *leaf);
void     print_typetree(TypeTree *tree);
bool     freetree(TypeTree *tree);
