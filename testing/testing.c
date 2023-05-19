#include "testing.h"
#include "stdbool.h"
#include "logging.h"
#include "../utils/err.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <windows.h>

#undef malloc
#undef calloc
#undef realloc
#undef free

// utility functions
void unimplemented(int line, const char *fn) {
	error("Unimplemented function %s called on line %d", fn, line);
	exit(1);
}

void panic(const char *err, int line, const char *fn) {
	error("Panic in function %s called on line %d", fn, line);
	error("\t\"%s\"", err);
	exit(1);
}

// unit testing
static UnitTestArr tests;
static bool        tests_setup = false;

static UnitTest_t memHashTable(void);

void setupUnitTests() {
	if (tests_setup) return;

	tests = (UnitTestArr) {
		.array    = malloc(sizeof(UnitTest) * UNIT_ARR_INIT_SIZE),
		.used     = 0,
		.capacity = UNIT_ARR_INIT_SIZE,
	};

	ADD_TEST(memHashTable);
	tests_setup = true;
}

bool addTest(char *name, char *file, unsigned int line, TestFn test) {
	if (tests.used >= tests.capacity) {
		tests.capacity *= 2;
		void *tmp = realloc(tests.array, sizeof(UnitTest) * tests.capacity);
		if (tmp == NULL) {
			free(tests.array);
			setupUnitTests();
			error("unit test reallocation failed");
			return false;
		}

		tests.array = tmp;
	}

	tests.array[tests.used++] = (UnitTest) {
		.name = name,
		.file = file,
		.line = line,
		.test = test
	};

	return true;
}

void addTests(size_t argc, ...) {
	va_list args;
	va_start(args, argc);

	for (int i = 0; i < argc; i++) {
		bool result = addTest(
				va_arg(args, char *),
				va_arg(args, char *),
				va_arg(args, unsigned int),
				va_arg(args, TestFn));

		if (!result) {
			LOG("Failed to allocate test %d", i);
		}
	}

	va_end(args);
}

void runTests() {
	int failNum = 0;
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

	for (int i = 0; i < tests.used; i++) {
		SetConsoleTextAttribute(hConsole,
				FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
		printf("Running test %s at %s:%d\n",
				tests.array[i].name, tests.array[i].file,
				tests.array[i].line);

		UnitTest_t testResult = tests.array[i].test();

		switch (testResult.passed) {
			case TEST_OK:
				SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN);
				printf("\tPassed\n");
				break;
			case TEST_ERROR:
				SetConsoleTextAttribute(hConsole, FOREGROUND_RED);
				printf("\tFailed\n");
				printf("\t%s at line %d\n\n", testResult.assertion, testResult.line);
				failNum++;
				break;
			case TEST_WARN:
				printf("\tWarning\n");
				break;
			case TEST_IGNORE:
				printf("\tIgnored\n");
				break;
		}
	}

	SetConsoleTextAttribute(hConsole, failNum > 0 ? FOREGROUND_RED : FOREGROUND_GREEN);
	printf("\n\t%zu tests completed %d failed\n", tests.used, failNum);
	SetConsoleTextAttribute(hConsole,
			FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}

#ifdef MEM_TEST
#define MHTABLE_SIZE 1024
unsigned int allocated          = 0;
static bool mallocHashTableOpen = FALSE;
static MallocHashItem mallocHashTable[MHTABLE_SIZE];

void initMemoryTests() {
	allocated = 0;
	mallocHashTableOpen = TRUE;
}

void logMemoryLeak() {
	printf("%d pointers still allocated\n", allocated);
	for (int i = 0; i < MHTABLE_SIZE; i++) {
		MallocHashItem *item = &mallocHashTable[i];

		while (item != NULL && item->ptr != NULL) {
			printf("\t%p with size %zu at %s:%d\n",
					item->ptr, item->size, item->file,
					item->line);
			item = item->next;
		}
	}
}

static unsigned int ptrHash(uintptr_t ptr) {
	ptr = ((ptr >> 16) ^ ptr) * 0x45d9f3b;
	ptr = ((ptr >> 16) ^ ptr) * 0x45d9f3b;
	ptr = ((ptr >> 16) ^ ptr);
	return ptr;
}

void mHashTableAdd(void *ptr, size_t size, char *file, unsigned int line) {
	if (!mallocHashTableOpen || ptr == NULL) return;

	const unsigned int index = ptrHash((uintptr_t) ptr) % MHTABLE_SIZE;
	MallocHashItem hashItem = {
		.ptr	= ptr,
		.size	= size,
		.file	= file,
		.line	= line,
		.next	= NULL
	};

	if (mallocHashTable[index].ptr == NULL) {
		mallocHashTable[index] = hashItem;
		allocated++;
	} else {
		MallocHashItem *item = &mallocHashTable[index];

		while(item->next != NULL) {
			if(item->ptr == ptr) {
				item->ptr	= ptr;
				item->size	= size;
				item->file	= file;
				item->line	= line;
				return;
			}

			item = item->next;
		}

		MallocHashItem *newItem = malloc(sizeof(MallocHashItem));
		*newItem = hashItem;
		item->next = newItem;
		allocated++;
	}
}

// For the purposes of testing and possible future use
MallocHashItem *mHashTableGet(void *ptr) {
	if (ptr == NULL) return NULL;

	const unsigned int index = ptrHash((uintptr_t) ptr) % MHTABLE_SIZE;
	MallocHashItem *item = &mallocHashTable[index];

	while (item != NULL) {
		if (item->ptr == ptr) {
			return item;
		}
		item = item->next;
	}

	return NULL;
}

/* the situation:
 * [0 0 0 0 0 0 4 0 0 8 0 0 0 0]
 *              |     |
 *              5     9
 *              |
 *              6 <- what we want to free
 * */
void mHashTableRemove(void *ptr) {
	if (ptr == NULL) return;

	const unsigned int index = ptrHash((uintptr_t) ptr) % MHTABLE_SIZE;
	MallocHashItem *item = &mallocHashTable[index];
	if (item->ptr == NULL) {
		return;
	} else if (item->ptr == ptr) {
		if (item->next == NULL) {
			item->ptr	= NULL;
			item->size	= 0;
			item->file	= NULL;
			item->line	= 0;
			item->next	= NULL;
		} else {
			MallocHashItem *toRemove = item->next;
			*item = *item->next;
			free(toRemove);
		}

		allocated--;
		return;
	}

	while (item->next != NULL) {
		if (item->next->ptr == ptr) {
			MallocHashItem *toRemove = item->next;
			item->next = item->next->next;
			free(toRemove);
			allocated--;
			return;
		}
		item = item->next;
	}
}

void *wrap_malloc(size_t size, char *file, unsigned int line) {
	void *ptr = malloc(size);
	mHashTableAdd(ptr, size, file, line);
	return ptr;
}

void *wrap_calloc(size_t nitems, size_t size, char *file, unsigned int line) {
	void *ptr = calloc(nitems, size);
	mHashTableAdd(ptr, nitems * size, file, line);
	return ptr;
}

void *wrap_realloc(void *ptr, size_t size, char *file, unsigned int line) {
	void *tmp = realloc(ptr, size);
	mHashTableAdd(tmp, size, file, line);
	return tmp;
}

void wrap_free(void *pointer) {
	mHashTableRemove(pointer);
	free(pointer);
}

#ifdef UNIT_TEST
// MARK: Memory tests

#define malloc(size)         wrap_malloc(size, __FILE__, __LINE__)
#define calloc(nitems, size) wrap_calloc(nitems, size, __FILE__, __LINE__)
#define realloc(ptr, size)   wrap_realloc(ptr, size, __FILE__, __LINE__)
#define free(p)              wrap_free(p)

static UnitTest_t memHashTable(void) {
#define MEM_TEST_SIZE 100
	ASSERT(allocated == 0, "allocations do not start at 0");

	volatile void *parr[MEM_TEST_SIZE];
	for (int i = 0; i < MEM_TEST_SIZE; i++) {
		parr[i] = malloc(50);
	}

	ASSERT(allocated == MEM_TEST_SIZE,
			"Mid allocation is not 100 (may be optimization err)");

	for (int i = 0; i < MEM_TEST_SIZE; i++) {
		free((void *) parr[i]);
	}

	ASSERT(allocated == 0, "End allocation was not 0");

	int leftover_count = 0;
	for (int i = 0; i < MEM_TEST_SIZE; i++) {
		leftover_count += mHashTableGet((void *) parr[i]) == NULL ? 0 : 1;
	}

	ASSERT(leftover_count == 0, "Was able to get more than 0 elements");

	END_UNIT_TEST();
#undef MEM_TEST_SIZE
}

void unitTestEntry(void) {
	runTests();
}
#endif
#endif
