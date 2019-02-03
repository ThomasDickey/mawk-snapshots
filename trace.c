/********************************************
trace.c
copyright 2012-2016,2019 Thomas E. Dickey

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: trace.c,v 1.16 2019/02/02 02:02:33 tom Exp $
 */
#include <mawk.h>
#include <repl.h>
#include <code.h>

static FILE *trace_fp;

void
Trace(const char *format, ...)
{
    va_list args;

    if (trace_fp == 0)
	trace_fp = fopen("Trace.out", "w");

    if (trace_fp == 0)
	rt_error("cannot open Trace.out");

    va_start(args, format);
    vfprintf(trace_fp, format, args);
    va_end(args);

    fflush(trace_fp);
}

void
TraceVA(const char *format, va_list args)
{
    vfprintf(trace_fp, format, args);
}

void
TraceCell(CELL *cp)
{
    TRACE(("cell %p ", (void *) cp));
    if (cp != 0) {
	switch ((MAWK_CELL_TYPES) cp->type) {
	case C_NOINIT:
	    TRACE(("is empty\n"));
	    break;
	case C_DOUBLE:
	    TRACE(("double %g\n", cp->dval));
	    break;
	case C_MBSTRN:
	case C_STRNUM:
	case C_STRING:
	    TRACE(("string "));
	    TraceString(string(cp));
	    TRACE((" (%d)\n", string(cp)->ref_cnt));
	    break;
	case C_SPACE:
	    TRACE(("split on space\n"));
	    break;
	case C_SNULL:
	    TRACE(("split on the empty string\n"));
	    break;
	case C_RE:
	    TRACE(("a regular expression at %p: %s\n", cp->ptr, re_uncompile(cp->ptr)));
	    break;
	case C_REPL:
	    TRACE(("a replacement string at %p: ", cp->ptr));
	    TraceString(string(cp));
	    TRACE(("\n"));
	    break;
	case C_REPLV:
	    TRACE(("a vector replacement, count %d at %p\n", cp->vcnt, cp->ptr));
	    break;
	case NUM_CELL_TYPES:
	    /* FALLTHRU */
	default:
	    TRACE(("unknown type %d\n", cp->type));
	    break;
	}
    } else {
	TRACE(("no cell\n"));
    }
}

void
TraceFunc(const char *name, CELL *sp)
{
    int nargs = sp->type;
    int n;

    TRACE(("** %s <-%p\n", name, (void *) sp));
    for (n = 0; n < nargs; ++n) {
	TRACE(("...arg%d: ", n));
	TraceCell(sp + n - nargs);
    }
}

void
TraceInst(INST * p, INST * base)
{
    INST *q = da_this(p, base, trace_fp);
    TRACE(("	...%ld\n", (long) (q - p)));
    if (p++ != q) {
	switch ((MAWK_OPCODES) (base->op)) {
	case AE_PUSHA:
	case AE_PUSHI:
	case A_PUSHA:
	    TRACE(("\tST_ARRAY *%p\n", p->ptr));
	    break;
	case F_PUSHA:
	    TRACE(("\tST_FIELD *%p\n", p->ptr));
	    break;
	case _BUILTIN:
	case _PRINT:
	    TRACE(("\tPF_CP *%p\n", p->ptr));
	    break;
	case _CALL:
	    TRACE(("\tFBLOCK *%p\n", p->ptr));
	    break;
	case _MATCH0:
	case _MATCH1:
	    TRACE(("\tregex *%p\n", p->ptr));
	    break;
	case _PUSHA:
	    TRACE(("\tST_VAR *%p\n", p->ptr));
	    break;
	case _PUSHC:
	case _PUSHI:
	    TRACE(("\tCELL *%p\n", p->ptr));
	    break;
	case _PUSHD:
	    TRACE(("\tdouble *%p\n", p->ptr));
	    break;
	case _PUSHS:
	    TRACE(("\tSTRING *%p\n", p->ptr));
	    break;
	case ALOOP:
	case A_CAT:
	case A_DEL:
	case A_LENGTH:
	case A_TEST:
	case DEL_A:
	case FE_PUSHA:
	case FE_PUSHI:
	case F_ADD_ASG:
	case F_ASSIGN:
	case F_DIV_ASG:
	case F_MOD_ASG:
	case F_MUL_ASG:
	case F_POST_DEC:
	case F_POST_INC:
	case F_POW_ASG:
	case F_PRE_DEC:
	case F_PRE_INC:
	case F_PUSHI:
	case F_SUB_ASG:
	case LAE_PUSHA:
	case LAE_PUSHI:
	case LA_PUSHA:
	case L_PUSHA:
	case L_PUSHI:
	case NF_PUSHI:
	case OL_GL:
	case OL_GL_NR:
	case POP_AL:
	case SET_ALOOP:
	case _ADD:
	case _ADD_ASG:
	case _ASSIGN:
	case _CAT:
	case _DIV:
	case _DIV_ASG:
	case _EQ:
	case _EXIT0:
	case _EXIT:
	case _GT:
	case _GTE:
	case _HALT:
	case _JMAIN:
	case _JMP:
	case _JNZ:
	case _JZ:
	case _LJNZ:
	case _LJZ:
	case _LT:
	case _LTE:
	case _MATCH2:
	case _MOD:
	case _MOD_ASG:
	case _MUL:
	case _MUL_ASG:
	case _NEQ:
	case _NEXT:
	case _NEXTFILE:
	case _NOT:
	case _OMAIN:
	case _POP:
	case _POST_DEC:
	case _POST_INC:
	case _POW:
	case _POW_ASG:
	case _PRE_DEC:
	case _PRE_INC:
	case _PUSHINT:
	case _RANGE:
	case _RET0:
	case _RET:
	case _STOP:
	case _SUB:
	case _SUB_ASG:
	case _TEST:
	case _UMINUS:
	case _UPLUS:
	    break;
	}
    }
}

void
TraceString2(const char *str, size_t len)
{
    size_t limit = len;
    size_t n;

    TRACE(("\""));
    if (limit) {
	for (n = 0; n < limit; ++n) {
	    UChar ch = (UChar) str[n];
	    switch (ch) {
	    case '\"':
		TRACE(("\\\""));
		break;
	    case '\\':
		TRACE(("\\\\"));
		break;
	    case '\b':
		TRACE(("\\b"));
		break;
	    case '\f':
		TRACE(("\\f"));
		break;
	    case '\n':
		TRACE(("\\n"));
		break;
	    case '\r':
		TRACE(("\\r"));
		break;
	    case '\t':
		TRACE(("\\t"));
		break;
	    default:
		if (ch < 32 || ch > 126)
		    TRACE(("\\%03o", ch));
		else
		    TRACE(("%c", ch));
		break;
	    }
	}
    }
    TRACE(("\""));
}

void
TraceString(STRING * sp)
{
    TraceString2(sp ? sp->str : "",
		 sp ? sp->len : 0);
}

#ifdef NO_LEAKS
void
trace_leaks(void)
{
    if (trace_fp != 0) {
	fclose(trace_fp);
	trace_fp = 0;
    }
}
#endif
