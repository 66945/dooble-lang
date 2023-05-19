#pragma once

#include "../strutils/str.h"
#include <stdbool.h>
#include <stdint.h>

#define KEY_SIZE 40
typedef struct {
	char    key[KEY_SIZE]; // Don't want to use string_t for this operation
	bool    full;
	int32_t ordered;
} HashItem;

// WARN: this is a constant and not an array so I can do dynamic reallocation,
// but the implementation never actually does this. This is a big problem for
// anything that requires more than the initial size.
#define HASH_INIT_SIZE 256
typedef struct {
	uint32_t  cap;
	uint32_t  len;
	HashItem *items;
} HashSet;

HashSet	init_hashset(void);
void	free_hashset(HashSet *set);

bool add_item    (HashSet *set, const char *key);
bool has_item    (HashSet *set, const char *key);
int  get_item    (HashSet *set, const char *key);
bool delete_item (HashSet *set, const char *key);

/* === Generic Hash Map Implementation === */

typedef struct {
	size_t  key;
	void   *content;
} HashKey;

typedef VEC(HashKey) HashKeyPool;

typedef size_t (*hash_t)   (void *);
typedef void   (*delete_t) (void *);

typedef struct {
	size_t   val_size;
	hash_t   hashfn;
	delete_t delete;

	VEC(HashKeyPool) table;
} HashMap;

#define BUILD_MAP(type, hashfn, delete) build_map(sizeof(type), hashfn, delete)
#define GET_PAIR(type, map, key) ((type *) get_pair(map, key))

// NULL hashfn and delete result in ptr hash and basic free
HashMap  build_map(size_t key_size, hash_t hashfn, delete_t delete);
bool     set_pair(HashMap *map, void *key, void *val);
void    *get_pair(HashMap *map, void *key);
bool     delete_pair(HashMap *map, void *key);
bool     free_map(HashMap *map);
