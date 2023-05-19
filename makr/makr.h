#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <time.h>
#include <sys/stat.h>

typedef struct {
	size_t  size;
	char   *files;
} Directory;

typedef struct {
	const char  *output_name; // str
	char       **files;       // arr of str
	char       **flags;       // arr of str
	size_t       files_len;
	size_t       flags_len;
	size_t       files_cap;
	size_t       flags_cap;
	bool         run;
	char        *run_flags;   // str
} CompilationUnit;

CompilationUnit fork_build(CompilationUnit *unit, const char *name);
void            compile(CompilationUnit *unit);

#define FOR_FLAGS for (int i = 1; i < argc; i++)
#define CHECK_FLAG(flags, flag) do {                    \
	if (strcmp(argv[i], #flag) == 0) flags.flag = true; \
} while (0)

#define RUN(flags)     run(&build_unit, flags)
#define ADD_FILES(...) add_files(&build_unit, __VA_ARGS__, NULL)
#define ADD_FLAGS(...) add_flags(&build_unit, __VA_ARGS__, NULL)

void run(CompilationUnit *unit, char *flags);
void add_files(CompilationUnit *unit, ...);
void add_flags(CompilationUnit *unit, ...);

// logging functions
void INFO  (const char *fmt, ...); // TODO: Implementation
void WARN  (const char *fmt, ...); // TODO: Implementation
void ERROR (const char *fmt, ...); // TODO: Implementation

// the magic entry point of my build system
void build(int argc, char *argv[]);

#ifndef BUILD_FILE
#define BUILD_FILE "makr_compile.c"
#endif

#ifdef MAKR_IMPL
static CompilationUnit build_unit;

CompilationUnit fork_build(CompilationUnit *unit, const char *name) {
	if (unit == NULL) {
		return (CompilationUnit) {
			.output_name = name,
			.files       = malloc(sizeof(char *) * 10),
			.flags       = malloc(sizeof(char *) * 10),
			.files_len   = 0,
			.flags_len   = 0,
			.files_cap   = 10,
			.flags_cap   = 10,
			.run         = false,
		};
	} else {
		char **files = malloc(sizeof(char *) * unit->files_len);
		char **flags = malloc(sizeof(char *) * unit->flags_len);

		memmove(files, unit->files, sizeof(char *) * unit->files_len);
		memmove(flags, unit->flags, sizeof(char *) * unit->flags_len);

		return (CompilationUnit) {
			.output_name = name,
			.files       = files,
			.flags       = flags,
			.files_len   = unit->files_len,
			.flags_len   = unit->flags_len,
			.files_cap   = unit->files_cap,
			.flags_cap   = unit->flags_cap,
			.run         = false,
		};
	}
}

void run(CompilationUnit *unit, char *flags) {
	unit->run       = true;
	unit->run_flags = flags;
}

void add_files(CompilationUnit *unit, ...) {
	va_list args;
	va_start(args, unit);

	for (char *file = va_arg(args, char *);
			file != NULL;
			file = va_arg(args, char *))
	{
		if (unit->files_len >= unit->files_cap) {
			char **tmp = realloc(unit->files, sizeof(char *) * unit->files_cap * 2);
			if (tmp == NULL) {
				printf("[FATAL]: could not add file [%s]\n", file);
				exit(1);
			}

			unit->files = tmp;
		}

		printf("[BUILD]: adding file [%s]\n", file);
		unit->files[unit->files_len++] = file;
	}
}

void add_flags(CompilationUnit *unit, ...) {
	va_list args;
	va_start(args, unit);

	for (char *flag = va_arg(args, char *);
			flag != NULL; flag = va_arg(args, char *))
	{
		if (unit->flags_len >= unit->flags_cap) {
			char **tmp = realloc(unit->flags, sizeof(char *) * unit->flags_cap * 2);
			if (tmp == NULL) {
				printf("[FATAL]: could not add flag [%s]\n", flag);
				exit(1);
			}

			unit->flags = tmp;
		}

		printf("[BUILD]: adding flag [%s]\n", flag);
		unit->flags[unit->flags_len++] = flag;
	}
}

void add_directory(CompilationUnit *unit, const char *dir_name) {
	// TODO: Implementation
}

void compile(CompilationUnit *unit) {
	printf("[BUILD]: compiling...\n");

	size_t cmd_size = 0;
	for (size_t i = 0; i < unit->files_len; i++) {
		cmd_size += strlen(unit->files[i]) + 1;
	}
	for (size_t i = 0; i < unit->flags_len; i++) {
		cmd_size += strlen(unit->flags[i]) + 1;
	}

	cmd_size += strlen(unit->output_name) + 20;

	char *cmd = malloc(sizeof(char) * cmd_size);
	strcpy(cmd, "clang ");

	for (size_t i = 0; i < unit->files_len; i++) {
		strcat(cmd, unit->files[i]);
		strcat(cmd, " ");
	}

	strcat(cmd, "-o ");
	strcat(cmd, unit->output_name);
	strcat(cmd, " ");

	for (size_t i = 0; i < unit->flags_len; i++) {
		strcat(cmd, unit->flags[i]);
		strcat(cmd, " ");
	}

	int err = system(cmd);
	free(cmd);

	printf("[BUILD]: done :-)\n");

	if (unit->run && err == 0) {
		char *runcmd = malloc(sizeof(char) *
				(strlen(unit->output_name) +
				 strlen(unit->run_flags)));

		strcpy(runcmd, unit->output_name);
		strcat(runcmd, " ");
		strcat(runcmd, unit->run_flags);

		printf("[BUILD]: running\n");
		system(runcmd);
		free(runcmd);
	}
}

int main(int argc, char *argv[]) {
	struct stat compiled;
	struct stat script;
	stat("makr.exe", &compiled);
	stat(BUILD_FILE, &script);

	if (compiled.st_mtime < script.st_mtime) {
		printf("[BUILD]: recompiling\n");
		system("start powershell \"clang " BUILD_FILE " -o makr.exe\"");
	} else {
		build_unit = fork_build(NULL, "build.exe");

		build(argc, argv);
		compile(&build_unit);
	}

	return 0;
}
#endif
