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

int stack_pos = 0;
int mem_pos = 0;

static struct sym *sym_find(char *s) {
	int i;
	struct sym *symbol = NULL;
	for (i = 0; i < sympos; i++) {
		if (strcmp(sym[i].name, s) == 0) {
			symbol = &sym[i];
		}
	}
	return symbol;
}

static struct sym *sym_declare(char *name, char type, int addr) {
	strncpy(sym[sympos].name, name, MAXTOKSZ);
	sym[sympos].addr = addr;
	sym[sympos].type = type;
	sympos++;
	return &sym[sympos-1];
}

/*
 * BACKEND
 */
#define MAXCODESZ 4096
static char code[MAXCODESZ];
static int codepos = 0;

static void emit(void *buf, size_t len) {
	memcpy(code + codepos, buf, len);
	codepos += len;
}

#define TYPE_NUM  0
#define TYPE_CHAR 1
#define TYPE_VAR  2

#include "gen.c"

/*
 * PARSER AND COMPILER
 */

static int expr();

/* read type name: int, char and pointers are supported */
static int typename() {
	if (peek("int") || peek("char")) {
		readtok();
		while (accept("*"));
		return 1;
	}
	return 0;
}

static int prim_expr() {
	int type = TYPE_NUM;
	if (isdigit(tok[0])) {
		int n = strtol(tok, NULL, 10);
		gen_const(n);
	} else if (isalpha(tok[0])) {
		struct sym *s = sym_find(tok);
		if (s == NULL) {
			error("Undeclared symbol: %s\n", tok);
		}
		if (s->type == 'L') {
			gen_stack_addr(stack_pos - s->addr - 1);
		} else {
			gen_const(s->addr);
		}
		type = TYPE_VAR;
	} else if (accept("(")) {
		type = expr();
		expect(")");
	} else {
		error("Unexpected primary expression: %s\n", tok);
	}
	readtok();
	return type;
}

static int binary(int type, int (*f)(), char *buf, size_t len) {
	if (type != TYPE_NUM) {
		gen_unref(type);
	}
	gen_push();
	type = f();
	if (type != TYPE_NUM) {
		gen_unref(type);
	}
	emit(buf, len);
	stack_pos = stack_pos - 1; /* assume that buffer contains a "pop" */
	return TYPE_NUM;
}

static int postfix_expr() {
	int type = prim_expr();
	if (type == TYPE_VAR && accept("[")) {
		binary(TYPE_NUM, expr, GEN_ADD, GEN_ADDSZ);
		expect("]");
		type = TYPE_CHAR;
	} else if (accept("(")) {
		int prev_stack_pos = stack_pos;
		gen_push(); /* store function address */
		int call_addr = stack_pos - 1;
		if (accept(")") == 0) {
			expr();
			gen_push();
			while (accept(",")) {
				expr();
				gen_push();
			}
			expect(")");
		}
		type = TYPE_NUM;
		gen_stack_addr(stack_pos - call_addr - 1);
		gen_unref(TYPE_VAR);
		gen_call();
		/* remove function address and args */
		gen_pop(stack_pos - prev_stack_pos);
		stack_pos = prev_stack_pos;
	}
	return type;
}

static int add_expr() {
	int type = postfix_expr();
	while (peek("+") || peek("-")) {
		if (accept("+")) {
			type = binary(type, postfix_expr, GEN_ADD, GEN_ADDSZ);
		} else if (accept("-")) {
			type = binary(type, postfix_expr, GEN_SUB, GEN_SUBSZ);
		}
	}
	return type;
}

static int shift_expr() {
	int type = add_expr();
	while (peek("<<") || peek(">>")) {
		if (accept("<<")) {
			add_expr();
			type = binary(type, add_expr, GEN_SHL, GEN_SHLSZ);
		} else if (accept(">>")) {
			add_expr();
			type = binary(type, add_expr, GEN_SHR, GEN_SHRSZ);
		}
	}
	return type;
}

static int rel_expr() {
	int type = shift_expr();
	while (peek("<")) {
		if (accept("<")) {
			shift_expr();
			type = binary(type, shift_expr, GEN_LESS, GEN_LESSSZ);
		}
	}
	return type;
}

static int eq_expr() {
	int type = rel_expr();
	while (peek("==") || peek("!=")) {
		type = TYPE_NUM;
		if (accept("==")) {
			rel_expr();
			DEBUG(" == ");
		} else if (accept("!=")) {
			rel_expr();
			DEBUG("!=");
		}
	}
	return type;
}

static int bitwise_expr() {
	int type = eq_expr();
	while (peek("|") || peek("&")) {
		type = TYPE_NUM;
		if (accept("|")) {
			eq_expr();
			DEBUG(" OR ");
		} else if (accept("&")) {
			eq_expr();
			DEBUG(" AND ");
		}
	}
	return type;
}

static int expr() {
	int type = bitwise_expr();
	/*DEBUG("type: %d\n", type);*/
	if (type != TYPE_NUM) {
		if (accept("=")) {
			gen_push(); expr(); emits(GEN_ASSIGN);
			stack_pos = stack_pos - 1; /* assume ASSIGN contains pop */
			type = TYPE_NUM;
		} else {
			gen_unref(type);
		}
	}
	return type;
}

static void statement() {
	if (accept("{")) {
		int prev_stack_pos = stack_pos;
		while (accept("}") == 0) {
			statement();
		}
		gen_pop(stack_pos-prev_stack_pos);
		stack_pos = prev_stack_pos;
		/* TODO: remove locals */
	} else if (typename()) {
		sym_declare(tok, 'L', stack_pos);
		readtok();
		if (accept("=")) {
			expr(); /* assume that expr() leave stack on the save level */
		}
		gen_push(); /* make room for new local variable */
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
		gen_pop(stack_pos); /* remove all locals from stack (except return address) */
		gen_ret();
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
		/*DEBUG("new symbol: %s\n", tok);*/
		struct sym *var = sym_declare(tok, 'U', mem_pos);
		readtok();
		if (accept(";")) {
			mem_pos = mem_pos + TYPE_NUM_SIZE;
			continue;
		}
		expect("(");
		int argc = 0;
		for (;;) {
			argc++;
			if (typename() == 0) {
				break;
			}
			sym_declare(tok, 'L', -argc-1);
			readtok();
			if (peek(")")) {
				break;
			}
			expect(",");
		}
		expect(")");
		if (accept(";") == 0) {
			stack_pos = 0;
			var->addr = codepos;
			statement(); /* function body */
			gen_ret(); /* another ret is user forgets to put 'return' */
		}
	}
}

int main(int argc, char *argv[]) {
	f = stdin;
	/* prefetch first char and first token */
	nextc = fgetc(f);
	readtok();
	gen_start();
	compile();
	gen_finish();
	return 0;
}

