#pragma once

#include "../str.h"
#include <corecrt.h>
#include <stdint.h>
#include <stdio.h>

#define MARK_SIZE 30
typedef struct {
	FILE     *source;
	char      mark[30];
	uint64_t  position;
	string_t  output;
} Template;

errno_t init_template(const char *filename, Template *tp);
void    tputchar(Template *tp, char ch);
void    tputstr(Template *tp, const char *str);
bool    next_mark(Template *tp);

void free_template(Template *tp);
