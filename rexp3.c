/********************************************
rexp3.c
copyright 2008-2017,2020, Thomas E. Dickey
copyright 2010, Jonathan Nieder
copyright 1991-1992,1993, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: rexp3.c,v 1.41 2020/01/20 15:16:04 tom Exp $
 */

/*  match a string against a machine   */

#include "rexp.h"

#define	 push(mx,sx,px,ssx,ux) do { \
	if (++stackp == RE_run_stack_limit) \
		stackp = RE_new_run_stack() ;\
	stackp->m = (mx); \
	stackp->s = (sx); \
	stackp->sp = (int) ((px) - RE_pos_stack_base); \
	stackp->tp = (px)->prev_offset; \
	stackp->ss = (ssx); \
	stackp->u = (ux); \
} while(0)

#define	CASE_UANY(x) case (x)+U_OFF:  /* FALLTHRU */ case (x)+U_ON

#define TR_STR(s) TRACE((" str:%i len:%lu\n", ((s) ? (int) ((s) - str) : -99), (unsigned long) *lenp))
#define RE_TURN() \
	if (cb_ss) { \
	    *lenp = (size_t) (cb_e - cb_ss); \
	} \
	TR_STR(s); \
	return cb_ss

/* returns start of first longest match and the length by
   reference.  If no match returns NULL and length zero */

char *
REmatch(char *str,		/* string to test */
	size_t str_len,		/* ...its length */
	PTR machine,		/* compiled regular expression */
	size_t *lenp,		/* where to return matched-length */
	int no_bol)		/* disallow match at beginning of line */
{
    register STATE *m = (STATE *) machine;
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

    TRACE(("REmatch: %s \"%s\" ~ /pattern/", no_bol ? "any" : "1st", str));

    /* check for the easy case */
    if (m->s_type == M_STR && (m + 1)->s_type == M_ACCEPT) {
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
    RE_CASE();

  refill:
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
    sp = RE_pos_stack_base + (stackp + 1)->sp;
    sp->prev_offset = (stackp + 1)->tp;
    u_flag = (stackp + 1)->u;

  reswitch:
    switch (m->s_type + u_flag) {
    case M_STR + U_OFF + END_OFF:
	if (strncmp(s, m->s_data.str, (size_t) m->s_len)) {
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
	RE_CASE();

    case M_STR + U_OFF + END_ON:
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
	RE_CASE();

    case M_STR + U_ON + END_OFF:
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
	RE_CASE();

    case M_STR + U_ON + END_ON:
	t = (int) ((SLen) (str_end - s) - m->s_len);
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
	if (RE_pos_pop(&sp, stackp) == s) {
	    m++;
	    RE_CASE();
	}
	/* FALLTHRU */
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
	    cb_ss = ss;
	    cb_e = s;
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
	    cb_ss = ss;
	    cb_e = s;
	} else if (ss == cb_ss && s == cb_e) {
	    RE_TURN();
	}
	RE_FILL();

    default:
	RE_panic("unexpected case in REmatch");
    }
}
#undef push
