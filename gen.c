#define emits(s) emit(s, strlen(s))

#define TYPE_NUM_SIZE 2

#define GEN_ADD   "pop B  \nA:=B+A \n"
#define GEN_ADDSZ strlen(GEN_ADD)

#define GEN_SUB   "pop B  \nA:=B-A \n"
#define GEN_SUBSZ strlen(GEN_SUB)

#define GEN_SHL   "pop B  \nA:=B<<A\n"
#define GEN_SHLSZ strlen(GEN_SHL)

#define GEN_SHR   "pop B  \nA:=B>>A\n"
#define GEN_SHRSZ strlen(GEN_SHR)

#define GEN_LESS  "pop B  \nA:=B<A \n"
#define GEN_LESSSZ strlen(GEN_LESS)

#define GEN_ASSIGN "pop B  \nM[B]:=A\n"
#define GEN_ASSIGNSZ strlen(GEN_ASSIGN)
#define GEN_ASSIGN8 "pop B  \nm[B]:=A\n"
#define GEN_ASSIGN8SZ strlen(GEN_ASSIGN)

static void gen_start() {
	emits("jmpCAFE\n");
}

static void gen_finish() {
	struct sym *main = sym_find("main");
	char s[32];
	sprintf(s, "%04x", main->addr);
	memcpy(code+3, s, 4);
	printf("%s", code);
}

static void gen_ret() {
	emits("ret    \n");
	stack_pos = stack_pos - 1;
}

static void gen_const(int n) {
	char s[32];
	sprintf(s, "A:=%04x\n", n);
	emits(s);
}

static void gen_push() {
	emits("push A \n");
	stack_pos = stack_pos + 1;
}

static void gen_pop(int n) {
	char s[32];
	if (n > 0) {
		sprintf(s, "pop%04x\n", n);
		emits(s);
		stack_pos = stack_pos - n;
	}
}

static void gen_stack_addr(int addr) {
	char s[32];
	sprintf(s, "sp@%04x\n", addr);
	emits(s);
}

static void gen_unref(int type) {
	if (type == TYPE_VAR) {
		emits("A:=M[A]\n");
	} else if (type == TYPE_CHAR) {
		emits("A:=m[A]\n");
	}
}

static void gen_call() {
	emits("call A \n");
	/* no, call doesn't increase current stack size */
	/*stack_pos = stack_pos + 1;*/
}
