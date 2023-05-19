#include "file.h"
#include "../strutils/str.h"
#include "../testing/testing.h"
#include "err.h"
#include <corecrt.h>
#include <stdio.h>

string_t read_file(const char *filename) {
	string_t filestr = init_str("");

	FILE *file;
	if (fopen_s(&file, filename, "r") != 0) {
		error("Cannot open file: ");
		error(filename);
		return filestr;
	}

	char ch;
	while ((ch = fgetc(file)) != EOF) {
		addchar(&filestr, ch);
	}

	fclose(file);
	return filestr;
}

bool write_file(string_t *str, const char *filename) {
	FILE *file;
	if (fopen_s(&file, filename, "w") != 0) {
		error("Cannot open file: ");
		error(filename);
		return false;
	}

	for (int i = 0; i < str->size; i++) {
		fputc(str->str[i], file);
	}

	fclose(file);
	return true;
}
