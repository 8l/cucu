#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>

static void error(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	exit(1);
}

#define MAXTOKSZ 256

/*
 * TOKENIZER
 */
static FILE *f;
static char tok[MAXTOKSZ];
static int toksz;
static int nextc;
static int tokpos;

void readchr() {
	if (tokpos == MAXTOKSZ - 1) {
		tok[tokpos] = '\0';
		error("Token too long: %s", tok);
	}
	tok[tokpos++] = nextc;
	nextc = fgetc(f);
}

void readtok() {
	for (;;) {
		/* skip spaces */
		while (isspace(nextc)) {
			nextc = fgetc(f);
		}
		/* try to read a literal token */
		tokpos = 0;
		while (isalnum(nextc) || nextc == '_') {
			readchr();
		}
		/* if it's not a literal token */
		if (tokpos == 0) {
			while (nextc == '<' || nextc == '=' || nextc == '>'
					|| nextc == '!' || nextc == '&' || nextc == '|') {
				readchr();
			}
		}
		if (tokpos == 0) {
			if (nextc == '\'' || nextc == '"') {
				char c = nextc;
				readchr();
				while (nextc != c) {
					readchr();
				}
			} else if (nextc == '/') { /* skip comments */
				readchr();
				if (nextc == '*') {
					nextc = fgetc(f);
					while (nextc != '/') {
						while (nextc != '*') {
							nextc = fgetc(f);
						}
						nextc = fgetc(f);
					}
					nextc = fgetc(f);
					continue;
				}
			} else if (nextc != EOF) {
				readchr();
			}
		}
		break;
	}
	tok[tokpos] = '\0';
}

int peek(char *s) {
	return (strcmp(tok, s) == 0);
}

int accept(char *s) {
	if (peek(s)) {
		readtok();
		return 1;
	}
	return 0;
}

int expect(char *s) {
	if (accept(s) == 0) {
		error("Error: expected '%s'\n", s);
	}
}

/*
 * LEXER AND COMPILER
 */
void compile() {
	while (tok[0] != 0) {
		fprintf(stderr, "TOKEN: %s\n", tok);
		readtok();
	}
}

int main(int argc, char *argv[]) {
	f = stdin;
	/* prefetch first char and first token */
	nextc = fgetc(f);
	readtok();
	compile();
	return 0;
}

