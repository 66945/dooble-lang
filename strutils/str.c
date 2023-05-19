#include "str.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// for mem testing
#undef malloc
#undef calloc
#undef realloc
#undef free
#undef init_str

#ifdef MEM_TEST
string_t init_str(const char *str, const char *fn, size_t line) {
#else
string_t init_str(const char *str) {
#endif
	string_t string = { .size = strlen(str) }; 
	bool small_string = string.size < SMALL_SIZE;

	string.capacity = small_string ? SMALL_SIZE : string.size * 2;
#ifdef MEM_TEST
	string.str = wrap_malloc(sizeof(char) * string.capacity, (char *) fn, line);
#else
	string.str = malloc(sizeof(char) * string.capacity);
#endif

	strcpy_s(string.str, string.capacity, str);
	return string;
}

string_t copy_str(const string_t *str) {
	string_t string = {
		.size		= str->size,
		.capacity	= str->capacity,
	};

	string.str = malloc(sizeof(char) * string.capacity);
	strcpy_s(string.str, string.capacity, str->str);

	return string;
}

#define GROW_CAPACITY_STR_MERGE(sizea, sizeb, cap, str)	\
do {													\
	if (sizea + sizeb + 1 > cap) {						\
		cap = cap * 2 > sizea + sizeb ?					\
			cap * 2 : cap + sizea + sizeb;				\
		str = realloc(str, sizeof(char) * cap);			\
		if (str == NULL)								\
			return false;								\
	}													\
} while (0)

bool concat(string_t *stra, string_t *strb) {
	if (stra == NULL || strb == NULL) return false;

	GROW_CAPACITY_STR_MERGE(stra->size, strb->size, stra->capacity, stra->str);
	stra->size += strb->size;
	return strcat_s(stra->str, stra->capacity, strb->str) == 0;
}

// There is a more efficient way to do this, because of how addchar
// shifts char by char, but I don't care yet
bool concat_cstr(string_t *stra, const char *strb) {
	if (stra == NULL || strb == NULL) return false;

	for (const char *p = strb; *p != 0; p++) {
		if (!addchar(stra, *p)) return false;
	}

	return true;
}

bool concatf_cstr(string_t *stra, const char *strb, ...) {
	va_list args;
	va_start(args, strb);

	// FIXME: find a better way to ensure allocation size is large enough
	// than just multiplying by 4
	size_t sizeb = strlen(strb) * 4;
	GROW_CAPACITY_STR_MERGE(stra->size, sizeb, stra->capacity, stra->str);

	char *write_buf = stra->str + stra->size;
	int concat_len = vsprintf_s(write_buf, stra->capacity - stra->size, strb, args);

	if (concat_len < 0) {
		error("Concatf failed\n");
		return false;
	}

	stra->size += concat_len;
	return true;
}

bool addchar(string_t *str, char ch) {
	if (str == NULL || ch == '\0') return false;

	// this condition should still hold true even when using SSO
	// + 2 for the null terminator and for the added character
	if (str->size + 2 > str->capacity) {
		str->capacity *= 2;

		str->str = realloc(str->str, sizeof(char) * str->capacity);
		if (str->str == NULL)
			return false;
	}

	str->size++;
	str->str[str->size - 1] = ch;
	str->str[str->size    ] = '\0';
	return true;
}

bool removechar(string_t *str, int32_t pos) {
	if (pos < 0) pos += str->size;

	let tmp = memmove(&str->str[pos], &str->str[pos + 1], str->size - pos);
	str->size--;
	return tmp != NULL;
}

bool insert(string_t *stra, string_t *strb, uint32_t position) {
	if (stra == NULL || strb == NULL) return false;
	if (position > stra->size)        return false;

	GROW_CAPACITY_STR_MERGE(stra->size, strb->size, stra->capacity, stra->str);

	uint32_t newSize   = stra->size + strb->size;
	uint32_t insertEnd = strb->size + position;

	for (int i = newSize; i >= insertEnd; i--)
		stra->str[i] = stra->str[i - strb->size];

	for (int i = 0; i < strb->size; i++)
		stra->str[i + position] = strb->str[i];

	stra->size = newSize;
	return true;
}

bool insert_cstr(string_t *stra, char *strb, uint32_t position) {
	if (stra == NULL || strb == NULL)	return false;
	if (position > stra->size)			return false;

	uint32_t sizeb = strlen(strb);
	GROW_CAPACITY_STR_MERGE(stra->size, sizeb, stra->capacity, stra->str);

	uint32_t newSize	= stra->size + sizeb;
	uint32_t insertEnd	= sizeb + position;

	for (int i = newSize; i >= insertEnd; i--)
		stra->str[i] = stra->str[i - sizeb];

	for (int i = 0; i < sizeb; i++)
		stra->str[i + position] = strb[i];

	stra->size = newSize;
	return true;
}

char str_iterator(const string_t *_string) {
	static const string_t *string	= NULL;
	static unsigned int it			= 0;

	// if iterator is on first run through this string
	if (_string != string) {
		if (string != NULL && it < string->size)
			warn("new iteration started without completing prior string\n");

		string = _string;
		it = 0;
	}

	if (it < string->size)
		return string->str[it++];

	return '\0';
}

void freestr(string_t *str) {
	if (str->str == NULL) return;

	if (MEM_TEST) {
		wrap_free(str->str);
	}
	else {
		free(str->str);
	}

	str->str = NULL;
}
