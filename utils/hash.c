#include "hash.h"
#include "../testing/testing.h"
#include "utils.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned long long hash_str(const char *key) {
	int len = strlen(key);
	if (len >= KEY_SIZE) return -1;

	const int p = 53;
	const int m = 1e9 + 9;

	unsigned long long hash = 0;
	unsigned long long p_power = 1;
	for (int i = 0; i < len; i++) {
		hash = (hash + (key[i] - 'a' + 1) * p_power) % m;
		p_power = (p_power * p) % m;
	}

	return hash;
}

HashSet init_hashset(void) {
	HashSet set = {
		.cap	= HASH_INIT_SIZE,
		.len	= 0,
		.items	= malloc(sizeof(HashItem) * HASH_INIT_SIZE),
	};

	for (int i = 0; i < HASH_INIT_SIZE; i++) {
		set.items[i].key[0]		= '\0';
		set.items[i].full		= false;
		set.items[i].ordered	= -1;
	}

	return set;
}

void free_hashset(HashSet *set) {
	free(set->items);
	set->cap = 0;
	set->len = 0;
}

bool add_item(HashSet *set, const char *key) {
	uint16_t slot = (hash_str(key) % set->cap);

	while (slot < set->cap) {
		if (!set->items[slot].full) {
			set->items[slot].full    = true;
			set->items[slot].ordered = set->len++;

			strcpy_s(set->items[slot].key, KEY_SIZE, key);
			return true;
		} else if (strcmp(set->items[slot].key, key) == 0) {
			return true;
		}

		slot++;
	}

	set->cap *= 2;
	set->items = realloc(set->items, sizeof(HashItem) * set->cap);
	if (set->items == NULL) return false;

	set->items[slot].full    = true;
	set->items[slot].ordered = set->len++;

	strcpy_s(set->items[slot].key, KEY_SIZE, key);
	return true;
}

bool has_item(HashSet *set, const char *key) {
	int slot = (hash_str(key) % set->cap);

	while (slot < set->cap) {
		if (set->items[slot].full && strcmp(set->items[slot].key, key) == 0) {
			return true;
		}

		slot++;
	}

	return false;
}

int get_item(HashSet *set, const char *key) {
	int slot = (hash_str(key) % set->cap);

	while (slot < set->cap) {
		if (set->items[slot].full && strcmp(set->items[slot].key, key) == 0) {
			return set->items[slot].ordered;
		}

		slot++;
	}

	return -1;
}

bool delete_item(HashSet *set, const char *key) {
	int slot = (hash_str(key) % set->cap);

	while (slot < set->cap) {
		if (set->items[slot].full && strcmp(set->items[slot].key, key) == 0) {
			const int order = set->items[slot].ordered;

			set->items[slot].full    = false;
			set->items[slot].ordered = -1;

			// reorder elements
			for (int i = 0; i < set->cap; i++) {
				if (set->items[i].full && set->items[i].ordered >= order) {
					set->items[i].ordered--;
				}
			}

			set->len--;
			return true;
		}

		slot++;
	}

	return false;
}

// Custom HashMap implementation (can afford to be slow at times. not the final dooble)
static size_t hash_ptr(void *p) {
	uintptr_t ptr = (uintptr_t) p;
	ptr = ((ptr >> 16) ^ ptr) * 0x45d9f3b;
	ptr = ((ptr >> 16) ^ ptr) * 0x45d9f3b;
	ptr = ((ptr >> 16) ^ ptr);
	return ptr;
}

HashMap build_map(size_t key_size, hash_t hashfn, delete_t delete) {
	HashMap map = {
		.val_size = key_size,
		.hashfn   = hashfn != NULL ? hashfn : hash_ptr,
		.delete   = delete,
		.table    = {
			.arr = make(HashKeyPool, HASH_INIT_SIZE),
			.len = 0,
			.cap = HASH_INIT_SIZE,
		},
	};

	memset(map.table.arr, 0, sizeof(HashKeyPool) * HASH_INIT_SIZE);
	return map;
}

bool set_pair(HashMap *map, void *key, void *val) {
	const size_t  HASH = map->hashfn(key);
	HashKeyPool  *pool = &map->table.arr[HASH % map->table.cap];

	if (pool->cap == 0) {
		pool->cap = 2;
		pool->arr = make(HashKey, pool->cap);
		pool->len = 0;
	}

	// strictly speaking this else should do nothing as a for loop will run over
	// a range of 0 .. 0. Also the fact that I have to write this comment is
	// indicative of bad code.
	else {
		for_range (i, pool->len) {
			HashKey *const current = &pool->arr[i];
			if (current->key != HASH) continue;

			if(map->delete != NULL) {
				map->delete(current->content);
			}

			free(current->content);
			current->content = malloc(map->val_size);
			memmove(current->content, val, map->val_size);
			return true;
		}
	}

	EXTEND_ARR(HashKey, pool->arr, pool->len, pool->cap);
	pool->arr[pool->len++] = (HashKey) {
		.key     = HASH,
		// WARN: for a large hashmap this could cause some problems with memory
		// fragmentation. a linear allocator might work, but some other pool
		// would probably do the job better
		.content = malloc(map->val_size),
	};

	void *const content = pool->arr[pool->len - 1].content;

	if (content == NULL) {
		warn("could not make allocation for hash item: %s:%d", __FILE__, __LINE__);
		return false;
	}

	memmove(content, val, map->val_size);

	map->table.len++;
	return true;
}

// returns a reference to the hashmap item with a lifetime of the key
void *get_pair(HashMap *map, void *key) {
	const size_t  HASH = map->hashfn(key);
	HashKeyPool  *pool = &map->table.arr[HASH % map->table.cap];

	for_range (i, pool->len) {
		if (pool->arr[i].key == HASH) {
			return pool->arr[i].content;
		}
	}

	return NULL;
}

bool delete_pair(HashMap *map, void *key) {
	const size_t HASH = map->hashfn(key);

	HashKeyPool *pool = &map->table.arr[HASH % map->table.cap];
	for_range (i, pool->len) {
		HashKey *const current = &pool->arr[i];
		if (current->content == NULL) return false;

		if (current->key == HASH) {
			if (map->delete != NULL) {
				map->delete(current->content);
			}

			free(current->content);
			if (i < pool->len - 1) {
				memmove(current, &pool->arr[i + 1], sizeof(HashKey) * (pool->len - i));
			}

			pool->len--;
			return true;
		}
	}

	return false;
}

bool free_map(HashMap *map) {
	for_range (i, map->table.cap) {
		HashKeyPool *const pool = &map->table.arr[i];
		if (pool->cap == 0) continue;

		for_range (j, pool->len) {
			if (map->delete != NULL) {
				map->delete(pool->arr[j].content);
			}

			free(pool->arr[j].content);
			pool->arr[j].content = NULL;
			pool->arr[j].key     = 0;
		}

		free(pool->arr);
		pool->arr = NULL;
		pool->cap = 0;
		pool->len = 0;
	}

	free(map->table.arr);
	return true;
}
