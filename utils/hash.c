#include "hash.h"
#include "../testing/testing.h"
#include "utils.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned long long hash_str(const char *key, size_t len) {
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
		set.items[i].key[0]  = 0;
		set.items[i].full    = false;
		set.items[i].ordered = -1;
	}

	return set;
}

void free_hashset(HashSet *set) {
	free(set->items);
	set->cap = 0;
	set->len = 0;
}

bool add_item(HashSet *set, const char *key) {
	size_t len = strlen(key);
	if (len > KEY_SIZE - 1) return false;
	uint16_t slot = (hash_str(key, len) % set->cap);

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
	size_t len = strlen(key);
	if (len > KEY_SIZE - 1) return false;
	int slot = (hash_str(key, len) % set->cap);

	while (slot < set->cap) {
		if (set->items[slot].full && strcmp(set->items[slot].key, key) == 0) {
			return true;
		}

		slot++;
	}

	return false;
}

int get_item(HashSet *set, const char *key) {
	size_t len = strlen(key);
	if (len > KEY_SIZE - 1) return -1;
	int slot = (hash_str(key, len) % set->cap);

	while (slot < set->cap) {
		if (set->items[slot].full && strcmp(set->items[slot].key, key) == 0) {
			return set->items[slot].ordered;
		}

		slot++;
	}

	return -1;
}

bool delete_item(HashSet *set, const char *key) {
	size_t len = strlen(key);
	if (len > KEY_SIZE - 1) return false;
	int slot = (hash_str(key, len) % set->cap);

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
			.arr = make(HashKey, HASH_INIT_SIZE),
			.len = 0,
			.cap = HASH_INIT_SIZE,
		},
	};

	memset(map.table.arr, 0, sizeof(HashKey) * HASH_INIT_SIZE);
	return map;
}

static void adjust_capacity(HashMap *map) {
	typeof(map->table) new_table = {
		.arr = make(HashKey, map->table.cap * 2),
		.len = map->table.len,
		.cap = map->table.cap * 2,
	};

	memset(new_table.arr, 0, sizeof(HashKey) * new_table.cap);

	for_range (i, map->table.cap) {
		if (map->table.arr[i].key == 0) continue;

		// should both be constant
		size_t  HASH    = map->table.arr[i].key;
		void   *CONTENT = map->table.arr[i].content;

		new_table.arr[HASH % new_table.cap].key     = HASH;
		new_table.arr[HASH % new_table.cap].content = CONTENT;
	}

	free(map->table.arr);
	map->table = new_table;
}

bool set_pair(HashMap *map, void *key, void *val) {
	const size_t  HASH      = map->hashfn(key);
	size_t        address   = HASH % map->table.cap;
	HashKey      *hash_item = &map->table.arr[address];

	float load = (float) map->table.len / (float) map->table.cap;
	if (load > HASH_MAX_LOAD) adjust_capacity(map);

	if (hash_item->content == NULL) {
		hash_item->key     = HASH;
		hash_item->content = malloc(map->val_size);
		memmove(hash_item->content, val, map->val_size);

		return true;
	}

	// open addressing style
	loop {
		address = (address + 1) % map->table.cap;
		if (map->table.arr[address].content == NULL) {
			hash_item          = &map->table.arr[address];
			hash_item->key     = HASH;
			hash_item->content = malloc(map->val_size);
			memmove(hash_item->content, val, map->val_size);

			return true;
		}
	}
}

// returns a reference to the hashmap item with a lifetime of the key
void *get_pair(HashMap *map, void *key) {
	return get_pairh(map, map->hashfn(key));
}

void *get_pairh(HashMap *map, const size_t hash) {
	size_t address = hash % map->table.cap;

	loop {
		if (map->table.arr[address].key == hash) {
			return map->table.arr[address].content;
		} else if (map->table.arr[address].content == NULL) {
			return NULL;
		}

		address = (address + 1) % map->table.cap;
	}
}

bool delete_pair(HashMap *map, void *key) {
	const size_t HASH    = map->hashfn(key);
	size_t       address = HASH % map->table.cap;

	loop {
		if (map->table.arr[address].key == HASH) {
			HashKey *hash_item = &map->table.arr[address];
			free(hash_item->content);
			hash_item->key     = 0;
			hash_item->content = NULL;

			return true;
		} else if (map->table.arr[address].content == NULL) {
			return false;
		}

		address = (address + 1) % map->table.cap;
	}
}

void *hash_iter(HashMap *map, bool start) {
	static size_t item_count = 0;
	if (start)    item_count = 0;

	while (item_count < map->table.cap) {
		if (map->table.arr[item_count++].key != 0) {
			return map->table.arr[item_count - 1].content;
		}
	}

	return nullptr;
}

bool free_map(HashMap *map) {
	for_range (i, map->table.cap) {
		if (map->table.arr[i].content == NULL) continue;
		free(map->table.arr[i].content);
	}

	free(map->table.arr);
	return true;
}
