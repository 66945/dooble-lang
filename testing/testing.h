#pragma once

#include "stdbool.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define UNIMPLEMENTED() unimplemented(__LINE__, __FUNCTION__)
#define PANIC(err)      panic(err, __LINE__, __FUNCTION__)

void unimplemented(int line, const char *fn);
void panic(const char *err, int line, const char *fn);

#define DYNAMIC_ASSERT(cond, msg) if (!(cond)) PANIC(msg)

// unit testing
#define MAKE_TEST __attribute__((constructor)) static void
#define ADD_TEST(name) addTest(#name, __FILE_NAME__, __LINE__, name)

#define ASSERT(cond, assertion) do { \
	if (!(cond)) {                   \
		return (UnitTest_t) {        \
			TEST_ERROR,              \
			assertion,               \
			__LINE__,                \
		};                           \
	}                                \
} while (0)

#define ASSERT_FLT(flt1, flt2, assertion) ASSERT(ABSF(flt1 - flt2) < 0.0001, assertion)
#define ASSERT_STR(str1, str2, assertion) ASSERT(strcmp(str1, str2) == 0, assertion)

#define END_UNIT_TEST() return (UnitTest_t) { TEST_OK, NULL, __LINE__ }

typedef struct UnitTest_type UnitTest_t;
typedef UnitTest_t (*TestFn) (void);

typedef struct {
	char         *name;
	char         *file;
	unsigned int  line;
	TestFn        test;
} UnitTest;

typedef enum {
	TEST_OK     = 0,
	TEST_ERROR  = 1,
	TEST_WARN   = 2,
	TEST_IGNORE = 3,
} UnitTestResult;

struct UnitTest_type {
	UnitTestResult  passed;
	char           *assertion;
	unsigned int    line;
};

#define UNIT_ARR_INIT_SIZE 20
typedef struct {
	UnitTest *array;
	size_t    used;
	size_t    capacity;
} UnitTestArr;

void setupUnitTests(void);
bool addTest(char *name, char *file, unsigned int line, TestFn test);
void addTests(size_t argc, ...);
void runTests(void);

void unitTestEntry(void);

#define UNIT_TEST 1

#ifdef UNIT_TEST
#define MEM_TEST 1
#endif

#ifdef MEM_TEST
extern unsigned int allocated;
#define INIT_MEMORY_TESTS() initMemoryTests();
#define END_MEMORY_TESTS()  logMemoryLeak();
void initMemoryTests(void);
void logMemoryLeak(void);

typedef struct MallocHashItem_t MallocHashItem;
struct MallocHashItem_t {
	void           *ptr;  // ref
	size_t          size;
	char           *file; // str
	unsigned int    line;
	MallocHashItem *next; // ref
};

void mHashTableAdd(void *ptr, size_t size, char *file, unsigned int line);
MallocHashItem *mHashTableGet(void *ptr);
void mHashTableRemove(void *ptr);

void *wrap_malloc(size_t size, char *file, unsigned int line);
void *wrap_calloc(size_t nitems, size_t size, char *file, unsigned int line);
void *wrap_realloc(void *ptr, size_t size, char *file, unsigned int line);
void  wrap_free(void *pointer);

#define malloc(size)         wrap_malloc(size, __FILE__, __LINE__)
#define calloc(nitems, size) wrap_calloc(nitems, size, __FILE__, __LINE__)
#define realloc(ptr, size)   wrap_realloc(ptr, size, __FILE__, __LINE__)
#define free(p)              wrap_free(p)
#else
#define INIT_MEMORY_TESTS()
#define END_MEMORY_TESTS()
#endif
