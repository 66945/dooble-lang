#include "input.h"
#include "../strutils/str.h"
#include <stdio.h>

string_t get_input(void) {
	let str = init_str("");

	putchar('\n');
	char ch = 0;
	while ((ch = getchar()) != '\n') {
		addchar(&str, ch);
	}

	return str;
}
