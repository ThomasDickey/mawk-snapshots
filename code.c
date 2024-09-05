/********************************************
code.c
copyright 2009-2023,2024, Thomas E. Dickey
copyright 1991-1994,1995, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: code.c,v 1.48 2024/09/05 17:44:48 tom Exp $
 */

#define Visible_CELL
#define Visible_CODEBLOCK
#define Visible_FBLOCK
#define Visible_STRING

#include <mawk.h>
#include <code.h>
#include <init.h>
#include <jmp.h>
#include <field.h>

#ifdef NO_LEAKS
#include <scan.h>
#endif

static CODEBLOCK *new_code(void);

CODEBLOCK active_code;

CODEBLOCK *main_code_p, *begin_code_p, *end_code_p;

INST *begin_start, *main_start, *end_start;
size_t begin_size, main_size, end_size;

INST *execution_start = 0;

/* grow the active code */
void
code_grow(void)
{
    size_t oldsize = (size_t) (code_limit - code_base);
    size_t newsize = PAGESZ + oldsize;
    size_t delta = (size_t) (code_ptr - code_base);

    if (code_ptr > code_limit)
	bozo("CODEWARN is too small");

    code_base = (INST *)
	zrealloc(code_base, INST_BYTES(oldsize),
		 INST_BYTES(newsize));
    code_limit = code_base + newsize;
    code_warn = code_limit - CODEWARN;
    code_ptr = code_base + delta;
}

/* shrinks executable code that's done to its final size */
INST *
code_shrink(CODEBLOCK * p, size_t *sizep)
{

    size_t oldsize = INST_BYTES(p->limit - p->base);
    size_t newsize = INST_BYTES(p->ptr - p->base);
    INST *retval;

    *sizep = newsize;

    retval = (INST *) zrealloc(p->base, oldsize, newsize);
    TRACE(("code_shrink old %p %lu, new %p %lu\n",
	   (void *) p->base,
	   (unsigned long) oldsize,
	   (void *) retval,
	   (unsigned long) newsize));
    ZFREE(p);
    return retval;
}

/* code an op and a pointer in the active_code */
void
xcode2(int op, PTR ptr)
{
    register INST *p = code_ptr + 2;

    if (p >= code_warn) {
	code_grow();
	p = code_ptr + 2;
    }

    p[-2].op = op;
    p[-1].ptr = ptr;
    code_ptr = p;
}

/* code an op and a function-pointer in the active_code */
void
xfunc2(int op, PF_CP fnc)
{
    register INST *p = code_ptr + 2;

    if (p >= code_warn) {
	code_grow();
	p = code_ptr + 2;
    }

    p[-2].op = op;
    p[-1].fnc = fnc;
    code_ptr = p;
}

/* code two ops in the active_code */
void
code2op(int x, int y)
{
    register INST *p = code_ptr + 2;

    if (p >= code_warn) {
	code_grow();
	p = code_ptr + 2;
    }

    p[-2].op = x;
    p[-1].op = y;
    code_ptr = p;
}

void
code_init(void)
{
    main_code_p = new_code();

    active_code = *main_code_p;
    code1(_OMAIN);
}

/* final code relocation
   set_code() as in set concrete */
void
set_code(void)
{
    /* set the main code which is active_code */
    if (end_code_p || code_offset > 1) {
	int gl_offset = code_offset;

	if (NR_flag)
	    code2op(OL_GL_NR, _HALT);
	else
	    code2op(OL_GL, _HALT);

	*main_code_p = active_code;
	main_start = code_shrink(main_code_p, &main_size);
	next_label = main_start + gl_offset;
	execution_start = main_start;
    } else {			/* only BEGIN */
	zfree(code_base, INST_BYTES(PAGESZ));
	code_base = 0;
	ZFREE(main_code_p);
    }

    /* set the END code */
    if (end_code_p) {
	active_code = *end_code_p;
	code2op(_EXIT0, _HALT);
	*end_code_p = active_code;
	end_start = code_shrink(end_code_p, &end_size);
    }

    /* set the BEGIN code */
    if (begin_code_p) {
	active_code = *begin_code_p;
	if (main_start)
	    code2op(_JMAIN, _HALT);
	else
	    code2op(_EXIT0, _HALT);
	*begin_code_p = active_code;
	begin_start = code_shrink(begin_code_p, &begin_size);

	execution_start = begin_start;
    }

    if (!execution_start) {
	/* program had functions but no pattern-action bodies */
	execution_start = begin_start = (INST *) zmalloc(2 * sizeof(INST));
	execution_start[0].op = _EXIT0;
	execution_start[1].op = _HALT;
    }
}

void
dump_code(void)
{
    dump_funcs();		/* dumps all user functions */
    if (begin_start) {
	fprintf(stdout, "BEGIN\n");
	da(begin_start, stdout);
    }
    if (end_start) {
	fprintf(stdout, "END\n");
	da(end_start, stdout);
    }
    if (main_start) {
	fprintf(stdout, "MAIN\n");
	da(main_start, stdout);
    }
    dump_regex();		/* dumps all regular expressions */
}

static CODEBLOCK *
new_code(void)
{
    CODEBLOCK *p = ZMALLOC(CODEBLOCK);

    p->base = (INST *) zmalloc(INST_BYTES(PAGESZ));
    p->limit = p->base + PAGESZ;
    p->warn = p->limit - CODEWARN;
    p->ptr = p->base;

    return p;
}

/* moves the active_code from MAIN to a BEGIN or END */

void
be_setup(int scope)
{
    *main_code_p = active_code;

    if (scope == SCOPE_BEGIN) {
	if (!begin_code_p) {
	    TRACE(("be_setup: BEGIN\n"));
	    begin_code_p = new_code();
	} else {
	    TRACE(("be_setup: BEGIN (again)\n"));
	}
	active_code = *begin_code_p;
    } else {
	if (!end_code_p) {
	    TRACE(("be_setup: END\n"));
	    end_code_p = new_code();
	} else {
	    TRACE(("be_setup: END (again)\n"));
	}
	active_code = *end_code_p;
    }
}

#ifdef NO_LEAKS
void
free_cell_data(CELL *cp)
{
    switch (cp->type) {
    case C_RE:
	TRACE(("\t... C_RE\n"));
	re_destroy(cp->ptr);
	zfree(cp, sizeof(CELL));
	break;
    case C_REPL:
	TRACE(("\t... C_REPL\n"));
	repl_destroy(cp);
	zfree(cp, sizeof(CELL));
	break;
    case C_REPLV:
	TRACE(("\t... C_REPLV\n"));
	repl_destroy(cp);
	zfree(cp, sizeof(CELL));
	break;
    case C_MBSTRN:
    case C_STRING:
    case C_STRNUM:
	if (cp >= (field + (nf < 1 ? 1 : nf)) || (cp < field)) {
	    cell_destroy(cp);
	}
	break;
    }
}

void
free_codes(const char *tag, INST * base, size_t size)
{
    INST *cdp;
    INST *last = base + (size / sizeof(*last));
    CELL *cp;

    (void) tag;

    TRACE(("free_codes(%s) base %p, size %lu\n",
	   tag,
	   (void *) base,
	   (unsigned long) size));

    if (base != 0 && size != 0) {
	for (cdp = base; cdp < last; ++cdp) {
#ifndef NO_LEAKS
	    TRACE_INST(cdp, base);
#endif

	    switch ((MAWK_OPCODES) (cdp->op)) {
	    case AE_PUSHA:
	    case AE_PUSHI:
		++cdp;		/* skip pointer */
		cp = (CELL *) (cdp->ptr);
		if (cp != 0) {
		    free_cell_data(cp);
		}
		break;
	    case _MATCH0:
	    case _MATCH1:
		++cdp;		/* skip pointer */
		re_destroy(cdp->ptr);
		break;
	    case LAE_PUSHA:
	    case LA_PUSHA:
	    case A_CAT:
		++cdp;		/* skip value */
		break;
	    case A_PUSHA:
	    case L_PUSHA:
	    case L_PUSHI:
	    case _LENGTH:
	    case _BUILTIN:
	    case _PRINT:
	    case _PUSHA:
	    case _PUSHI:
	    case _PUSHINT:
		++cdp;		/* skip value */
		break;
	    case _PUSHD:
		++cdp;		/* skip value */
		if (cdp->ptr != &double_one && cdp->ptr != &double_zero)
		    zfree(cdp->ptr, sizeof(double));
		break;
	    case F_PUSHI:
		++cdp;		/* skip pointer */
		++cdp;		/* skip integer */
		break;
	    case _PUSHS:
		++cdp;		/* skip value */
		free_STRING((STRING *) (cdp->ptr));
		break;
	    case _RANGE:
		cdp += 4;	/* PAT1 */
		break;
	    case _CALLX:
	    case _CALL:
		cdp += 1 + cdp[2].op;
		break;
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
	    case F_PUSHA:
	    case F_SUB_ASG:
	    case NF_PUSHI:
	    case OL_GL:
	    case OL_GL_NR:
	    case POP_AL:
	    case _ADD:
	    case _ADD_ASG:
	    case _ASSIGN:
	    case _CAT:
	    case _DIV:
	    case _DIV_ASG:
	    case _EQ:
	    case _EXIT0:	/* this does free memory... */
	    case _EXIT:
	    case _GT:
	    case _GTE:
	    case _HALT:
	    case _JMAIN:
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
	    case _RET0:
	    case _RET:
	    case _STOP:
	    case _SUB:
	    case _SUB_ASG:
	    case _TEST:
	    case _UMINUS:
	    case _UPLUS:
		break;
	    case _JNZ:
	    case _JZ:
	    case _LJZ:
	    case _LJNZ:
	    case _JMP:
	    case _PUSHC:
	    case ALOOP:
	    case LAE_PUSHI:
	    case SET_ALOOP:
		++cdp;		/* cdp->op is literal param */
		break;
	    }
	}
	zfree(base, size);
    }
}

void
code_leaks(void)
{
    TRACE(("code_leaks\n"));
    if (begin_start != 0) {
	free_codes("BEGIN", begin_start, begin_size);
	begin_start = 0;
	begin_size = 0;
    }
    if (end_start != 0) {
	free_codes("END", end_start, end_size);
	end_start = 0;
	end_size = 0;
    }
    if (main_start != 0) {
	free_codes("MAIN", main_start, main_size);
	main_start = 0;
	main_size = 0;
    }
}
#endif
