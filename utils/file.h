#pragma once

#include "../strutils/str.h"

typedef struct FileTree_t FileTree;

struct FileTree_t {
	FileTree	*subfolders;
	char		**files;
};

string_t read_file(const char *filename);
bool write_file(string_t *str, const char *filename);
