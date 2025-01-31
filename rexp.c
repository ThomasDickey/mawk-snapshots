/********************************************
rexp.c
copyright 2008-2024,2025, Thomas E. Dickey
copyright 1991-1993,1996, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: rexp.c,v 1.61 2025/01/31 22:33:22 tom Exp $
 */

/*  op precedence  parser for regular expressions  */

#include <rexp.h>
#include <regexp.h>

#ifndef FIXME_INTERVAL_LIMITS
#define FIXME_INTERVAL_LIMITS 0	/* =1 for pre-bugfix */
#endif

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

static jmp_buf err_buf;		/* used to trap on error */

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

#ifndef NO_INTERVAL_EXPR
#define MAX_LOOP_LEVEL 10	/* this would be very complex... */
static int used_loop_level;	/* used to flag post-processing step */

/* duplicate a machine, oldmp into newmp */
static void
duplicate_m(MACHINE * newmp, MACHINE * oldmp)
{
    register STATE *p;
    TRACE(("duplicate_m %p -> %p\n", (void *) oldmp, (void *) newmp));
    TRACE(("...start %p\n", (void *) oldmp->start));
    TRACE(("...stop  %p\n", (void *) oldmp->stop));
    p = (STATE *) RE_malloc(2 * STATESZ);
    RE_copy_states(p, oldmp->start, 2);
    newmp->start = (STATE *) p;
    newmp->stop = (STATE *) (p + 1);
}

extern FILE *trace_fp;
/*
 * Find the end of the last-created loop, i.e., with M_2JC, and replace that
 * with an M_LOOP with the given limits.  Also:
 *
 * (a) resize the machine and insert a M_ENTER before the M_SAVE_POS which
 *     is at the beginning of the M_2JC loop.
 * (b) replace any nested loops within this updated loop, so that the whole
 *     stack will use M_LOOP consistently.
 *
 * Because this is applied to the last-created loop, it is not necessary to
 * adjust jump-offsets to following loops which could span this loop.  But it
 * is necessary to adjust jumps which precede (i.e., jump over) this loop.
 */
static void
RE_set_limit(MACHINE * mp, Int minlimit, Int maxlimit)
{
    STATE *p = mp->start;
    STATE *last_1st = NULL;
    STATE *last_end = NULL;
    STATE *temp = NULL;
    int nests = 0;

    TRACE(("RE_set_limit " INT_FMT ".." INT_FMT "\n", minlimit, maxlimit));

    if (p->s_type == M_2JA)
	++p;

    if (p->s_type == M_SAVE_POS) {
	int depth = 0;
	temp = p;
	do {
	    switch (temp->s_type) {
	    case M_SAVE_POS:
		if (depth++ == 0)
		    last_1st = temp;
		break;
	    case M_2JC:
		if (depth > 1)
		    ++nests;
		/* FALLTHRU */
	    case M_LOOP:
		if (--depth == 0) {
		    last_end = temp;
		}
		break;
	    case M_ACCEPT:
		depth = -1;
		break;
	    }
	    ++temp;
	} while (depth > 0);
    }
    /*
     * If we found the end of a top-level loop, we can modify it.
     */
    if (last_end != NULL) {
	size_t len = (size_t) (mp->stop - mp->start + 2);
	size_t base = (size_t) (last_1st - mp->start);
	size_t newlen = (size_t) (1 + len + (size_t) nests);
	int offset = (int) (last_end - mp->start);

	last_end->s_type = M_LOOP;
	last_end->it_min = minlimit;
	last_end->it_max = maxlimit;
	last_end->s_enter = -(offset + 1);
	last_end->s_enter += (int) base;

	/*
	 * Reallocate the states, to insert an item at the beginning.
	 *
	 * The new size accounts for any nested loops which we found, but the
	 * stop-pointer is set for the current length of the top loop.
	 */
	mp->start = (STATE *) RE_realloc(mp->start, newlen * STATESZ);
	mp->stop = mp->start + len - 1;
	temp = mp->start + base;
	len -= base;
	while (--len != 0) {
	    temp[len] = temp[len - 1];
	}
	temp->s_type = M_ENTER;
	temp->s_data.jump = (int) ((size_t) (offset + 1) - base);
	used_loop_level = 1;

	/* if there were jumps over the adjusted loop, adjust them */
	if (base) {
	    int n;
	    temp = mp->start;
	    for (n = 0; n < (int) base; ++n) {
		switch (temp[n].s_type) {
		case M_1J:
		case M_2JA:
		case M_2JB:
		    if ((size_t) (n + temp[n].s_data.jump) > base)
			temp[n].s_data.jump++;
		    break;
		}
	    }
	}

	/*
	 * Transform nested loops ending with M_2JC (+), to M_LOOP {1,}
	 * Exclude for now any which are preceded by M_2JA (*), which is
	 * not yet handled for M_LOOP (2025-01-31).
	 */
	while (nests > 0) {
	    int probe;
	    int inner;
	    int outer;
	    int ender;
	    int check;
	    int oldlen = (int) (mp->stop - mp->start + 1);
	    p = mp->start;
	    /*
	     * Look for a loop to expand.
	     */
	    for (probe = oldlen; probe != 0; --probe) {
		if (p[probe - 1].s_type == M_2JC) {
		    --probe;
		    inner = probe + p[probe].s_data.jump;
		    if (inner != 0
			&& p[inner].s_type == M_SAVE_POS
			&& p[inner - 1].s_type == M_2JA) {
			nests--;
			continue;
		    }
		    /*
		     * Adjust jumps across the loop which will be expanded.
		     */
		    for (outer = 0; outer < oldlen && outer < probe; ++outer) {
			switch (p[outer].s_type) {
			case M_2JA:
			    break;
			case M_ENTER:
			    ender = outer + p[outer].s_data.jump;
			    check = ender + p[ender].s_enter;
			    if (ender > probe
				&& check == outer) {
				p[outer].s_data.jump++;
				p[ender].s_enter--;
			    }
			    break;
			}
		    }
		    for (outer = probe + 1; outer < oldlen; ++outer) {
			switch (p[outer].s_type) {
			case M_LOOP:
			    ender = outer + p[outer].s_data.jump;
			    if (ender < probe)
				p[outer].s_data.jump--;
			    break;
			}
		    }
		    /*
		     * Now, expand the loop we found.
		     */
		    p[probe].s_type = M_LOOP;
		    p[probe].it_min = 1;
		    p[probe].it_max = MAX__INT;
		    p[probe].s_enter = (int) (inner - probe - 1);
		    for (outer = oldlen + 1; outer != inner; --outer) {
			p[outer] = p[outer - 1];
		    }
		    p[inner].s_type = M_ENTER;
		    p[inner].s_data.jump = probe - inner + 1;
		    mp->stop++;
		    /*
		     * Find any remaining jumps spanning the adjusted loop,
		     * and adjust those as well.
		     */
		    while (inner != 0) {
			switch (p[inner].s_type) {
			case M_1J:
			case M_2JA:
			case M_2JB:
			case M_2JC:
			    if (inner + p[inner].s_data.jump >= probe) {
				p[inner].s_data.jump++;
			    }
			    break;
			}
			--inner;
		    }
		    /*
		     * Done for now, look for remaining loops.
		     */
		    break;
		}
	    }
	    --nests;
	}
	for (p = mp->start, nests = 0; p != mp->stop; ++p) {
	    switch (p->s_type) {
	    case M_ENTER:
		p->it_cnt = ++nests;
		break;
	    case M_LOOP:
		--nests;
		break;
	    }
	}
    }
}

/*  replace m with m*  limited to the max iterations
        (variation of m*   closure)   */
static void
RE_close_limit(MACHINE * mp, Int min_limit, Int max_limit)
{
    RE_close(mp);
    RE_set_limit(mp, min_limit, max_limit);
}

/*  replace m with m+  limited to the max iterations
     which is one or more, limited
        (variation of m+   positive closure)   */
static void
RE_poscl_limit(MACHINE * mp, Int min_limit, Int max_limit)
{
    RE_poscl(mp);
    RE_set_limit(mp, min_limit, max_limit);
}

/* If we used M_ENTER/M_LOOP, set the level-number for M_ENTER */
static STATE *
markup_loop_levels(MACHINE * mp)
{
    STATE *p = mp->start;
    if (used_loop_level && p != NULL) {
	STATE *q = p;
	STATE *r;
	int level = 0;
	int done = 0;
	while (!done && q != mp->stop) {
	    switch (q->s_type) {
	    case M_ACCEPT:
		done = 1;
		break;
	    case M_ENTER:
		q->it_cnt = ++level;
		if (level > MAX_LOOP_LEVEL)
		    compile_error("brace expression exceeds %d levels\n",
				  MAX_LOOP_LEVEL);
		break;
	    case M_LOOP:
		r = (q + q->s_enter);
		if (level-- != (int) r->it_cnt)
		    compile_error("mismatched levels for brace-expression");
		break;
	    }
	    ++q;
	}
    }
    return p;
}
#else
#define markup_loop_levels(mp) (mp)->start
#endif /* ! NO_INTERVAL_EXPR */

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

#ifndef NO_INTERVAL_EXPR
    used_loop_level = 0;
#endif

    /* provide for making the trace a little easier to read by indenting */
#if OPT_TRACE > 1
#define M_FMT(format) "@%d: %*s " format, __LINE__, 4 * ((int) (m_ptr - m_array)), " "
#else
#define M_FMT(format) format
#endif

    while (1) {
	TRACE((M_FMT("RE_lex token %s\n"), token_name(t)));
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
	    TRACE((M_FMT("interval {%ld,%ld}\n"), (long) intrvalmin, (long) intrvalmax));
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
		    TRACE((M_FMT("RE_lex token %s\n"),
			   "of zero interval is ignored!"));
		    break;
		case 1:
		    RE_01(m_ptr);	/* m{0,1} which is m? */
		    TRACE((M_FMT("RE_lex token %s\n"), token_name(T_Q)));
		    break;
		default:
		    RE_close_limit(m_ptr, intrvalmin, intrvalmax);
		    TRACE((M_FMT("RE_lex token %s\n"), token_name(T_Q)));
		}
	    } else if (BigMachine(m_ptr)) {
		RE_poscl_limit(m_ptr, intrvalmin, intrvalmax);
	    } else if (intrvalmin == 1) {	/* one or more */
		RE_poscl_limit(m_ptr, intrvalmin, intrvalmax);
	    } else if (m_ptr->start != NULL) {	/* n or more */
		/* loop-unrolling only works if min==max, so that the loops in
		 * test/match functions can process the whole loop in each
		 * iteration */
		if (FIXME_INTERVAL_LIMITS || intrvalmin == intrvalmax) {
		    register Int i;
		    /* copy 2 copies of m_ptr, use 2nd copy to replace
		       the first copy that gets swallowed by concat */
		    MACHINE *result_mp = m_ptr;
		    MACHINE *concat_mp = (m_ptr + 1);
		    MACHINE *new_mp = (m_ptr + 2);
		    TRACE((M_FMT("calling duplicate_m result_mp %ld -> concat_mp %ld\n"),
			   result_mp - m_array,
			   concat_mp - m_array));
		    duplicate_m(concat_mp, result_mp);
		    TRACE((M_FMT("calling duplicate_m result_mp %ld -> new_mp %ld\n"),
			   result_mp - m_array,
			   new_mp - m_array));
		    duplicate_m(new_mp, result_mp);
		    for (i = 2; i <= intrvalmin; i++) {
			RE_cat(result_mp, concat_mp);
			duplicate_m(concat_mp, new_mp);
		    }
		    /* don't need 2nd copy in new_mp */
		    RE_free(new_mp->start);
		} else {
		    RE_poscl_limit(m_ptr, intrvalmin, intrvalmax);
		}
	    }
	    break;
#endif /* ! NO_INTERVAL_EXPR */

	case T_NONE:		/*  end of reg expr   */
	    if (op_ptr->token == 0) {
		/*  done   */
		if (m_ptr == m_stack(0)) {
		    return markup_loop_levels(m_ptr);
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

	if (m_ptr >= m_stack(STACKSZ - 1)) {
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
