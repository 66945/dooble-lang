#pragma once

// mandatory includes
#include "../testing/testing.h"
#include "err.h"
#include <stdint.h>

#define let         __auto_type
#define startup     [[gnu::constructor]]
#define fallthrough [[fallthrough]]
#define typeof(t)   __typeof__(t)
#define typeofp(p)  __typeof__(*p)

#define MACRO_WRAP(wrap) do { wrap } while(0)

#define loop while (1)
#define for_range(i, num) for (int i = 0; i < num; i++)
#define for_char(str)     for (char *c = str; c != nullptr; *i++)
#define new(T)            (T *) calloc(sizeof(T), 1)
#define make(T, N)        (T *) calloc(sizeof(T), N)

#define VEC(T)     struct { T *arr; size_t len; size_t cap; }
#define RANGE(T)   struct { T *arr; size_t len; }
#define PAIR(A, B) struct { A a; B b; }
#define EXTEND_ARR(type, arr, len, cap)                   \
	do {                                                  \
		if (len > cap) {                                  \
			cap *= 2;                                     \
			void *tmp = realloc(arr, sizeof(type) * cap); \
			if (tmp == NULL) {                            \
				free(arr);                                \
				PANIC("cannot extend dynamic array");     \
			}                                             \
			arr = tmp;                                    \
		}                                                 \
	} while (0)

#define LEN(arr) sizeof(arr) / sizeof(typeof(*arr))
#define ABSF(f) (f >= 0 ? f : -f)

typedef int8_t   i8;
typedef uint8_t  u8;
typedef int16_t  i16;
typedef uint16_t u16;
typedef int32_t  i32;
typedef uint32_t u32;
typedef int64_t  i64;
typedef uint64_t u64;

typedef const char cstr[];
