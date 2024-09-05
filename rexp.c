/********************************************
rexp.c
copyright 2008-2023,2024, Thomas E. Dickey
copyright 1991-1993,1996, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: rexp.c,v 1.51 2024/09/04 23:02:39 tom Exp $
 */

/*  op precedence  parser for regular expressions  */

#include <rexp.h>
#include <regexp.h>

/*  DATA   */
int REerrno;
const char *const REerrlist[] =
{(char *) 0,
 /* ERR_1  */ "missing '('",
 /* ERR_2  */ "missing ')'",
 /* ERR_3  */ "bad class -- [], [^] or [",
 /* ERR_4  */ "missing operand",
 /* ERR_5  */ "resource exhaustion -- regular expression too large",
 /* ERR_6  */ "syntax error ^* or ^+",
 /* ERR_7  */ "bad interval expression",
 /* ERR_8  */ ""
};
/* ERR_5 is very unlikely to occur */

/* This table drives the operator precedence parser */
/* *INDENT-OFF* */
#ifdef NO_INTERVAL_EXPR
static  short  table[8][8] = {
/*        0      |      CAT     *      +      ?      (      )   */
/* 0 */  {0,     OP_L,  OP_L,   OP_L,  OP_L,  OP_L,  OP_L,  ERR_1},
/* | */  {OP_G,  OP_G,  OP_L,   OP_L,  OP_L,  OP_L,  OP_L,  OP_G},
/* CAT*/ {OP_G,  OP_G,  OP_G,   OP_L,  OP_L,  OP_L,  OP_L,  OP_G},
/* * */  {OP_G,  OP_G,  OP_G,   OP_G,  OP_G,  OP_G,  ERR_7, OP_G},
/* + */  {OP_G,  OP_G,  OP_G,   OP_G,  OP_G,  OP_G,  ERR_7, OP_G},
/* ? */  {OP_G,  OP_G,  OP_G,   OP_G,  OP_G,  OP_G,  ERR_7, OP_G},
/* ( */  {ERR_2, OP_L,  OP_L,   OP_L,  OP_L,  OP_L,  OP_L,  OP_EQ},
/* ) */  {OP_G , OP_G,  OP_G,   OP_G,  OP_G,  OP_G,  ERR_7, OP_G}};
#else
static  short  table[10][10]  =  {
/*       0       |      CAT    *      +      ?      (      )      {      }  */
/* 0 */  {0,     OP_L,  OP_L,  OP_L,  OP_L,  OP_L,  OP_L,  ERR_1, ERR_7, OP_L},
/* | */  {OP_G,  OP_G,  OP_L,  OP_L,  OP_L,  OP_L,  OP_L,  OP_G,  OP_G,  OP_G},
/* CAT*/ {OP_G,  OP_G,  OP_G,  OP_L,  OP_L,  OP_L,  OP_L,  OP_G,  OP_L,  OP_G},
/* * */  {OP_G,  OP_G,  OP_G,  OP_G,  OP_G,  OP_G,  ERR_7, OP_G,  OP_G,  OP_G},
/* + */  {OP_G,  OP_G,  OP_G,  OP_G,  OP_G,  OP_G,  ERR_7, OP_G,  OP_G,  OP_G},
/* ? */  {OP_G,  OP_G,  OP_G,  OP_G,  OP_G,  OP_G,  ERR_7, OP_G,  OP_G,  OP_G},
/* ( */  {ERR_2, OP_L,  OP_L,  OP_L,  OP_L,  OP_L,  OP_L,  OP_EQ, OP_G,  OP_G},
/* ) */  {OP_G , OP_G,  OP_G,  OP_G,  OP_G,  OP_G,  ERR_7, OP_G,  ERR_7, OP_G},
/* { */  {OP_G,  OP_G,  OP_G,  OP_G,  OP_G,  OP_G,  ERR_7, OP_G,  OP_G,  OP_EQ},
/* } */  {OP_G , OP_G,  OP_G,  OP_G,  OP_G,  OP_G,  ERR_7, OP_G,  ERR_7, OP_G}   };
#endif
/* *INDENT-ON* */

#define	 STACKSZ   64

static const char *REs_type(STATE * p);

static jmp_buf err_buf;		/*  used to trap on error */

#if OPT_TRACE > 0
static const char *
token_name(int token)
{
    const char *result;
#define CASE(name) case name: result = #name; break
    switch (token) {
	CASE(T_NONE);
	CASE(T_OR);
	CASE(T_CAT);
	CASE(T_STAR);
	CASE(T_PLUS);
	CASE(T_Q);
	CASE(T_LP);
	CASE(T_RP);
	CASE(T_START);
	CASE(T_END);
	CASE(T_ANY);
	CASE(T_CLASS);
	CASE(T_SLASH);
	CASE(T_CHAR);
	CASE(T_STR);
#ifndef NO_INTERVAL_EXPR
	CASE(T_LB);
	CASE(T_RB);
#endif
	CASE(T_U);
    default:
	result = "?";
	break;
    }
#undef CASE
    return result;
}
#endif

void
RE_error_trap(int x)
{
    TRACE(("RE_error_trap(%d)\n", x));
    REerrno = x;
    longjmp(err_buf, 1);
}

typedef struct {
    int token;
    int prec;
} OPS;

/* duplicate_m() relies upon copying machines whose size is 1, i.e., atoms */
#define BigMachine(mp) (((mp)->stop - (mp)->start) > 1)

STATE *
REcompile(char *re, size_t len)
{
#define m_stack(n) &m_array[(n) + 1]
    MACHINE m_array[1 + STACKSZ];
    OPS op_stack[STACKSZ];
    register MACHINE *m_ptr;
    register OPS *op_ptr;
    register int t;

    TRACE(("REcompile %.*s\n", (int) len, re));
    /* do this first because it also checks if we have a
       run time stack */
    RE_lex_init(re, len);

    if (len == 0) {
	STATE *p = (STATE *) RE_malloc(sizeof(STATE));
	p->s_type = M_ACCEPT;
	return p;
    }

    if (setjmp(err_buf))
	return NULL;
    /* we used to try to recover memory left on machine stack ;
       but now m_ptr is in a register so it won't be right unless
       we force it out of a register which isn't worth the trouble */

    /* initialize the stacks  */
    m_ptr = m_array;
    op_ptr = op_stack;
    op_ptr->token = 0;

    t = RE_lex(m_stack(0));
    memset(m_ptr, 0, sizeof(*m_ptr));

    while (1) {
	TRACE(("RE_lex token %s\n", token_name(t)));
	switch (t) {
	case T_STR:
	case T_ANY:
	case T_U:
	case T_START:
	case T_END:
	case T_CLASS:
	    m_ptr++;
	    break;

#ifndef NO_INTERVAL_EXPR
	case T_RB:
	    if (!repetitions_flag) {
		goto default_case;
	    }
	    /* interval expression {n,m}
	     * eg, 
	     *   convert m{3} to mmm
	     *   convert m{3,} to mmm* (with a limit of MAX_INT)
	     *   convert m{3,10} to mmm* with a limit of 10
	     */
	    TRACE(("interval {%ld,%ld}\n", (long) intrvalmin, (long) intrvalmax));
	    if ((m_ptr - m_array) < STACKSZ)
		memset(m_ptr + 1, 0, sizeof(*m_ptr));
	    if (intrvalmin == 0) {	/* zero or more */
		switch (intrvalmax) {
		case 0:
		    /* user stupidity: m{0} or m{0,0} 
		     * don't add this re token
		     */
		    if (m_ptr == m_array) {
			t = RE_lex(++m_ptr);
			if (t != T_NONE) {
			    continue;
			} else {
			    m_array[1] = RE_any();	/* FIXME: RE_none? */
			    m_ptr = m_stack(0);
			}
		    } else if (op_ptr != op_stack) {
			/* no previous re */
			RE_free(m_ptr->start);
			m_ptr--;
			switch (op_ptr->token) {
			case T_RP:
			    while (op_ptr != op_stack) {
				--op_ptr;
				if (op_ptr->token == T_LP) {
				    if (op_ptr == op_stack) {
					op_ptr->token = T_NONE;
				    }
				    break;
				}
			    }
			    op_ptr = op_stack + 1;
			    break;
			case T_LP:
			    break;
			default:
			    op_ptr--;
			    break;
			}
		    } else if (*re_exp == '\0') {
			/* this was the only re expr
			   so leave one M_ACCEPT as the machine */
			m_ptr->start->s_type = M_ACCEPT;
		    } else {
			RE_free(m_ptr->start);
			m_ptr--;
		    }
		    TRACE(("RE_lex token %s\n",
			   "of zero interval is ignored!"));
		    break;
		case 1:
		    RE_01(m_ptr);	/* m{0,1} which is m? */
		    TRACE(("RE_lex token %s\n", token_name(T_Q)));
		    break;
		default:
		    RE_close_limit(m_ptr, intrvalmin, intrvalmax);
		    TRACE(("RE_lex token %s\n", token_name(T_Q)));
		}
	    } else if (BigMachine(m_ptr)) {
		RE_poscl_limit(m_ptr, intrvalmin, intrvalmax);
#ifdef NO_RI_LOOP_UNROLL
	    } else if (intrvalmin >= 1) {	/* one or more */
		RE_poscl_limit(m_ptr, intrvalmin, intrvalmax);
		TRACE(("RE_lex token %s\n", token_name(T_PLUS)));
#else
	    } else if (intrvalmin == 1) {	/* one or more */
		RE_poscl_limit(m_ptr, intrvalmin, intrvalmax);
		TRACE(("RE_lex token %s\n", token_name(T_PLUS)));
#endif
	    } else if (m_ptr->start != 0) {	/* n or more */
		register Int i;
		/* copy 2 copies of m_ptr, use 2nd copy to replace
		   the first copy that gets swallowed by concat */
		MACHINE *result_mp = m_ptr;
		MACHINE *concat_mp = (m_ptr + 1);
		MACHINE *new_mp = (m_ptr + 2);
		TRACE(("calling duplicate_m result_mp %ld -> concat_mp %ld\n",
		       result_mp - m_array,
		       concat_mp - m_array));
		duplicate_m(concat_mp, result_mp);
		TRACE(("calling duplicate_m result_mp %ld -> new_mp %ld\n",
		       result_mp - m_array,
		       new_mp - m_array));
		duplicate_m(new_mp, result_mp);
		for (i = 2; i <= intrvalmin; i++) {
		    RE_cat(result_mp, concat_mp);
		    duplicate_m(concat_mp, new_mp);
		}
		/* don't need 2nd copy in new_mp */
		RE_free(new_mp->start);
	    }
	    break;
#endif /* ! NO_INTERVAL_EXPR */

	case T_NONE:		/*  end of reg expr   */
	    if (op_ptr->token == 0) {
		/*  done   */
		if (m_ptr == m_stack(0)) {
		    return m_ptr->start;
		} else {
		    /* machines still on the stack  */
		    RE_panic("values still on machine stack for %s", re);
		}
	    }
	    /* FALLTHRU */

	    /*  otherwise, default is operator case  */

	default:
#ifndef NO_INTERVAL_EXPR
	  default_case:
#endif

	    if ((op_ptr->prec = table[op_ptr->token][t]) == OP_G) {
		do {		/* op_pop   */

		    if (op_ptr->token <= T_CAT) {	/*binary op */
			if (m_ptr == m_stack(0)
			    && op_ptr->token == T_CAT) {
			    TRACE(("...ignoring empty T_CAT\n"));
			    op_ptr--;
			    continue;
			}
			m_ptr--;
		    }
		    /* if not enough values on machine stack
		       then we have a missing operand */
		    if (m_ptr == m_array)
			RE_error_trap(-ERR_4);

		    switch (op_ptr->token) {
		    case T_CAT:
			RE_cat(m_ptr, m_ptr + 1);
			break;

		    case T_OR:
			RE_or(m_ptr, m_ptr + 1);
			break;

		    case T_STAR:
			RE_close(m_ptr);
			break;

		    case T_PLUS:
			RE_poscl(m_ptr);
			break;

		    case T_Q:
			RE_01(m_ptr);
			break;

		    default:
			/*nothing on ( or ) */
			break;
		    }

		    op_ptr--;
		}
		while (op_ptr->prec != OP_L);

		continue;	/* back thru switch at top */
	    }

	    if (op_ptr->prec < 0) {
		if (op_ptr->prec == ERR_7)
		    RE_panic("parser returns ERR_7");
		else
		    RE_error_trap(-op_ptr->prec);
	    }

	    if (++op_ptr == op_stack + STACKSZ) {
		/* stack overflow */
		RE_error_trap(-ERR_5);
	    }

	    op_ptr->token = t;
	}			/* end of switch */

	if (m_ptr == m_stack(STACKSZ - 1)) {
	    /*overflow */
	    RE_error_trap(-ERR_5);
	}

	t = RE_lex(m_ptr + 1);
    }
}

#ifdef NO_LEAKS
void
REdestroy(STATE * ptr)
{
    int done = 0;
    int n = 0;
    STATE *q = ptr;

    TRACE(("REdestroy %p\n", (void *) ptr));
    while (!done) {
	TRACE(("...destroy[%d] %p type %s\n", n, (void *) q, REs_type(q)));
	switch (q->s_type) {
	case M_ACCEPT:
	    done = 1;
	    break;
	case M_STR:
	    RE_free(q->s_data.str);
	    break;
	default:
	    if (q->s_type < 0 || q->s_type > END_ON)
		done = -1;
	    break;
	}
	++q;
	++n;
    }
    RE_free(ptr);
}
#endif /* NO_LEAKS */

/* getting here means a logic flaw or unforeseen case */
void
RE_panic(const char *format, ...)
{
    const char *where = "REcompile() - panic:  ";
    va_list args;

    fflush(stdout);

#if OPT_TRACE > 0
    va_start(args, format);
    Trace("?? %s", where);
    TraceVA(format, args);
    Trace("\n");
    va_end(args);
#endif

    fputs(where, stderr);

    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    fprintf(stderr, "\n");

    mawk_exit(100);
}

/* getting regexp error message */
const char *
REerror(void)
{
    return REerrlist[REerrno];
}

#ifndef NO_INTERVAL_EXPR
/* duplicate a machine, oldmp into newmp */
void
duplicate_m(MACHINE * newmp, MACHINE * oldmp)
{
    register STATE *p;
    TRACE(("duplicate_m %p -> %p\n", oldmp, newmp));
    TRACE(("...start %p\n", oldmp->start));
    TRACE(("...stop  %p\n", oldmp->stop));
    p = (STATE *) RE_malloc(2 * STATESZ);
    RE_copy_states(p, oldmp->start, 2);
    newmp->start = (STATE *) p;
    newmp->stop = (STATE *) (p + 1);
}
#endif /* NO_INTERVAL_EXPR */
