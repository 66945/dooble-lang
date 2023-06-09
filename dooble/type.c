#include "type.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef size_t TypePath[10]; // With 10 it takes 80 bytes. Should this be a vector?

static bool leaf_eq(TypeLeaf *leafa, TypeLeaf *leafb) {
	if (leafa->tag != leafb->tag) return false;

	switch (leafa->tag) {
		case DBLTP_ARR:
			return leafa->size == leafb->size;
		case DBLTP_NAME:
			return strcmp(leafa->name.str, leafb->name.str) == 0;
		case DBLTP_MAP:
			return
				leafa->map.key == leafb->map.key &&
				leafa->map.val == leafb->map.val;
		case DBLTP_FN:
			if (leafa->fn.len != leafb->fn.len) {
				return false;
			}
			if (leafa->fn.ret != leafb->fn.ret) {
				return false;
			}

			for_range (i, leafa->fn.len) {
				if (leafa->fn.arr[i] != leafb->fn.arr[i])
					return false;
			}
			return true;

		case DBLTP_STRUCT:
			fallthrough;
		case DBLTP_UNION:
			return false;

		default:
			if (leafa->parent == NULL && leafb->parent == NULL) {
				return true;
			}
			else if (leafa->parent == NULL || leafb->parent == NULL) {
				return false;
			}

			return leaf_eq(leafa->parent, leafb->parent);
	}
}

typeid get_leaf(TypeTree *tree, TypeLeaf *base, TypeLeaf *leaf) {
	TypeBranch *branch = NULL;

	if (base == NULL) {
		branch = &tree->branches[0];
	}
	else if (base->next == NULL) {
		// creates a 'branch' and sets next to it.
		EXTEND_ARR(TypeBranch, tree->branches, tree->len, tree->cap);
		tree->branches[tree->len++] = (TypeBranch) {
			.arr = make(TypeLeaf, 3),
			.len = 0,
			.cap = 3,
		};

		base->next = &tree->branches[tree->len - 1];
		branch     = base->next;
	}

	for_range (i, branch->len) {
		if (leaf_eq(&branch->arr[i], leaf)) {
			return &branch->arr[i];
		}
	}

	EXTEND_ARR(TypeLeaf, branch->arr, branch->len, branch->cap);
	TypeLeaf *const new_leaf = &branch->arr[branch->len++];

	*new_leaf = *leaf;
	new_leaf->parent = base;
	new_leaf->next   = NULL;

	return new_leaf;
}

typeid basic_type(TypeTree *tree, PrimativeIndex index) {
	if (index < tree->branches->len) {
		return &tree->branches[0].arr[index];
	}

	return VOID_ID;
}

static inline void add_type(TypeTree *tree, cstr typename) {
	get_leaf(tree, NULL, &(TypeLeaf) {
		.tag  = DBLTP_NAME,
		.name = init_str(typename),
	});
}

TypeTree init_TypeTree(void) {
	TypeTree tree = {
		.branches = make(TypeBranch, 5),
		.len = 0,
		.cap = 5,

		.aliases = {
			.arr = make(TypeAlias, 5),
			.len = 0,
			.cap = 5,
		},
		// WARN: CTypes will free the memory they own when they get generated,
		// but the structs will still exist in limbo in the hashmap. As such, the
		// hashmap must be dumped immediately after the code generation process ends.
		// Maybe this could be achieved by separating they type module and the
		// generation code. This needs to be completely compatible with any other
		// backend I decide to write later on.

		// guarenteeing that is hard

		// i may have written some bad code

		// NOTE: I may have fixed this already
	};

	EXTEND_ARR(TypeBranch, tree.branches, tree.len, tree.cap);
	tree.branches[tree.len++] = (TypeBranch) {
		.arr = make(TypeLeaf, 5),
		.len = 0,
		.cap = 5,
	};

	add_type(&tree, "int");
	add_type(&tree, "float");
	add_type(&tree, "dooble");
	add_type(&tree, "bool");

	return tree;
}

// FIXME: add TypePath for deep copy of type references

/* bool copy_typetree(TypeTree *tree_a, TypeTree *tree_b) {
 * 	TypeBranch *const base_types = &tree_b->arr[0];
 * 
 * 	for_range (i, base_types->len) {
 * 		TypeLeaf *leaf  = &base_types->arr[i];
 * 		TypeLeaf *clone = get_leaf(tree_a, NULL, leaf); // build type
 * 
 * 		// NOTE: is clone != NULL necessary?
 *
 * 		while (leaf != NULL && clone != NULL) {
 * 			size_t clone_len = clone->next != NULL ? clone->next->len : 0;
 * 			size_t leaf_len  = leaf->next  != NULL ? leaf->next->len  : 0;
 * 
 * 			if (clone_len <= leaf_len) {
 * 				leaf = leaf->parent;
 * 				continue;
 * 			}
 * 
 * 			else {
 * 				get_leaf(tree_a, clone, &leaf->next->arr[clone->next->len++]);
 * 				leaf = &leaf->next->arr[clone->next->len - 1];
 * 			}
 * 		}
 * 	}
 * 
 * 	return true;
 * }
 * */

bool freetree(TypeTree *tree) {
	for_range (i, tree->len) {
		TypeBranch *const branch = &tree->branches[i];

		for_range (j, branch->len) {
			if (branch->arr[j].tag == DBLTP_FN) {
				if (branch->arr[j].fn.arr != NULL) {
					free(branch->arr[j].fn.arr);
				}

				branch->arr[j].fn.len = 0;
				branch->arr[j].fn.cap = 0;
			}

			else if (branch->arr[j].tag == DBLTP_STRUCT
					|| branch->arr[j].tag == DBLTP_UNION)
			{
				for_range (k, branch->arr[j].members.len) {
					freestr(&branch->arr[j].members.arr[k].name);
				}

				free(branch->arr[j].members.arr);
				branch->arr[j].members.len = 0;
				branch->arr[j].members.cap = 0;
				branch->arr[j].members.arr = NULL;
			}

			else if (branch->arr[j].tag == DBLTP_NAME) {
				freestr(&branch->arr[j].name);
			}
		}

		free(branch->arr);
		branch->len = 0;
		branch->cap = 0;
		branch->arr = NULL;
	}

	free(tree->branches);
	tree->len      = 0;
	tree->cap      = 0;
	tree->branches = NULL;

	// aliases
	free(tree->aliases.arr);
	tree->aliases.len = 0;
	tree->aliases.cap = 0;
	tree->aliases.arr = NULL;

	return false;
}
