#pragma once

#define INIT_HANDLE_TYPE(type) typedef struct { unsigned int h; } type

typedef struct FreeList_t {
	void				*block;
	struct FreeList_t	*next;
} FreeList;
