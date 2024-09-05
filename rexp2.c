/********************************************
rexp2.c
copyright 2009-2020,2024, Thomas E. Dickey
copyright 2010, Jonathan Nieder
copyright 1991-1992,1993, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: rexp2.c,v 1.46 2024/09/05 17:44:48 tom Exp $
 */

/*  test a string against a machine   */

#include <rexp.h>

#define	 STACKGROWTH	16

RT_STATE *RE_run_stack_base;
RT_STATE *RE_run_stack_limit;

/* Large model DOS segment arithmetic breaks the current stack.
   This hack fixes it without rewriting the whole thing, 5/31/91 */
RT_STATE *RE_run_stack_empty;

RT_POS_ENTRY *RE_pos_stack_base;
RT_POS_ENTRY *RE_pos_stack_limit;
RT_POS_ENTRY *RE_pos_stack_empty;

void
RE_run_stack_init(void)
{
    if (!RE_run_stack_base) {
	RE_run_stack_base = (RT_STATE *)
	    RE_malloc(sizeof(RT_STATE) * STACKGROWTH);
	RE_run_stack_limit = RE_run_stack_base + STACKGROWTH;
	RE_run_stack_empty = RE_run_stack_base - 1;
    }
}

void
RE_pos_stack_init(void)
{
    if (!RE_pos_stack_base) {
	RE_pos_stack_base = (RT_POS_ENTRY *)
	    RE_malloc(sizeof(RT_POS_ENTRY) * STACKGROWTH);
	RE_pos_stack_limit = RE_pos_stack_base + STACKGROWTH;
	RE_pos_stack_empty = RE_pos_stack_base;

	RE_pos_stack_base->pos = NULL;	/* RE_pos_peek(RE_pos_stack_empty) */
	RE_pos_stack_base->owner = -1;	/* avoid popping base */
	RE_pos_stack_base->prev_offset = 0;	/* avoid popping below base */
    }
}

/* sometimes during REmatch(), this stack can grow pretty large.
   In real life cases, the back tracking usually fails. Some
   work is needed here to improve the algorithm.
   I.e., figure out how not to stack useless paths.
*/

RT_STATE *
RE_new_run_stack(void)
{
    size_t oldsize = (size_t) (RE_run_stack_limit - RE_run_stack_base);
    size_t newsize = oldsize + STACKGROWTH;

#ifdef	LMDOS			/* large model DOS */
    /* have to worry about overflow on multiplication (ugh) */
    if (newsize >= 4096)
	RE_run_stack_base = (RT_STATE *) 0;
    else
#endif

	RE_run_stack_base = (RT_STATE *) realloc(RE_run_stack_base,
						 newsize * sizeof(RT_STATE));

    if (!RE_run_stack_base) {
	fprintf(stderr, "out of memory for RE run time stack\n");
	/* this is pretty unusual, I've only seen it happen on
	   weird input to REmatch() under 16bit DOS , the same
	   situation worked easily on 32bit machine.  */
	mawk_exit(100);
    }

    RE_run_stack_limit = RE_run_stack_base + newsize;
    RE_run_stack_empty = RE_run_stack_base - 1;

    /* return the new stackp */
    return RE_run_stack_base + oldsize;
}

RT_POS_ENTRY *
RE_new_pos_stack(void)
{
    size_t oldsize = (size_t) (RE_pos_stack_limit - RE_pos_stack_base);
    size_t newsize = oldsize + STACKGROWTH;

    /* FIXME: handle overflow on multiplication for large model DOS
     * (see RE_new_run_stack()).
     */
    RE_pos_stack_base = (RT_POS_ENTRY *)
	realloc(RE_pos_stack_base, newsize * sizeof(RT_POS_ENTRY));

    if (!RE_pos_stack_base) {
	fprintf(stderr, "out of memory for RE string position stack\n");
	mawk_exit(100);
    }

    RE_pos_stack_limit = RE_pos_stack_base + newsize;
    RE_pos_stack_empty = RE_pos_stack_base;

    /* return the new stackp */
    return RE_pos_stack_base + oldsize;
}

#ifdef	DEBUG
static RT_STATE *
slow_push(
	     RT_STATE * sp,
	     STATE * m,
	     char *s,
	     RT_POS_ENTRY * pos_top,
	     int u)
{
    if (sp == RE_run_stack_limit)
	sp = RE_new_run_stack();
    sp->m = m;
    sp->s = s;
    sp->u = u;
    sp->sp = pos_top - RE_pos_stack_base;
    sp->tp = pos_top->prev_offset;
    return sp;
}
#endif

#ifdef	 DEBUG
#define	 push(mx,sx,px,ux) do { \
		stackp = slow_push(++stackp, mx, sx, px, ux); \
	} while(0)
#else
#define	 push(mx,sx,px,ux) do { \
		if (++stackp == RE_run_stack_limit) \
			stackp = RE_new_run_stack(); \
		stackp->m = (mx); \
		stackp->s = (sx); \
		stackp->u = (ux); \
		stackp->sp = (int) ((px) - RE_pos_stack_base); \
		stackp->tp = (px)->prev_offset; \
	} while(0)
#endif

#define	CASE_UANY(x) case (x)+U_OFF:  /* FALLTHRU */ case (x)+U_ON

static const char *
RE_u_end(int flag)
{
    static const char *utable[] =
    {
	"U_OFF + END_OFF",
	"U_ON  + END_OFF",
	"U_OFF + END_ON",
	"U_ON  + END_ON",
    };
    flag /= U_ON;
    return utable[flag % 4];
}

static GCC_NORETURN void
RE_bad_state(const char *where, STATE * m, int u_flag)
{
    RE_panic("unexpected case (%s + %s) in %s",
	     REs_type(m),
	     RE_u_end(m->s_type + u_flag),
	     where);
}

/*
 * test if str ~ /machine/
 */
int
REtest(char *str,		/* string to test */
       size_t len,		/* ...its length */
       STATE * machine)		/* compiled regular-expression */
{
    register STATE *m = machine;
    char *s = str;
    register RT_STATE *stackp;
    int u_flag;
    char *str_end = str + len;
    RT_POS_ENTRY *sp;
    int ti;			/*convenient temps */
    STATE *tm;

    TRACE(("REtest: \"%s\" ~ /pattern/\n", str));

    /* handle the easy case quickly */
    if (m->s_type == M_STR && (m + 1)->s_type == M_ACCEPT) {
	return str_str(s, len, m->s_data.str, (size_t) m->s_len) != (char *) 0;
    } else {
	u_flag = U_ON;
	stackp = RE_run_stack_empty;
	sp = RE_pos_stack_empty;
	RE_init_it_cnt(m);
	RE_CASE();
    }

  refill:
    if (stackp == RE_run_stack_empty) {
	return 0;
    }
    m = stackp->m;
    s = stackp->s;
    sp = RE_pos_stack_base + stackp->sp;
    sp->prev_offset = stackp->tp;
    u_flag = (stackp--)->u;

  reswitch:
    TRACE2(("[%s@%d] %d:%03d %-8s %-15s: %s\n", __FILE__, __LINE__,
	    (int) (stackp - RE_run_stack_base),
	    (int) (m - machine),
	    REs_type(m),
	    RE_u_end(u_flag),
	    s));

    switch (m->s_type + u_flag) {
    case M_STR + U_OFF + END_OFF:
	if (s > str_end
	    || (size_t) (str_end - s) < m->s_len
	    || memcmp(s, m->s_data.str, m->s_len)) {
	    RE_FILL();
	}
	s += m->s_len;
	m++;
	RE_CASE();

    case M_STR + U_OFF + END_ON:
	if ((size_t) (str_end - s) != m->s_len
	    || memcmp(s, m->s_data.str, m->s_len) != 0) {
	    RE_FILL();
	}
	s += m->s_len;
	m++;
	RE_CASE();

    case M_STR + U_ON + END_OFF:
	if (!(s = str_str(s, (size_t) (str_end - s), m->s_data.str, (size_t) m->s_len))) {
	    RE_FILL();
	}
	push(m, s + 1, sp, U_ON);
	s += m->s_len;
	m++;
	u_flag = U_OFF;
	RE_CASE();

    case M_STR + U_ON + END_ON:
	ti = (int) (str_end - s) - (int) m->s_len;
	if (ti < 0 || memcmp(s + ti, m->s_data.str, m->s_len)) {
	    RE_FILL();
	}
	s = str_end;
	m++;
	u_flag = U_OFF;
	RE_CASE();

    case M_CLASS + U_OFF + END_OFF:
	if (s >= str_end || !ison(*m->s_data.bvp, s[0])) {
	    RE_FILL();
	}
	s++;
	m++;
	RE_CASE();

    case M_CLASS + U_OFF + END_ON:
	if (s >= str_end) {
	    RE_FILL();
	}
	if ((s + 1) < str_end || !ison(*m->s_data.bvp, s[0])) {
	    RE_FILL();
	}
	s++;
	m++;
	RE_CASE();

    case M_CLASS + U_ON + END_OFF:
	for (;;) {
	    if (s >= str_end) {
		RE_FILL();
	    } else if (ison(*m->s_data.bvp, s[0])) {
		break;
	    }
	    s++;
	}
	s++;
	push(m, s, sp, U_ON);
	m++;
	u_flag = U_OFF;
	RE_CASE();

#ifndef LCOV_UNUSED
    case M_CLASS + U_ON + END_ON:
	/* NOTREACHED */
	if (s >= str_end || !ison(*m->s_data.bvp, str_end[-1])) {
	    RE_FILL();
	}
	s = str_end;
	m++;
	u_flag = U_OFF;
	RE_CASE();
#endif

    case M_ANY + U_OFF + END_OFF:
	if (s >= str_end) {
	    RE_FILL();
	}
	s++;
	m++;
	RE_CASE();

#ifndef LCOV_UNUSED
    case M_ANY + U_OFF + END_ON:
	/* NOTREACHED */
	if (s >= str_end || (s + 1) < str_end) {
	    RE_FILL();
	}
	s++;
	m++;
	RE_CASE();
#endif

    case M_ANY + U_ON + END_OFF:
	if (s >= str_end) {
	    RE_FILL();
	}
	s++;
	push(m, s, sp, U_ON);
	m++;
	u_flag = U_OFF;
	RE_CASE();

#ifndef LCOV_UNUSED
    case M_ANY + U_ON + END_ON:
	/* NOTREACHED */
	if (s >= str_end) {
	    RE_FILL();
	}
	s = str_end;
	m++;
	u_flag = U_OFF;
	RE_CASE();
#endif

    case M_START + U_OFF + END_OFF:
    case M_START + U_ON + END_OFF:
	if (s != str) {
	    RE_FILL();
	}
	m++;
	u_flag = U_OFF;
	RE_CASE();

    case M_START + U_OFF + END_ON:
    case M_START + U_ON + END_ON:
	if (s != str || s < str_end) {
	    RE_FILL();
	}
	m++;
	u_flag = U_OFF;
	RE_CASE();

    case M_END + U_OFF:
	if (s < str_end) {
	    RE_FILL();
	}
	m++;
	RE_CASE();

    case M_END + U_ON:
	s = str_end;
	m++;
	u_flag = U_OFF;
	RE_CASE();

      CASE_UANY(M_U):
	u_flag = U_ON;
	m++;
	RE_CASE();

      CASE_UANY(M_1J):
	m += m->s_data.jump;
	RE_CASE();

      CASE_UANY(M_SAVE_POS):	/* save position for a later M_2JC */
	sp = RE_pos_push(sp, stackp, s);
	m++;
	RE_CASE();

      CASE_UANY(M_2JA):	/* take the non jump branch */
	/* don't stack an ACCEPT */
	if ((tm = m + m->s_data.jump)->s_type == M_ACCEPT) {
	    return 1;
	}
	push(tm, s, sp, u_flag);
	m++;
	RE_CASE();

      CASE_UANY(M_2JC):	/* take the jump branch if position changed */
#ifndef NO_INTERVAL_EXPR
	if (m->it_max < MAX__INT && ++(m->it_cnt) >= m->it_max) {
	    RE_pos_pop(&sp, stackp);
	    m++;
	    RE_CASE();		/* test the next thing */
	} else
#endif /* ! NO_INTERVAL_EXPR */
	if (RE_pos_pop(&sp, stackp) == s) {
	    /* did not advance: do not jump back */
	    m++;
	    RE_CASE();
	}
	/* don't stack an ACCEPT */
	if ((tm = m + 1)->s_type == M_ACCEPT) {
	    return 1;
	}
	push(tm, s, sp, u_flag);
	m += m->s_data.jump;
	RE_CASE();

      CASE_UANY(M_2JB):
	/* don't stack an ACCEPT */
	if ((tm = m + 1)->s_type == M_ACCEPT) {
	    return 1;
	}
	push(tm, s, sp, u_flag);
	m += m->s_data.jump;
	RE_CASE();

      CASE_UANY(M_ACCEPT):
	return 1;

    default:
	RE_bad_state("REtest", m, u_flag);
    }
}

#undef push

#include <field.h>

char *
is_string_split(PTR q, size_t * lenp)
{
    STATE *p = cast_to_re(q);

    if (p != 0 && (p[0].s_type == M_STR && p[1].s_type == M_ACCEPT)) {
	*lenp = p->s_len;
	return p->s_data.str;
    } else
	return (char *) 0;
}
