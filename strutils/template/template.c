#include "template.h"
#include "../../utils/err.h"
#include "../../testing/testing.h"
#include <corecrt.h>
#include <stdbool.h>
#include <stdio.h>

static void copy_mark(FILE *file, char *mark) {
	for (int i = 0; i < MARK_SIZE - 1; i++) {
		char ch = fgetc(file);
		if (ch == EOF) {
			fclose(file);
			mark[i] = '\0';
			return;
		}
		if (ch == ']') {
			mark[i] = '\0';
			return;
		}

		mark[i] = ch;
	}

	mark[MARK_SIZE - 1] = '\0';
}

errno_t init_template(const char *filename, Template *tp) {
	*tp = (Template) {
		.position	= 0,
		.mark		= "hi there",
		.output		= init_str(""),
	};

	if (fopen_s(&tp->source, filename, "r") != 0) {
		error("Cannot open file: ");
		error(filename);
		return 1;
	}

	next_mark(tp);

	tp->mark[0] = '\0';
	return 0;
}

void tputchar(Template *tp, char ch) {
	addchar(&tp->output, ch);
}

void tputstr(Template *tp, const char *str) {
	concat_cstr(&tp->output, str);
}

bool next_mark(Template *tp) {
	char ch;
	while ((ch = fgetc(tp->source)) != EOF) {
		// somewhat janky comparison
		if (ch == '@') {
			char next;
			if ((next = fgetc(tp->source)) == '[') {
				copy_mark(tp->source, tp->mark);
				return false;
			} else {
				ungetc(next, tp->source);
			}
		}

		addchar(&tp->output, ch);
		tp->position++;
	}

	fclose(tp->source);
	return true;
}

void free_template(Template *tp) {
	freestr(&tp->output);
}
