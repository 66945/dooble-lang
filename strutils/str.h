#pragma once

#include "../utils/utils.h"
#include <stdbool.h>
#include <stdint.h>

// auto frees the string
#define smart_string __attribute__((cleanup(freestr))) string_t

#define SMALL_SIZE 16

typedef struct string {
	char     *str;
	uint32_t  size;
	uint32_t  capacity;
} string_t;

#ifdef MEM_TEST
string_t init_str(const char *str, const char *fn, size_t line);
#define init_str(str) init_str(str, __FILE__, __LINE__)
#else
string_t init_str(const char *str);
#endif
string_t copy_str(const string_t *str);
bool     concat(string_t *stra, string_t *strb);
bool     concat_cstr(string_t *stra, const char *strb);
bool     concatf_cstr(string_t *stra, const char *strb, ...);
bool     addchar(string_t *str, char ch);
bool     insert(string_t *stra, string_t *strb, uint32_t position);
bool     insert_cstr(string_t *stra, char *strb, uint32_t position);
bool     removechar(string_t *str, int32_t position);
void     freestr(string_t *str);

/* This is an experimental iterator/generator
 *
 * Basically, if you call the iterator with the same
 * pointer it will iterate through the string.
 * When called with a different pointer however, it will
 * restart and iterate through that instead.
 *
 * example:
 *
 * {
 *     char ch;
 *     while ((ch = str_iterator(&str)) != '\0')
 *         printf("%c", ch);
 * }
 *
 *
 * Nested iteration will cause infinite loops, and for
 * this reason it will print an error if a new iteration
 * starts without finishing an old one
 *
 * It is probably better to call a for loop directly
 *
 * Again, this is mostly experimental for if I do
 * linked lists and so on
 * */
char str_iterator(const string_t *string);
