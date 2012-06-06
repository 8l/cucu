#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

static void error(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	exit(1);
}

#define DEBUG(...) fprintf(stderr, __VA_ARGS__)

#define MAXTOKSZ 256

/*
 * LEXER
 */
static FILE *f;
static char tok[MAXTOKSZ];
static int tokpos;
static int nextc;

void readchr() {
	if (tokpos == MAXTOKSZ - 1) {
		tok[tokpos] = '\0';
		error("Token too long: %s\n", tok);
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

void expect(char *s) {
	if (accept(s) == 0) {
		error("Error: expected '%s', but found: %s\n", s, tok);
	}
}

/*
 * SYMBOLS
 */
#define MAXSYMBOLS 4096
static struct sym {
	char type;
	int addr;
	char name[MAXTOKSZ];
} sym[MAXSYMBOLS];
static int sympos = 0;

static struct sym *sym_find(char *s) {
	int i;
	struct sym *symbol;
	for (i = 0; i < sympos; i++) {
		if (strcmp(sym[i].name, s) == 0) {
			symbol = &sym[i];
		}
	}
	return symbol;
}

static void sym_declare(char *name, char type, int addr) {
	strncpy(sym[sympos].name, name, MAXTOKSZ);
	sym[sympos].addr = addr;
	sym[sympos].type = type;
	sympos++;
}

/*
 * PARSER AND COMPILER
 */

static void expr();

/* read type name: int, char and pointers are supported */
static int typename() {
	if (peek("int") || peek("char")) {
		readtok();
		while (accept("*"));
		return 1;
	}
	return 0;
}

static void prim_expr() {
	if (isdigit(tok[0])) {
		DEBUG(" const-%s ", tok);
	} else if (isalpha(tok[0])) {
		DEBUG(" var-%s ", tok);
	} else if (accept("(")) {
		expr();
		expect(")");
	} else {
		error("Unexpected primary expression: %s\n", tok);
	}
	readtok();
}

static void postfix_expr() {
	prim_expr();
	if (accept("[")) {
		expr();
		expect("]");
		DEBUG(" [] ");
	} else if (accept("(")) {
		if (accept(")") == 0) {
			expr();
			DEBUG(" FUNC-ARG\n");
			while (accept(",")) {
				expr();
				DEBUG(" FUNC-ARG\n");
			}
			expect(")");
		}
		DEBUG(" FUNC-CALL\n");
	}
}

static void add_expr() {
	postfix_expr();
	while (peek("+") || peek("-")) {
		if (accept("+")) {
			postfix_expr();
			DEBUG(" + ");
		} else if (accept("-")) {
			postfix_expr();
			DEBUG(" - ");
		}
	}
}

static void shift_expr() {
	add_expr();
	while (peek("<<") || peek(">>")) {
		if (accept("<<")) {
			add_expr();
			DEBUG(" << ");
		} else if (accept(">>")) {
			add_expr();
			DEBUG(" >> ");
		}
	}
}

static void rel_expr() {
	shift_expr();
	while (peek("<")) {
		if (accept("<")) {
			shift_expr();
			DEBUG(" < ");
		}
	}
}

static void eq_expr() {
	rel_expr();
	while (peek("==") || peek("!=")) {
		if (accept("==")) {
			rel_expr();
			DEBUG(" == ");
		} else if (accept("!=")) {
			rel_expr();
			DEBUG("!=");
		}
	}
}

static void bitwise_expr() {
	eq_expr();
	while (peek("|") || peek("&")) {
		if (accept("|")) {
			eq_expr();
			DEBUG(" OR ");
		} else if (accept("&")) {
			eq_expr();
			DEBUG(" AND ");
		}
	}
}

static void expr() {
	bitwise_expr();
	if (accept("=")) {
		expr();
		DEBUG(" := ");
	}
}

static void statement() {
	if (accept("{")) {
		while (accept("}") == 0) {
			statement();
		}
	} else if (typename()) {
		DEBUG("local variable: %s\n", tok);
		readtok();
		if (accept("=")) {
			expr();
			DEBUG(" :=\n");
		}
		expect(";");
	} else if (accept("if")) {
		/* TODO */
	} else if (accept("while")) {
		/* TODO */
	} else if (accept("return")) {
		if (peek(";") == 0) {
			expr();
		}
		expect(";");
		DEBUG("RET\n");
	} else {
		expr();
		expect(";");
	}
}

static void compile() {
	while (tok[0] != 0) { /* until EOF */
		if (typename() == 0) {
			error("Error: type name expected\n");
		}
		DEBUG("identifier: %s\n", tok);
		readtok();
		if (accept(";")) {
			DEBUG("variable definition\n");
			continue;
		}
		expect("(");
		int argc = 0;
		for (;;) {
			argc++;
			typename();
			DEBUG("function argument: %s\n", tok);
			readtok();
			if (peek(")")) {
				break;
			}
			expect(",");
		}
		expect(")");
		if (accept(";") == 0) {
			DEBUG("function body\n");
			statement();
		}
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

