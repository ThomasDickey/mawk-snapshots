/********************************************
fcall.c
copyright 2009-2023,2024, Thomas E. Dickey
copyright 1991-1993,1995, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: fcall.c,v 1.24 2024/12/14 12:57:40 tom Exp $
 */

#define Visible_ARRAY
#define Visible_CA_REC
#define Visible_CELL
#define Visible_CODEBLOCK
#define Visible_FCALL_REC
#define Visible_FBLOCK
#define Visible_SYMTAB

#include <mawk.h>
#include <symtype.h>
#include <code.h>

/* This file has functions involved with type checking of
   function calls
*/

static void relocate_arglist(CA_REC *, int, unsigned, int);

static int check_progress;
 /* flag that indicates call_arg_check() was able to type
    check some call arguments */

#if OPT_TRACE
static void
trace_arg_list(CA_REC * arg_list)
{
    CA_REC *item;
    int count = 0;
    TRACE(("trace_arg_list\n"));
    while ((item = arg_list) != NULL) {
	arg_list = item->link;
	TRACE(("...arg %d is %s\n", item->arg_num + 1, type_to_str(item->type)));
	++count;
    }
}
#else
#define trace_arg_list(arg_list)	/* nothing */
#endif

#if OPT_TRACE
/*
 * FIXME: pass in the maximum offset, but keep track of jumps forward (and
 * range) which extend beyond the last halt/stop/ret/ret0, to give a more
 * precise value for the code size.
 */
static int
inst_len(INST * p)
{
    int result = 0;
    while (p != NULL && p->op != _HALT) {
	++result;
	switch ((MAWK_OPCODES) (p++->op)) {
	case _HALT:
	case _STOP:
	case FE_PUSHA:
	case FE_PUSHI:
	case A_TEST:
	case A_DEL:
	case DEL_A:
	case POP_AL:
	case _POP:
	case _ADD:
	case _SUB:
	case _MUL:
	case _DIV:
	case _MOD:
	case _POW:
	case _NOT:
	case _UMINUS:
	case _UPLUS:
	case _TEST:
	case _CAT:
	case _ASSIGN:
	case _ADD_ASG:
	case _SUB_ASG:
	case _MUL_ASG:
	case _DIV_ASG:
	case _MOD_ASG:
	case _POW_ASG:
	case NF_PUSHI:
	case F_ASSIGN:
	case F_ADD_ASG:
	case F_SUB_ASG:
	case F_MUL_ASG:
	case F_DIV_ASG:
	case F_MOD_ASG:
	case F_POW_ASG:
	case _POST_INC:
	case _POST_DEC:
	case _PRE_INC:
	case _PRE_DEC:
	case F_POST_INC:
	case F_POST_DEC:
	case F_PRE_INC:
	case F_PRE_DEC:
	case _EQ:
	case _NEQ:
	case _LT:
	case _LTE:
	case _GT:
	case _GTE:
	case _MATCH2:
	case _EXIT:
	case _EXIT0:
	case _NEXT:
	case _NEXTFILE:
	case _RET:
	case _RET0:
	case _OMAIN:
	case _JMAIN:
	case OL_GL:
	case OL_GL_NR:
	    /* simple_codes */
	    break;
	case L_PUSHA:
	case L_PUSHI:
	case LAE_PUSHI:
	case LAE_PUSHA:
	case LA_PUSHA:
	case F_PUSHA:
	case F_PUSHI:
	case AE_PUSHA:
	case AE_PUSHI:
	case A_PUSHA:
	case _PUSHI:
	case _PUSHA:
	case _MATCH0:
	case _MATCH1:
	case _PUSHS:
	case _PUSHD:
	case _PUSHC:
	case _PUSHINT:
	case _BUILTIN:
	case _PRINT:
	case _JMP:
	case _JNZ:
	case _JZ:
	case _LJZ:
	case _LJNZ:
	case SET_ALOOP:
	case ALOOP:
	case A_CAT:
	    ++result;
	    break;
	case A_LENGTH:
	case _LENGTH:
	    ++result;
	    break;
	case _CALLX:
	case _CALL:
	    result += 2;
	    break;
	case _RANGE:
	    result += 4;
	    break;
	}
    }
    return result;
}
#endif /* OPT_TRACE */

/* type checks a list of call arguments,
   returns a list of arguments whose type is still unknown
*/
static CA_REC *
call_arg_check(FBLOCK * callee,
	       CA_REC * entry_list,
	       INST * start)
{
    register CA_REC *q;
    CA_REC *exit_list = (CA_REC *) 0;

    TRACE(("call_arg_check\n"));

    check_progress = 0;

    /* loop :
       take q off entry_list
       test it
       if OK  zfree(q)  else put on exit_list  */
    while ((q = entry_list)) {
	entry_list = q->link;

	TRACE(("...arg %d is %s\n", q->arg_num + 1, type_to_str(q->type)));
	if (q->type == ST_NONE) {
	    /* try to infer the type */
	    /* it might now be in symbol table */
	    if (q->sym_p->type == ST_VAR) {
		TRACE(("...use CA_EXPR\n"));
		/* set type and patch */
		q->type = CA_EXPR;
		start[q->call_offset + 1].ptr = (PTR) q->sym_p->stval.cp;
	    } else if (q->sym_p->type == ST_ARRAY) {
		TRACE(("...use CA_ARRAY\n"));
		q->type = CA_ARRAY;
		start[q->call_offset].op = A_PUSHA;
		start[q->call_offset + 1].ptr = (PTR) q->sym_p->stval.array;
	    } else {		/* try to infer from callee */
		TRACE(("...infer?\n"));
		switch (callee->typev[q->arg_num]) {
		case ST_LOCAL_VAR:
		    q->type = CA_EXPR;
		    q->sym_p->type = ST_VAR;
		    q->sym_p->stval.cp = ZMALLOC(CELL);
		    q->sym_p->stval.cp->type = C_NOINIT;
		    start[q->call_offset + 1].ptr =
			(PTR) q->sym_p->stval.cp;
		    break;

		case ST_LOCAL_ARRAY:
		    q->type = CA_ARRAY;
		    q->sym_p->type = ST_ARRAY;
		    q->sym_p->stval.array = new_ARRAY();
		    start[q->call_offset].op = A_PUSHA;
		    start[q->call_offset + 1].ptr =
			(PTR) q->sym_p->stval.array;
		    break;
		}
	    }
	} else if (q->type == ST_LOCAL_NONE) {
	    TRACE(("...infer2?\n"));
	    /* try to infer the type */
	    if (*q->type_p == ST_LOCAL_VAR) {
		/* set type , don't need to patch */
		q->type = CA_EXPR;
	    } else if (*q->type_p == ST_LOCAL_ARRAY) {
		q->type = CA_ARRAY;
		start[q->call_offset].op = LA_PUSHA;
		/* offset+1 op is OK */
	    } else {		/* try to infer from callee */
		switch (callee->typev[q->arg_num]) {
		case ST_LOCAL_VAR:
		    q->type = CA_EXPR;
		    *q->type_p = ST_LOCAL_VAR;
		    /* do not need to patch */
		    break;

		case ST_LOCAL_ARRAY:
		    q->type = CA_ARRAY;
		    *q->type_p = ST_LOCAL_ARRAY;
		    start[q->call_offset].op = LA_PUSHA;
		    break;
		}
	    }
	}

	/* if we still do not know the type put on the new list
	   else type check */
	if (q->type == ST_NONE || q->type == ST_LOCAL_NONE) {
	    q->link = exit_list;
	    exit_list = q;
	} else {		/* type known */
	    if (callee->typev[q->arg_num] == ST_LOCAL_NONE) {
		callee->typev[q->arg_num] = q->type;
	    } else if (q->type != callee->typev[q->arg_num]) {
#if OPT_CALLX
		TRACE(("OOPS: arg %d (code %p, size %ld), actual %s vs %s\n",
		       q->arg_num + 1,
		       callee->code,
		       callee->size,
		       type_to_str(q->type),
		       type_to_str(callee->typev[q->arg_num])));
		callee->typev[q->arg_num] = q->type;
		callee->defer = 1;
#else
		token_lineno = q->call_lineno;
		compile_error("type error in arg(%d) in call to %s (actual %s vs %s)",
			      q->arg_num + 1, callee->name,
			      type_to_str(q->type),
			      type_to_str(callee->typev[q->arg_num]));
#endif
	    }

	    ZFREE(q);
	    check_progress = 1;
	}
	TRACE(("%s: code %p size %ld.%ld:%d\n",
	       callee->name,
	       (void *) callee->code,
	       callee->size / sizeof(INST),
	       callee->size % sizeof(INST),
	       inst_len(callee->code)));
    }				/* while */

    return exit_list;
}

static int
arg_cnt_ok(FBLOCK * fbp,
	   CA_REC * q)
{
    if ((int) q->arg_num >= (int) fbp->nargs) {
	token_lineno = q->call_lineno;
	compile_error("too many arguments in call to %s", fbp->name);
	return 0;
    } else
	return 1;
}

FCALL_REC *resolve_list;
 /* function calls whose arg types need checking
    are stored on this list */

/* on first pass thru the resolve list
   we check :
      if forward referenced functions were really defined
      if right number of arguments
   and compute call_start which is now known
*/

static FCALL_REC *
first_pass(FCALL_REC * p)
{
    FCALL_REC dummy;
    register FCALL_REC *q = &dummy;	/* trails p */

    q->link = p;
    while (p) {
	if (!p->callee->code) {
	    /* callee never defined */
	    compile_error("function %s never defined", p->callee->name);
	    /* delete p from list */
	    q->link = p->link;
	    /* don't worry about freeing memory, we'll exit soon */
	}
	/* note p->arg_list starts with last argument */
	else if (!p->arg_list /* nothing to do */  ||
		 (!p->arg_cnt_checked &&
		  !arg_cnt_ok(p->callee, p->arg_list))) {
	    q->link = p->link;	/* delete p */
	    /* the ! arg_list case is not an error so free memory */
	    ZFREE(p);
	} else {
	    /* keep p and set call_start */
	    q = p;
	    switch (p->call_scope) {
	    case SCOPE_MAIN:
		p->call_start = main_start;
		break;

	    case SCOPE_BEGIN:
		p->call_start = begin_start;
		break;

	    case SCOPE_END:
		p->call_start = end_start;
		break;

	    case SCOPE_FUNCT:
		p->call_start = p->call->code;
		break;
	    }
	}
	p = q->link;
    }
    return dummy.link;
}

/* continuously walk the resolve_list making type deductions
   until this list goes empty or no more progress can be made
   (An example where no more progress can be made is at end of file
*/

void
resolve_fcalls(void)
{
    register FCALL_REC *p, *old_list, *new_list;
    int progress;		/* a flag */

    TRACE(("resolve_fcalls\n"));

    old_list = first_pass(resolve_list);
    new_list = (FCALL_REC *) 0;
    progress = 0;

    while (1) {
	if (!old_list) {
	    /* flop the lists */
	    old_list = new_list;
	    if (!old_list	/* nothing left */
		|| !progress /* can't do any more */ )
		return;

	    new_list = (FCALL_REC *) 0;
	    progress = 0;
	}

	p = old_list;
	old_list = p->link;

	TRACE(("%s@%d ", __FILE__, __LINE__));
	if ((p->arg_list = call_arg_check(p->callee, p->arg_list,
					  p->call_start))) {
	    /* still have work to do , put on new_list   */
	    progress |= check_progress;
	    p->link = new_list;
	    new_list = p;
	} else {
	    /* done with p */
	    progress = 1;
	    ZFREE(p);
	}
    }
}

/* the parser has just reduced a function call ;
   the info needed to type check is passed in.	If type checking
   can not be done yet (most common reason -- function referenced
   but not defined), a node is added to the resolve list.
*/
void
check_fcall(
	       FBLOCK * callee,
	       int call_scope,
	       int move_level,
	       FBLOCK * call,
	       CA_REC * arg_list)
{
    FCALL_REC *p;

    TRACE(("check_fcall(%s)\n", callee->name));
    if (!callee->code) {
	TRACE(("...forward reference\n"));
	/* forward reference to a function to be defined later */
	p = ZMALLOC(FCALL_REC);
	p->callee = callee;
	p->call_scope = (short) call_scope;
	p->move_level = (short) move_level;
	p->call = call;
	p->arg_list = arg_list;
	p->arg_cnt_checked = 0;
	trace_arg_list(arg_list);
	/* add to resolve list */
	p->link = resolve_list;
	resolve_list = p;
    } else if (arg_list && arg_cnt_ok(callee, arg_list)) {
	/* usually arg_list disappears here and all is well
	   otherwise add to resolve list */

	TRACE(("%s@%d ", __FILE__, __LINE__));
	if ((arg_list = call_arg_check(callee, arg_list,
				       code_base))) {
	    p = ZMALLOC(FCALL_REC);
	    p->callee = callee;
	    p->call_scope = (short) call_scope;
	    p->move_level = (short) move_level;
	    p->call = call;
	    p->arg_list = arg_list;
	    p->arg_cnt_checked = 1;
	    /* add to resolve list */
	    p->link = resolve_list;
	    resolve_list = p;
	}
    }
}

/* code_pop() has just moved some code.	 If this code contains
   a function call, it might need to be relocated on the
   resolve list too.  This function does it.

   delta == relocation distance
*/

void
relocate_resolve_list(
			 int scope,
			 int move_level,
			 const FBLOCK * fbp,
			 int orig_offset,
			 unsigned len,
			 int delta)
{
    FCALL_REC *p = resolve_list;

    while (p) {
	if (scope == p->call_scope && move_level == p->move_level &&
	    (scope == SCOPE_FUNCT ? fbp == p->call : 1)) {
	    relocate_arglist(p->arg_list, orig_offset,
			     len, delta);
	}
	p = p->link;
    }
}

static void
relocate_arglist(
		    CA_REC * arg_list,
		    int offset,
		    unsigned len,
		    int delta)
{
    register CA_REC *p;

    if (!arg_list)
	return;

    p = arg_list;
    /* all nodes must be relocated or none, so test the
       first one */

    /* Note: call_offset is always set even for args that don't need to
       be patched so that this check works. */
    if (p->call_offset < offset ||
	(unsigned) p->call_offset >= (unsigned) offset + len)
	return;

    /* relocate the whole list */
    do {
	p->call_offset += delta;
	p = p->link;
    }
    while (p);
}

/*  example where typing cannot progress

{ f(z) }

function f(x) { print NR }

# this is legal, does something useful, but absurdly written
# We have to design so this works
*/
