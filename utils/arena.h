#pragma once

#include <stdbool.h>

typedef struct PoolFree_t {
	struct PoolFree_t *next;
} PoolFree;

typedef struct {
	unsigned char *pool;
	PoolFree      *start;
	size_t         len;
	size_t         size;
	bool           resize;
} Pool;

bool  init_pool(Pool *p, size_t size, size_t count, bool resize);
void *palloc   (Pool *pool);
void  pfree    (Pool *pool, void *ptr);
void  pdump    (Pool *pool);
