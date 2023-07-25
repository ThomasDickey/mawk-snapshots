/********************************************
rexp3.c
copyright 2008-2020,2023, Thomas E. Dickey
copyright 2010, Jonathan Nieder
copyright 1991-1992,1993, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: rexp3.c,v 1.50 2023/07/24 20:43:39 tom Exp $
 */

/*  match a string against a machine   */

#include "rexp.h"

#define	 push(mx,sx,px,ssx,ux) do { \
	if (++stackp == RE_run_stack_limit) \
		stackp = RE_new_run_stack() ;\
	TRACE2(("@%d, pushing %d:%03d\n", __LINE__, (int)(stackp - RE_run_stack_base), (int)(m - machine))); \
	stackp->m = (mx); \
	stackp->s = (sx); \
	stackp->sp = (int) ((px) - RE_pos_stack_base); \
	stackp->tp = (px)->prev_offset; \
	stackp->ss = (ssx); \
	stackp->u = (ux); \
} while(0)

#ifdef NO_RI_LOOP_UNROLL
#define restart_count(old,new) \
	if (old != new) { \
		TRACE2(("RESET %p ->%p\n", old, new)); \
		m->it_cnt = 1; \
	}
#else
#define restart_count(old,new)	/* nothing */
#endif

#define	CASE_UANY(x) case (x)+U_OFF:  /* FALLTHRU */ case (x)+U_ON

#define TR_STR(s) TRACE((" str:%i len:%lu\n", ((s) ? (int) ((s) - str) : -99), (unsigned long) *lenp))
#define RE_TURN() \
	if (cb_ss) { \
	    *lenp = (size_t) (cb_e - cb_ss); \
	} \
	TR_STR(s); \
	TRACE2(("returning @%d: %d\n", __LINE__, cb_ss ? (int)(cb_ss - str) : -1)); \
	return cb_ss

#if OPT_TRACE
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
#endif

/* returns start of first longest match and the length by
   reference.  If no match returns NULL and length zero */

char *
REmatch(char *str,		/* string to test */
	size_t str_len,		/* ...its length */
	STATE * machine,	/* compiled regular expression */
	size_t *lenp,		/* where to return matched-length */
	int no_bol)		/* disallow match at beginning of line */
{
    register STATE *m = machine;
    char *s = str;
    char *ss;
    register RT_STATE *stackp;
    int u_flag, t;
    char *str_end = s + str_len;
    RT_POS_ENTRY *sp;
    char *ts;

    /* state of current best match stored here */
    char *cb_ss;		/* the start */
    char *cb_e = 0;		/* the end , pts at first char not matched */

    *lenp = 0;

    TRACE(("REmatch: %s \"%s\" ~ /pattern/\n", no_bol ? "any" : "1st", str));

    /* check for the easy case */
    if (m->s_type == M_STR && (m + 1)->s_type == M_ACCEPT) {
	TRACE2(("@%d, now %03d\n", __LINE__, (int) (m - machine)));
	if ((ts = str_str(s, str_len, m->s_data.str, (size_t) m->s_len))) {
	    *lenp = m->s_len;
	}
	TR_STR(ts);
	return ts;
    }

    u_flag = U_ON;
    cb_ss = ss = (char *) 0;
    stackp = RE_run_stack_empty;
    sp = RE_pos_stack_empty;
    RE_init_it_cnt(m);
    RE_CASE();

  refill:
    TRACE2(("@%d, refill machine %03d\n", __LINE__, (int) (m - machine)));
    if (stackp == RE_run_stack_empty) {
	RE_TURN();
    }
    ss = stackp->ss;
    s = (stackp--)->s;
    if (cb_ss) {		/* does new state start too late ? */
	if (ss) {
	    if (cb_ss < ss || (cb_ss == ss && cb_e == str_end)) {
		RE_FILL();
	    }
	} else if (cb_ss < s || (cb_ss == s && cb_e == str_end)) {
	    RE_FILL();
	}
    }

    m = (stackp + 1)->m;
    TRACE2(("@%d, now %03d\n", __LINE__, (int) (m - machine)));
    sp = RE_pos_stack_base + (stackp + 1)->sp;
    sp->prev_offset = (stackp + 1)->tp;
    u_flag = (stackp + 1)->u;

  reswitch:
    TRACE(("[%s@%d] %d:%03d %-8s %-15s: %s\n", __FILE__, __LINE__,
	   (int) (stackp - RE_run_stack_base),
	   (int) (m - machine),
	   REs_type(m),
	   RE_u_end(u_flag),
	   cb_ss ? cb_ss : s));
    switch (m->s_type + u_flag) {
    case M_STR + U_OFF + END_OFF:
	TRACE2(("@%d, now %03d\n", __LINE__, (int) (m - machine)));
	if (strncmp(s, m->s_data.str, (size_t) m->s_len)) {
	    TRACE2(("@%d, now %03d\n", __LINE__, (int) (m - machine)));
	    RE_FILL();
	}
	if (!ss) {
	    if (cb_ss && s > cb_ss) {
		TRACE2(("@%d, now %03d\n", __LINE__, (int) (m - machine)));
		RE_FILL();
	    } else {
		ss = s;
	    }
	}
	s += m->s_len;
	m++;
	TRACE2(("@%d, next %03d\n", __LINE__, (int) (m - machine)));
	RE_CASE();

    case M_STR + U_OFF + END_ON:
	TRACE2(("@%d, now %03d\n", __LINE__, (int) (m - machine)));
	if (strcmp(s, m->s_data.str)) {
	    RE_FILL();
	}
	if (!ss) {
	    if (cb_ss && s > cb_ss) {
		RE_FILL();
	    } else {
		ss = s;
	    }
	}
	s += m->s_len;
	m++;
	TRACE2(("@%d, next %03d\n", __LINE__, (int) (m - machine)));
	RE_CASE();

    case M_STR + U_ON + END_OFF:
	TRACE2(("@%d, now %03d\n", __LINE__, (int) (m - machine)));
	if (s >= str_end) {
	    RE_FILL();
	}
	if (!(s = str_str(s, (size_t) (str_end - s), m->s_data.str, (size_t) m->s_len))) {
	    RE_FILL();
	}
	if (s >= str_end) {
	    RE_FILL();
	}
	push(m, s + 1, sp, ss, U_ON);
	if (!ss) {
	    if (cb_ss && s > cb_ss) {
		RE_FILL();
	    } else {
		ss = s;
	    }
	}
	s += m->s_len;
	m++;
	u_flag = U_OFF;
	TRACE2(("@%d, next %03d\n", __LINE__, (int) (m - machine)));
	RE_CASE();

    case M_STR + U_ON + END_ON:
	TRACE2(("@%d, now %03d\n", __LINE__, (int) (m - machine)));
	t = (int) ((size_t) (str_end - s) - m->s_len);
	if (t < 0 || memcmp(ts = s + t, m->s_data.str, (size_t) m->s_len)) {
	    RE_FILL();
	}
	if (!ss) {
	    if (cb_ss && ts > cb_ss) {
		RE_FILL();
	    } else {
		ss = ts;
	    }
	}
	s = str_end;
	m++;
	u_flag = U_OFF;
	TRACE2(("@%d, next %03d\n", __LINE__, (int) (m - machine)));
	RE_CASE();

    case M_CLASS + U_OFF + END_OFF:
	if (s >= str_end) {
	    RE_FILL();
	}
	if (!ison(*m->s_data.bvp, s[0])) {
	    RE_FILL();
	}
	if (!ss) {
	    if (cb_ss && s > cb_ss) {
		RE_FILL();
	    } else {
		ss = s;
	    }
	}
	s++;
	m++;
	RE_CASE();

    case M_CLASS + U_OFF + END_ON:
	if (s >= str_end) {
	    RE_FILL();
	}
	if (s[1] || !ison(*m->s_data.bvp, s[0])) {
	    RE_FILL();
	}
	if (!ss) {
	    if (cb_ss && s > cb_ss) {
		RE_FILL();
	    } else {
		ss = s;
	    }
	}
	s++;
	m++;
	RE_CASE();

    case M_CLASS + U_ON + END_OFF:
	if (s >= str_end) {
	    RE_FILL();
	}
	while (!ison(*m->s_data.bvp, s[0])) {
	    if (s >= str_end) {
		RE_FILL();
	    } else {
		s++;
	    }
	}
	if (s >= str_end) {
	    RE_FILL();
	}
	s++;
	push(m, s, sp, ss, U_ON);
	if (!ss) {
	    if (cb_ss && s - 1 > cb_ss) {
		RE_FILL();
	    } else {
		ss = s - 1;
	    }
	}
	m++;
	u_flag = U_OFF;
	RE_CASE();

    case M_CLASS + U_ON + END_ON:
	if ((s >= str_end) || !ison(*m->s_data.bvp, str_end[-1])) {
	    RE_FILL();
	}
	if (!ss) {
	    if (cb_ss && str_end - 1 > cb_ss) {
		RE_FILL();
	    } else {
		ss = str_end - 1;
	    }
	}
	s = str_end;
	m++;
	u_flag = U_OFF;
	RE_CASE();

    case M_ANY + U_OFF + END_OFF:
	if (s >= str_end) {
	    RE_FILL();
	}
	if (!ss) {
	    if (cb_ss && s > cb_ss) {
		RE_FILL();
	    } else {
		ss = s;
	    }
	}
	s++;
	m++;
	RE_CASE();

    case M_ANY + U_OFF + END_ON:
	if ((s >= str_end) || ((s + 1) < str_end)) {
	    RE_FILL();
	}
	if (!ss) {
	    if (cb_ss && s > cb_ss) {
		RE_FILL();
	    } else {
		ss = s;
	    }
	}
	s++;
	m++;
	RE_CASE();

    case M_ANY + U_ON + END_OFF:
	if (s >= str_end) {
	    RE_FILL();
	}
	s++;
	push(m, s, sp, ss, U_ON);
	if (!ss) {
	    if (cb_ss && s - 1 > cb_ss) {
		RE_FILL();
	    } else {
		ss = s - 1;
	    }
	}
	m++;
	u_flag = U_OFF;
	RE_CASE();

    case M_ANY + U_ON + END_ON:
	if (s >= str_end) {
	    RE_FILL();
	}
	if (!ss) {
	    if (cb_ss && str_end - 1 > cb_ss) {
		RE_FILL();
	    } else {
		ss = str_end - 1;
	    }
	}
	s = str_end;
	m++;
	u_flag = U_OFF;
	RE_CASE();

    case M_START + U_OFF + END_OFF:
    case M_START + U_ON + END_OFF:
	if (s != str || no_bol) {
	    RE_FILL();
	}
	ss = s;
	m++;
	u_flag = U_OFF;
	RE_CASE();

    case M_START + U_OFF + END_ON:
    case M_START + U_ON + END_ON:
	if (s != str || no_bol || (s < str_end)) {
	    RE_FILL();
	}
	ss = s;
	m++;
	u_flag = U_OFF;
	RE_CASE();

    case M_END + U_OFF:
	if (s < str_end) {
	    RE_FILL();
	}
	if (!ss) {
	    if (cb_ss && s > cb_ss) {
		RE_FILL();
	    } else {
		ss = s;
	    }
	}
	m++;
	RE_CASE();

    case M_END + U_ON:
	s = str_end;
	if (!ss) {
	    if (cb_ss && s > cb_ss) {
		RE_FILL();
	    } else {
		ss = s;
	    }
	}
	m++;
	u_flag = U_OFF;
	RE_CASE();

      CASE_UANY(M_U):
	if (!ss) {
	    if (cb_ss && s > cb_ss) {
		RE_FILL();
	    } else {
		ss = s;
	    }
	}
	u_flag = U_ON;
	m++;
	RE_CASE();

      CASE_UANY(M_1J):
	m += m->s_data.jump;
	RE_CASE();

      CASE_UANY(M_SAVE_POS):	/* save position for a later M_2JC */
	/* see also REtest */
	sp = RE_pos_push(sp, stackp, s);
	m++;
	RE_CASE();

      CASE_UANY(M_2JA):	/* take the non jump branch */
	push(m + m->s_data.jump, s, sp, ss, u_flag);
	m++;
	RE_CASE();

    case (M_2JC) + U_OFF:	/* take the jump branch if position changed */
    case (M_2JC) + U_ON:
	/* see REtest */
#ifndef NO_INTERVAL_EXPR
#ifdef NO_RI_LOOP_UNROLL
	m->it_cnt++;
	TRACE(("checking loop " INT_FMT " [" INT_FMT ".." INT_FMT "]\n",
	       m->it_cnt, m->it_min, m->it_max));
	TR_STR(s);
	if (m->it_cnt < m->it_min) {
	    /* keep looping until minimum is met */
	    RE_pos_pop(&sp, stackp);
	    push(m + 1, s, sp, ss, u_flag);
	    m += m->s_data.jump;
	    TRACE2(("TEST @%d: %03d\n", __LINE__, (int) (m - machine)));
	} else if ((m->it_cnt >= m->it_min)
		   && (m->it_max == MAX__INT
		       || (m->it_max < MAX__INT && m->it_cnt >= m->it_max))) {
	    /* quit looping once maximum is met */
	    RE_pos_pop(&sp, stackp);
	    m++;
	    TRACE2(("TEST @%d: %03d\n", __LINE__, (int) (m - machine)));
	} else
#else /* !NO_RI_LOOP_UNROLL */
	if (m->it_max < MAX__INT && ++(m->it_cnt) >= m->it_max) {
	    RE_CASE();		/* test the next thing */
	} else
#endif /* NO_RI_LOOP_UNROLL */
	if (RE_pos_pop(&sp, stackp) == s) {
	    /* fall out of loop, to next instruction */
	    m++;
	    TRACE2(("TEST @%d: %03d\n", __LINE__, (int) (m - machine)));
	} else {
	    /* continue looping as long as matching */
	    push(m + 1, s, sp, ss, u_flag);
	    m += m->s_data.jump;
	    TRACE2(("TEST @%d: %03d\n", __LINE__, (int) (m - machine)));
	}
	RE_CASE();
#else
	if (RE_pos_pop(&sp, stackp) == s) {
	    m++;
	    RE_CASE();
	}
	/* FALLTHRU */
#endif /* ! NO_INTERVAL_EXPR */
    case (M_2JB) + U_OFF:	/* take the jump branch */
	/* FALLTHRU */
    case (M_2JB) + U_ON:
	push(m + 1, s, sp, ss, u_flag);
	m += m->s_data.jump;
	RE_CASE();

    case M_ACCEPT + U_OFF:
	if (!ss)
	    ss = s;
	if (!cb_ss || ss < cb_ss || (ss == cb_ss && s > cb_e)) {
	    /* we have a new current best */
	    restart_count(cb_ss, ss);
	    cb_ss = ss;
	    cb_e = s;
	    TRACE2(("@%d, new best [%d..%d]'%.*s'\n", __LINE__,
		    (int) (cb_ss - str),
		    (int) (cb_e - str),
		    (int) (cb_e - cb_ss),
		    cb_ss));
	} else if (ss == cb_ss && s == cb_e) {
	    RE_TURN();
	}
	RE_FILL();

    case M_ACCEPT + U_ON:
	if (!ss) {
	    ss = s;
	} else {
	    s = str_end;
	}

	if (!cb_ss || ss < cb_ss || (ss == cb_ss && s > cb_e)) {
	    /* we have a new current best */
	    restart_count(cb_ss, ss);
	    cb_ss = ss;
	    cb_e = s;
	    TRACE2(("@%d, new best [%d..%d]'%.*s'\n", __LINE__,
		    (int) (cb_ss - str),
		    (int) (cb_e - str),
		    (int) (cb_e - cb_ss),
		    cb_ss));
	} else if (ss == cb_ss && s == cb_e) {
	    RE_TURN();
	}
	RE_FILL();

    default:
	RE_panic("unexpected case in REmatch");
    }
}
#undef push
