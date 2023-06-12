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

typedef size_t (*hash_t)   (void *);
typedef void   (*delete_t) (void *);

typedef struct {
	size_t   val_size;
	hash_t   hashfn;
	delete_t delete;

	VEC(HashKey) table;
} HashMap;

#define BUILD_MAP(type, hashfn, delete) build_map(sizeof(type), (hash_t) hashfn, (delete_t) delete)
#define GET_PAIR(type, map, key)    ((type *) get_pair(map, key))
#define GET_PAIRH(type, map, key)   ((type *) get_pairh(map, key))
#define HASH_ITER(type, map, start) ((type *) hash_iter(map, start))

#define HASH_MAX_LOAD 0.75

// NULL hashfn and delete result in ptr hash and basic free
HashMap  build_map(size_t key_size, hash_t hashfn, delete_t delete);
bool     set_pair(HashMap *map, void *key, void *val);
void    *get_pair(HashMap *map, void *key);
void    *get_pairh(HashMap *map, size_t hash);
bool     delete_pair(HashMap *map, void *key);
bool     free_map(HashMap *map);

void *hash_iter(HashMap *map, bool start);

// hash functions
unsigned long long hash_str(const char *key, size_t len);
