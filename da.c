/********************************************
da.c
copyright 2008-2023,2024, Thomas E. Dickey
copyright 1991-1994,1995, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: da.c,v 1.49 2024/08/18 23:26:39 tom Exp $
 */

/*  da.c  */
/*  disassemble code */

#include  <mawk.h>

#include  <code.h>
#include  <bi_funct.h>
#include  <repl.h>
#include  <field.h>

typedef struct fdump {
    struct fdump *link;
    FBLOCK *data;
} DUMP_FUNCS;

static DUMP_FUNCS *fdump_list;	/* linked list of all user functions */

#ifdef LOCAL_REGEXP
#include <regexp.h>

typedef struct regex {
    struct regex *link;
    PTR data;
} DUMP_REGEX;

static DUMP_REGEX *regex_list;	/* list regular expressions dumped */

static void add_to_regex_list(PTR);
#else
#define add_to_regex_list(p)	/* nothing */
#endif /* LOCAL_REGEXP */

typedef struct {
    char op;
    const char name[12];
} OP_NAME;

static const char *find_bi_name(PF_CP);
/* *INDENT-OFF* */
static const OP_NAME simple_code[] =
{
    { _STOP,      "stop" },
    { FE_PUSHA,   "fe_pusha" },
    { FE_PUSHI,   "fe_pushi" },
    { A_TEST,     "a_test" },
    { A_DEL,      "a_del" },
    { DEL_A,      "del_a" },
    { POP_AL,     "pop_al" },
    { _POP,       "pop" },
    { _ADD,       "add" },
    { _SUB,       "sub" },
    { _MUL,       "mul" },
    { _DIV,       "div" },
    { _MOD,       "mod" },
    { _POW,       "pow" },
    { _NOT,       "not" },
    { _UMINUS,    "uminus" },
    { _UPLUS,     "uplus" },
    { _TEST,      "test" },
    { _CAT,       "cat" },
    { _ASSIGN,    "assign" },
    { _ADD_ASG,   "add_asg" },
    { _SUB_ASG,   "sub_asg" },
    { _MUL_ASG,   "mul_asg" },
    { _DIV_ASG,   "div_asg" },
    { _MOD_ASG,   "mod_asg" },
    { _POW_ASG,   "pow_asg" },
    { NF_PUSHI,   "nf_pushi" },
    { F_ASSIGN,   "f_assign" },
    { F_ADD_ASG,  "f_add_asg" },
    { F_SUB_ASG,  "f_sub_asg" },
    { F_MUL_ASG,  "f_mul_asg" },
    { F_DIV_ASG,  "f_div_asg" },
    { F_MOD_ASG,  "f_mod_asg" },
    { F_POW_ASG,  "f_pow_asg" },
    { _POST_INC,  "post_inc" },
    { _POST_DEC,  "post_dec" },
    { _PRE_INC,   "pre_inc" },
    { _PRE_DEC,   "pre_dec" },
    { F_POST_INC, "f_post_inc" },
    { F_POST_DEC, "f_post_dec" },
    { F_PRE_INC,  "f_pre_inc" },
    { F_PRE_DEC,  "f_pre_dec" },
    { _EQ,        "eq" },
    { _NEQ,       "neq" },
    { _LT,        "lt" },
    { _LTE,       "lte" },
    { _GT,        "gt" },
    { _GTE,       "gte" },
    { _MATCH2,    "match2" },
    { _EXIT,      "exit" },
    { _EXIT0,     "exit0" },
    { _NEXT,      "next" },
    { _NEXTFILE,  "nextfile" },
    { _RET,       "ret" },
    { _RET0,      "ret0" },
    { _OMAIN,     "omain" },
    { _JMAIN,     "jmain" },
    { OL_GL,      "ol_gl" },
    { OL_GL_NR,   "ol_gl_nr" },
    { _HALT,      "" }
} ;
/* *INDENT-ON* */

static const char *jfmt = "%s%s%03d\n";
   /* format to print jumps */
static const char *tab2 = "\t\t";

#if OPT_TRACE
static FBLOCK *
inst2fblock(INST * p)
{
    DUMP_FUNCS *q;
    FBLOCK *result = NULL;
    for (q = fdump_list; q != NULL; q = q->link) {
	if (q->data->code == p) {
	    result = q->data;
	    break;
	}
    }
    return result;
}
#endif

void
da_string2(FILE *fp, const char *value, size_t length, int delim)
{
    size_t n;

    fputc(delim, fp);
    for (n = 0; n < length; ++n) {
	UChar ch = (UChar) value[n];
	switch (ch) {
	case '\\':
	    fputc(ch, fp);
	    break;
	case '\a':		/* alert, ascii 7 */
	    fputs("\\\\", fp);
	    break;
	case '\b':		/* backspace, ascii 8 */
	    fputs("\\b", fp);
	    break;
	case '\t':		/* tab, ascii 9 */
	    fputs("\\t", fp);
	    break;
	case '\n':		/* newline, ascii 10 */
	    fputs("\\n", fp);
	    break;
	case '\v':		/* vertical tab, ascii 11 */
	    fputs("\\v", fp);
	    break;
	case '\f':		/* formfeed, ascii 12 */
	    fputs("\\f", fp);
	    break;
	case '\r':		/* carriage return, ascii 13 */
	    fputs("\\r", fp);
	    break;
	default:
	    if (ch == delim)
		fprintf(fp, "\\%c", ch);
	    else if (ch >= 32 && ch < 127)
		fputc(ch, fp);
	    else
		fprintf(fp, "\\%03o", ch);
	    break;
	}
    }
    fputc(delim, fp);
}

void
da_string(FILE *fp, const STRING * sparm, int delim)
{
    da_string2(fp, sparm->str, sparm->len, delim);
}

#define NORMAL_FORM "%03ld .\t"
#define ADJUST_FORM  "# patching %s\n    .\t"

INST *
da_this(INST * p, const INST * start, FILE *fp)
{
    CELL *cp;
    const char *op_name = da_op_name(p);

    /* print the relative code address (label) */
    fprintf(fp, NORMAL_FORM, (long) (p - start));

    switch ((MAWK_OPCODES) (p++->op)) {

    case _PUSHC:
	cp = (CELL *) p++->ptr;
	switch (cp->type) {
	case C_RE:
	    add_to_regex_list(cp->ptr);
	    fprintf(fp, "%s\t%p\t", op_name, cp->ptr);
	    da_string(fp, re_uncompile(cp->ptr), '/');
	    fputc('\n', fp);
	    break;

	case C_SPACE:
	    fprintf(fp, "%s\tspace split\n", op_name);
	    break;

	case C_SNULL:
	    fprintf(fp, "%s\tnull split\n", op_name);
	    break;

	case C_REPL:
	    fprintf(fp, "%s\trepl\t", op_name);
	    da_string(fp, repl_uncompile(cp), '"');
	    fputc('\n', fp);
	    break;

	case C_REPLV:
	    fprintf(fp, "%s\treplv\t", op_name);
	    da_string(fp, repl_uncompile(cp), '"');
	    fputc('\n', fp);
	    break;

	default:
	    fprintf(fp, "%s\tWEIRD\n", op_name);;
	    break;
	}
	break;

    case _PUSHD:
	fprintf(fp, "%s\t%.6g\n", op_name, *(double *) p++->ptr);
	break;

    case _PUSHS:
	fprintf(fp, "%s\t", op_name);
	da_string(fp, (STRING *) p++->ptr, '"');
	fputc('\n', fp);
	break;

    case _MATCH0:
    case _MATCH1:
	add_to_regex_list(p->ptr);
	fprintf(fp, "%s\t%p\t", op_name, p->ptr);
	da_string(fp, re_uncompile(p->ptr), '/');
	fputc('\n', fp);
	p++;
	break;

    case _PUSHA:
	fprintf(fp, "%s\t%s\n", op_name, reverse_find(ST_VAR, &p++->ptr));
	break;

    case _PUSHI:
	cp = (CELL *) p++->ptr;
	if (cp == field)
	    fprintf(fp, "%s\t$0\n", op_name);
	else if (cp == &fs_shadow)
	    fprintf(fp, "%s\t@fs_shadow\n", op_name);
	else {
	    const char *name;
	    if (cp > NF && cp <= LAST_PFIELD)
		name = reverse_find(ST_FIELD, &cp);
	    else
		name = reverse_find(ST_VAR, &cp);

	    fprintf(fp, "%s\t%s\n", op_name, name);
	}
	break;

    case L_PUSHA:
    case L_PUSHI:
    case LAE_PUSHI:
    case LAE_PUSHA:
    case LA_PUSHA:
	fprintf(fp, "%s\t%d\n", op_name, p++->op);
	break;

    case F_PUSHA:
	cp = (CELL *) p++->ptr;
	if (cp >= NF && cp <= LAST_PFIELD)
	    fprintf(fp, "%s\t%s\n", op_name, reverse_find(ST_FIELD, &cp));
	else
	    fprintf(fp, "%s\t$%d\n", op_name, field_addr_to_index(cp));
	break;

    case F_PUSHI:
	p++;
	fprintf(fp, "%s\t$%d\n", op_name, p++->op);
	break;

    case AE_PUSHA:
    case AE_PUSHI:
    case A_PUSHA:
	fprintf(fp, "%s\t%s\n", op_name, reverse_find(ST_ARRAY, &p++->ptr));
	break;

    case A_LENGTH:
	{
	    SYMTAB *stp = (SYMTAB *) p++->ptr;

	    fprintf(fp, ADJUST_FORM, type_to_str(stp->type));
	    TRACE((NORMAL_FORM, (long) (p - 2 - start)));
	    TRACE(("patch/alen %s\n", type_to_str(stp->type)));

	    switch (stp->type) {
	    case ST_VAR:
		fprintf(fp, "pushi\t%s\t# defer_alen\n", stp->name);
		break;
	    case ST_ARRAY:
		fprintf(fp, "a_pusha\t%s\t# defer_alen\n", stp->name);
		p[1].ptr = (PTR) bi_alength;
		break;
	    case ST_NONE:
		fprintf(fp, "pushi\t%s\t# defer_alen\n", "@missing");
		break;
	    default:
		bozo("da A_LENGTH");
		/* NOTREACHED */
	    }
	}
	break;

    case _LENGTH:
	{
	    DEFER_LEN *dl = (DEFER_LEN *) p++->ptr;
	    FBLOCK *fbp = dl->fbp;
	    short offset = dl->offset;
	    int type = fbp->typev[offset];

	    fprintf(fp, ADJUST_FORM, type_to_str(type));
	    TRACE((NORMAL_FORM, (long) (p - 2 - start)));
	    TRACE(("patch/len %s\n", type_to_str(type)));

	    switch (type) {
	    case ST_LOCAL_VAR:
		fprintf(fp, "l_pushi\t@%hd\t# defer_len\n", offset);
		break;
	    case ST_LOCAL_ARRAY:
		fprintf(fp, "la_pusha\t@%hd\t# defer_len\n", offset);
		p[1].ptr = (PTR) bi_alength;
		break;
	    case ST_LOCAL_NONE:
		fprintf(fp, "pushi\t%s\t# defer_len\n", "@missing");
		break;
	    default:
		bozo("da _LENGTH");
		/* NOTREACHED */
	    }
	}
	break;

    case _PUSHINT:
	fprintf(fp, "%s\t%d\n", op_name, p++->op);
	break;

    case _BUILTIN:
    case _PRINT:
	fprintf(fp, "%s\n", op_name);
	++p;
	break;

    case _JMP:
    case _JNZ:
    case _JZ:
    case _LJZ:
    case _LJNZ:
	fprintf(fp, jfmt, op_name,
		(strlen(op_name) < 8) ? tab2 : (tab2 + 1),
		(p - start) + p->op);
	p++;
	break;

    case SET_ALOOP:
	fprintf(fp, "%s\t%03ld\n", op_name, (long) (p + p->op - start));
	p++;
	break;

    case ALOOP:
	fprintf(fp, "%s\t%03ld\n", op_name, (long) (p - start + p->op));
	p++;
	break;

    case A_CAT:
	fprintf(fp, "%s\t%d\n", op_name, p++->op);
	break;

    case _CALL:
	fprintf(fp, "%s\t%s\t%d\n", op_name, ((FBLOCK *) p->ptr)->name, p[1].op);
	p += 2;
	break;

    case _RANGE:
	fprintf(fp, "%s\t%03ld %03ld %03ld\n", op_name,
	/* label for pat2, action, follow */
		(long) (p - start + p[1].op),
		(long) (p - start + p[2].op),
		(long) (p - start + p[3].op));
	p += 4;
	break;

    default:
	fprintf(fp, "%s\n", op_name);
	break;
    }
    return p;
}

void
da(INST * p, FILE *fp)
{
    INST *base = p;

    TRACE(("dump-asm %s\n",
	   ((p == begin_start)
	    ? "BEGIN"
	    : ((p == end_start)
	       ? "END"
	       : ((p == main_start)
		  ? "MAIN"
		  : inst2fblock(p)->name)))));

    while (p->op != _HALT) {
	p = da_this(p, base, fp);
    }
    fflush(fp);
}
/* *INDENT-OFF* */
static const struct
{
   PF_CP action ;
   const char name[10] ;
}
special_cases[] =
{
    { bi_split,    "split" },
    { bi_length,   "length" },
    { bi_alength,  "alength" },
    { bi_match,    "match" },
    { bi_getline,  "getline" },
    { bi_sub,      "sub" },
    { bi_gsub,     "gsub" },
    { (PF_CP) 0,   "" }
} ;
/* *INDENT-ON* */

static const char *
find_bi_name(PF_CP p)
{
    const BI_REC *q;
    int i;

    for (q = bi_funct; q->name; q++) {
	if (q->fp == p) {
	    /* found */
	    return q->name;
	}
    }
    /* next check some special cases */
    for (i = 0; special_cases[i].action; i++) {
	if (special_cases[i].action == p) {
	    return special_cases[i].name;
	}
    }

    return "unknown builtin";
}

void
add_to_fdump_list(FBLOCK * fbp)
{
    DUMP_FUNCS *p = ZMALLOC(DUMP_FUNCS);
    p->data = fbp;
    p->link = fdump_list;
    fdump_list = p;
}

void
dump_funcs(void)
{
    DUMP_FUNCS *p;

    TRACE(("dump_funcs\n"));

    while ((p = fdump_list) != NULL) {
	fprintf(stdout, "function %s\n", p->data->name);
	da(p->data->code, stdout);
	fdump_list = p->link;
	ZFREE(p);
    }
}

#ifdef LOCAL_REGEXP
static void
add_to_regex_list(PTR re)
{
    DUMP_REGEX *p = ZMALLOC(DUMP_REGEX);
    p->data = re;
    p->link = regex_list;
    regex_list = p;
}

void
dump_regex(void)
{
    register DUMP_REGEX *p, *q = regex_list;

    while (q) {
	p = q;
	q = p->link;
	fprintf(stdout, "# regex %p\n", p->data);
	REmprint(cast_to_re(p->data), stdout);
	ZFREE(p);
    }
}
#else /* using system regex */
void
dump_regex(void)
{
}
#endif /* LOCAL_REGEXP */
#if OPT_TRACE
/* *INDENT-OFF* */
static const OP_NAME type_names[] =
{
    {C_NOINIT,    "noinit"},
    {C_DOUBLE,    "double"},
    {C_STRING,    "string"},
    {C_STRNUM,    "strnum"},
    {C_MBSTRN,    "mbstrn"},
    {C_RE,        "re"},
    {C_SPACE,     "space"},
    {C_SNULL,     "snull"},
    {C_REPL,      "repl"},
    {C_REPLV,     "replv"}
};
/* *INDENT-ON* */

const char *
da_type_name(const CELL *cdp)
{
    int n;
    const char *result = "?";

    for (n = 0; n < (int) TABLESIZE(type_names); ++n) {
	if (cdp->type == (int) type_names[n].op) {
	    result = type_names[n].name;
	    break;
	}
    }
    return result;
}
#endif /* OPT_TRACE */
/* *INDENT-OFF* */
static const OP_NAME other_codes[] = {
    { AE_PUSHA,   "ae_pusha" },
    { AE_PUSHI,   "ae_pushi" },
    { ALOOP,      "aloop" },
    { A_CAT,      "a_cat" },
    { A_PUSHA,    "a_pusha" },
    { F_PUSHA,    "f_pusha" },
    { F_PUSHI,    "f_pushi" },
    { LAE_PUSHA,  "lae_pusha" },
    { LAE_PUSHI,  "lae_pushi" },
    { LA_PUSHA,   "la_pusha" },
    { L_PUSHA,    "l_pusha" },
    { L_PUSHI,    "l_pushi" },
    { SET_ALOOP,  "set_al" },
    { _CALL,      "call" },
    { _CALLX,     "callx" },
    { _JMP,       "jmp" },
    { _JNZ,       "jnz" },
    { _JZ,        "jz" },
    { _LJNZ,      "ljnz" },
    { _LJZ,       "ljz" },
    { _MATCH0,    "match0" },
    { _MATCH1,    "match1" },
    { _PUSHA,     "pusha" },
    { _PUSHC,     "pushc" },
    { _PUSHD,     "pushd" },
    { _PUSHI,     "pushi" },
    { _PUSHINT,   "pushint" },
    { _PUSHS,     "pushs" },
    { _RANGE,     "range" },
    { _LENGTH,    "defer_len" },
    { A_LENGTH,   "defer_alen" },
    { _HALT,      "" }
};
/* *INDENT-ON* */

const char *
da_op_name(const INST * cdp)
{
    int n;
    const char *result = 0;

    if (cdp->op == _BUILTIN) {
	result = find_bi_name((PF_CP) cdp[1].ptr);
    } else if (cdp->op == _PRINT) {
	result = ((PF_CP) cdp->ptr == bi_printf
		  ? "printf"
		  : "print");
    } else if (cdp->op == _HALT) {
	result = "halt";
    } else {
	for (n = 0; simple_code[n].op != _HALT; ++n) {
	    if (simple_code[n].op == cdp->op) {
		result = simple_code[n].name;
		break;
	    }
	}
	if (result == 0) {
	    for (n = 0; other_codes[n].op != _HALT; ++n) {
		if (other_codes[n].op == cdp->op) {
		    result = other_codes[n].name;
		    break;
		}
	    }
	}
	if (result == 0) {
	    result = "bad instruction";
	}
    }
    return result;
}
