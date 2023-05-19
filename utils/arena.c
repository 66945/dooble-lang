#include "arena.h"
#include "err.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>

bool init_pool(Pool *p, size_t size, size_t count, bool resize) {
	*p = (Pool) {
		.pool   = calloc(size, count),
		.size   = size > sizeof(void *) ? size : sizeof(void *),
		.len    = count,
		.resize = resize,
	};

	if (p->pool == NULL) {
		error("could not allocate memory for pool");
		return false;
	}

	for_range (i, count - 1) {
		void     *ptr      = &p->pool[i       * size];
		void     *next     = &p->pool[(i + 1) * size];
		PoolFree *node     = ptr;
		PoolFree *nextnode = next;

		node->next = nextnode;
	}

	void     *end     = &p->pool[(count - 1) * size];
	PoolFree *endnode = end;

	endnode->next = NULL;
	return true;
}

void *palloc(Pool *pool) {
	PoolFree *free_node = pool->start;

	// when pool is at max memory
	if (free_node == NULL) {
		if (!pool->resize) {
			error("pool has reached maximum allocation limit");
			return NULL;
		}

		pool->len *= 2;
		let tmp = realloc(pool->pool, pool->len);
		if (tmp == NULL) {
			error("failed to grow pool arena");
			pdump(pool);
			return NULL;
		}
		pool->pool = tmp;

		void *next = &pool->pool[(pool->len - 1) * pool->size];
		free_node->next = next;
	}

	pool->start = free_node->next;

	return memset(free_node, 0, pool->size);
}

void pfree(Pool *pool, void *ptr) {
	if (ptr == NULL) return;

	// WARN: does not check to make sure freed ptr exists within the pool

	PoolFree *node = ptr;
	node->next  = pool->start;
	pool->start = node->next;
}

void pdump(Pool *pool) {
	pool->start = NULL;
	pool->len   = 0;
	pool->size  = 0;
	free(pool->pool);
	pool->pool = NULL;
}
