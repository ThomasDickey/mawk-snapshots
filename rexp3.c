/********************************************
rexp3.c
copyright 2008-2023,2024, Thomas E. Dickey
copyright 2010, Jonathan Nieder
copyright 1991-1992,1993, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: rexp3.c,v 1.70 2024/12/31 10:20:48 tom Exp $
 */

/*  match a string against a machine   */

#include <rexp.h>

#define rt_push(mx,sx,px,ssx,ux) do { \
	if (++run_entry == RE_run_stack_limit) \
		run_entry = RE_new_run_stack() ;\
	run_entry->m = (mx); \
	run_entry->s = (sx); \
	run_entry->pos_index = (int) ((px) - RE_pos_stack_base); \
	run_entry->top_index = (px)->prev_offset; \
	run_entry->ss = (ssx); \
	run_entry->u = (ux); \
	TRACE2((rt_form "rt_push %s\n", rt_args, REs_type(mx))); \
} while(0)

#define	CASE_UANY(x) case (x)+U_OFF:  /* FALLTHRU */ case (x)+U_ON

#define TR_BEST() \
	TRACE2((rt_form "new best [%d..%d] \"%.*s\"\n", rt_args, \
		(int) (cb_ss - str), \
		(int) (cb_e - str), \
		(int) (cb_e - cb_ss), \
		cb_ss))

#define TR_STR(s) \
	TRACE((rt_form "str:%i len:%lu\n", rt_args, \
		((s) ? (int) ((s) - str) : -99), \
		(unsigned long) *lenp))

#define RE_TURN() \
	if (cb_ss) { \
	    *lenp = (size_t) (cb_e - cb_ss); \
	} \
	TR_STR(s); \
	TRACE2((rt_form "returning %d\n", rt_args, \
		cb_ss ? (int)(cb_ss - str) : -1)); \
	return cb_ss

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
    char *s;
    char *ss;
    const char *old_s;
    RT_STATE *run_entry = NULL;
    int u_flag;
    char *str_end;
    RT_POS_ENTRY *pos_entry;
    char *ts;

    /* state of current best match stored here */
#define current_best(n) (cb_ss < (n) || (cb_e == str_end && cb_ss == (n)))
    char *cb_ss;		/* the start */
    char *cb_e;			/* the end , pts at first char not matched */

    *lenp = 0;

    TRACE(("REmatch: %s \"%s\" ~ /pattern/\n", no_bol ? "any" : "1st", str));

    /* check for the easy case */
    if (m->s_type == M_STR && (m + 1)->s_type == M_ACCEPT) {
	if ((ts = str_str(str, str_len, m->s_data.str, m->s_len))) {
	    *lenp = m->s_len;
	}
	TR_STR(ts);
	return ts;
    }

    str_end = str + str_len;
    s = str;
    u_flag = U_ON;
    cb_e = cb_ss = ss = (char *) 0;
    run_entry = RE_run_stack_empty;
    pos_entry = RE_pos_stack_empty;
    RE_CASE();

  refill:
    TRACE((rt_form "refill... pos@%d\n", rt_args,
	   (int) (pos_entry - RE_pos_stack_base)));
#ifndef NO_INTERVAL_EXPR
    if (0) {
#if OPT_TRACE > 1
	RT_STATE *statep;
	RT_POS_ENTRY *posp;

	for (statep = RE_run_stack_base; statep <= run_entry; ++statep) {
	    TRACE(("%s - STATE %ld: m %03ld s \"%s\" pos@%d top@%d u %d\n",
		   statep == run_entry ? "CHECK" : "check",
		   (statep - RE_run_stack_base),
		   (statep->m - machine),
		   NonNull(statep->s),
		   statep->pos_index,
		   statep->top_index,
		   statep->u));
	}
	for (posp = RE_pos_stack_base; posp <= pos_entry; ++posp) {
	    TRACE(("%s - POS %ld: pos \"%s\" owner@%d prev@%d\n",
		   posp == pos_entry ? "CHECK" : "check",
		   (posp - RE_pos_stack_base),
		   NonNull(posp->pos),
		   posp->owner,
		   posp->prev_offset));
	}
#endif
    }
#endif
    if (run_entry == RE_run_stack_empty) {
	RE_TURN();
    }
    ss = run_entry->ss;
    s = run_entry->s;
    rt_pop();
    TRACE((rt_form "run-sp s=\"%s\", ss=\"%s\"\n", rt_args,
	   NonNull(s),
	   NonNull(ss)));
    if (cb_ss) {		/* does new state start too late ? */
	if (ss) {
	    if (current_best(ss)) {
		RE_FILL();
	    }
	} else if (current_best(s)) {
	    RE_FILL();
	}
    }

    TRACE((rt_form "run-sp type %s -> %s\n", rt_args,
	   REs_type(m),
	   REs_type((run_entry + 1)->m)));

    m = (run_entry + 1)->m;
    pos_entry = RE_pos_stack_base + (run_entry + 1)->pos_index;
    pos_entry->prev_offset = (run_entry + 1)->top_index;
    u_flag = (run_entry + 1)->u;

  reswitch:
    TRACE((rt_form "%-8s %-15s: \"%s\"\n", rt_args,
	   REs_type(m),
	   RE_u_end(u_flag),
	   cb_ss ? cb_ss : s));
    switch (m->s_type + u_flag) {
    case M_STR + U_OFF + END_OFF:
	if (s >= str_end || (str_end - s) < (ptrdiff_t) m->s_len) {
	    TR_AT("now too far to match");
	    RE_FILL();
	} else if (memcmp(s, m->s_data.str, m->s_len) != 0) {
	    TR_AT("now mismatched");
	    RE_FILL();
	}
	TR_AT("now matched");
	if (!ss) {
	    if (cb_ss && current_best(s)) {
		TR_AT("new match is not better");
		RE_FILL();
	    } else {
		ss = s;
	    }
	}
	s += m->s_len;
	m++;
	TR_AT("next");
	RE_CASE();

    case M_STR + U_OFF + END_ON:
	TR_AT("now");
	if ((str_end - s) != (ptrdiff_t) m->s_len) {
	    RE_FILL();
	} else if (memcmp(s, m->s_data.str, m->s_len) != 0) {
	    RE_FILL();
	}
	if (!ss) {
	    if (cb_ss && current_best(s)) {
		TR_AT("new match is not better");
		RE_FILL();
	    } else {
		ss = s;
	    }
	}
	s = str_end;
	m++;
	TR_AT("next");
	RE_CASE();

    case M_STR + U_ON + END_OFF:
	TR_AT("now");
	if (s >= str_end) {
	    RE_FILL();
	} else if (s < str) {
	    s = str;
	}
	s = str_str(s, (size_t) (str_end - s), m->s_data.str, m->s_len);
	if (s == NULL) {
	    RE_FILL();
	}
	rt_push(m, s + 1, pos_entry, ss, U_ON);
	if (!ss) {
	    if (cb_ss && current_best(s)) {
		TR_AT("new match is not better");
		RE_FILL();
	    } else {
		ss = s;
	    }
	}
	s += m->s_len;
	m++;
	u_flag = U_OFF;
	TR_AT("next");
	RE_CASE();

    case M_STR + U_ON + END_ON:
	TR_AT("now");
	if (s >= str_end) {
	    RE_FILL();
	} else if (s < str) {
	    s = str;
	} {
	    ptrdiff_t ti = (str_end - s) - (ptrdiff_t) m->s_len;
	    if (ti < 0 || memcmp(s = s + ti, m->s_data.str, m->s_len) != 0) {
		RE_FILL();
	    }
	}
	if (!ss) {
	    if (cb_ss && current_best(s)) {
		RE_FILL();
	    } else {
		ss = s;
	    }
	}
	s = str_end;
	m++;
	u_flag = U_OFF;
	TR_AT("next");
	RE_CASE();

    case M_CLASS + U_OFF + END_OFF:
	if (s >= str_end) {
	    RE_FILL();
	} else if (!ison(*m->s_data.bvp, s[0])) {
	    RE_FILL();
	} else if (!ss) {
	    if (cb_ss && current_best(s)) {
		RE_FILL();
	    } else {
		ss = s;
	    }
	}
	s++;
	m++;
	RE_CASE();

    case M_CLASS + U_OFF + END_ON:
	if (s >= str_end || s[1] || !ison(*m->s_data.bvp, s[0])) {
	    RE_FILL();
	} else if (!ss) {
	    if (cb_ss && current_best(s)) {
		TR_AT("new match is not better");
		RE_FILL();
	    } else {
		ss = s;
	    }
	}
	s = str_end;
	m++;
	RE_CASE();

    case M_CLASS + U_ON + END_OFF:
	if (s < str)
	    s = str;
	while (1) {
	    if (s >= str_end) {
		RE_FILL();
	    }
	    if (ison(*m->s_data.bvp, s[0]))
		break;
	    s++;
	}
	rt_push(m, s + 1, pos_entry, ss, U_ON);
	if (!ss) {
	    if (cb_ss && current_best(s)) {
		TR_AT("new match is not better");
		RE_FILL();
	    } else {
		ss = s;
	    }
	}
	s++;
	m++;
	u_flag = U_OFF;
	RE_CASE();

#ifndef LCOV_UNUSED
    case M_CLASS + U_ON + END_ON:
	/* NOTREACHED */
	if (s < str)
	    s = str;
	if (s >= str_end || !ison(*m->s_data.bvp, str_end[-1])) {
	    RE_FILL();
	} else if (!ss) {
	    char *xs = str_end - 1;
	    if (cb_ss && current_best(xs)) {
		TR_AT("new match is not better");
		RE_FILL();
	    } else {
		ss = xs;
	    }
	}
	s = str_end;
	m++;
	u_flag = U_OFF;
	RE_CASE();
#endif

    case M_ANY + U_OFF + END_OFF:
	if (s >= str_end) {
	    RE_FILL();
	} else if (!ss) {
	    if (cb_ss && current_best(s)) {
		TR_AT("new match is not better");
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
	} else if (!ss) {
	    if (cb_ss && current_best(s)) {
		TR_AT("new match is not better");
		RE_FILL();
	    } else {
		ss = s;
	    }
	}
	s = str_end;
	m++;
	RE_CASE();

    case M_ANY + U_ON + END_OFF:
	if (s < str)
	    s = str;
	if (s >= str_end) {
	    RE_FILL();
	}
	rt_push(m, s + 1, pos_entry, ss, U_ON);
	if (!ss) {
	    if (cb_ss && current_best(s)) {
		TR_AT("new match is not better");
		RE_FILL();
	    } else {
		ss = s;
	    }
	}
	s++;
	m++;
	u_flag = U_OFF;
	RE_CASE();

    case M_ANY + U_ON + END_ON:
	if (s < str)
	    s = str;
	if (s >= str_end) {
	    RE_FILL();
	}
	s = str_end - 1;
	if (!ss) {
	    if (cb_ss && current_best(s)) {
		TR_AT("new match is not better");
		RE_FILL();
	    } else {
		ss = s;
	    }
	}
	s = str_end;
	m++;
	u_flag = U_OFF;
	RE_CASE();

      CASE_UANY(M_START + END_OFF):
	if (s != str || no_bol) {
	    RE_FILL();
	}
	ss = s = str;
	m++;
	u_flag = U_OFF;
	RE_CASE();

#ifndef LCOV_UNUSED
      CASE_UANY(M_START + END_ON):
	/* NOTREACHED */
	if (s != str || no_bol || (s < str_end)) {
	    RE_FILL();
	}
	ss = str;
	s = str + 1;
	m++;
	u_flag = U_OFF;
	RE_CASE();
#endif

    case M_END + U_OFF:
	if (s != str_end) {
	    RE_FILL();
	} else if (!ss) {
	    if (cb_ss) {
		TR_AT("new match is not better");
		RE_FILL();
	    } else {
		ss = str_end;
	    }
	}
	s = str_end;
	m++;
	RE_CASE();

    case M_END + U_ON:
	if (s >= str_end) {
	    if (cb_ss && current_best(s)) {
		RE_FILL();
	    }
	} else if (!ss) {
	    if (cb_ss) {
		TR_AT("new match is not better");
		RE_FILL();
	    } else
		ss = str_end;
	}
	s = str_end;
	m++;
	u_flag = U_OFF;
	RE_CASE();

      CASE_UANY(M_U):
	if (s < str)
	    s = str;
	if (!ss) {
	    if (cb_ss && current_best(s)) {
		TR_AT("new match is not better");
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
	pos_push(pos_entry, run_entry, s);
	m++;
	RE_CASE();

      CASE_UANY(M_2JA):	/* take the non jump branch */
	rt_push(m + m->s_data.jump, s, pos_entry, ss, u_flag);
	m++;
	RE_CASE();

      CASE_UANY(M_2JB):	/* take the non jump branch */
	rt_push(m + m->s_data.jump, s, pos_entry, ss, u_flag);
	m++;
	RE_CASE();

#ifndef NO_INTERVAL_EXPR
      CASE_UANY(M_ENTER):	/* take the jump branch if position changed */
	(m + m->s_data.jump)->it_cnt = 0;
	m++;
	RE_CASE();

      CASE_UANY(M_LOOP):	/* take the jump branch if position changed */
	m->it_cnt++;
	TRACE(("checking #%d: loop " INT_FMT " [" INT_FMT ".." INT_FMT "]\n",
	       (int) (pos_entry - RE_pos_stack_base),
	       m->it_cnt, m->it_min, m->it_max));
	if (m->it_max < MAX__INT && m->it_cnt >= m->it_max) {
	    ++m;
	    TR_AT("now test the next thing");
	    RE_CASE();		/* test the next thing */
	} else if (m->it_cnt < m->it_min) {
	    TR_AT("now continue getting minimum");
	    m += m->s_data.jump;
	    RE_CASE();
	}
	goto fall_through;	/* workaround for gcc bug */
      fall_through:
	/* FALLTHRU */
#endif

      CASE_UANY(M_2JC):	/* take the jump branch if position changed */
	pos_pop(pos_entry, run_entry, old_s);
	if (old_s == s) {
	    m++;
	    TR_AT("now fall out of loop");
	} else {
	    rt_push(m + 1, s, pos_entry, ss, u_flag);
	    m += m->s_data.jump;
	    TR_AT("now continue loop to match");
	}
	RE_CASE();

    case M_ACCEPT + U_OFF:
	if (s >= str_end) {
	    s = str_end;
	}
	if (!ss)
	    ss = s;
	if (!cb_ss || ss < cb_ss || (ss == cb_ss && s > cb_e)) {
	    /* we have a new current best */
	    cb_ss = ss;
	    cb_e = s;
	    TR_BEST();
	} else if (ss == cb_ss && s == cb_e) {
	    RE_TURN();		/* fix infinite loop for noloop1 */
	}
	RE_FILL();

    case M_ACCEPT + U_ON:
	if (s >= str_end) {
	    s = str_end;
	} else if (s < str) {
	    s = str;
	}
	if (!ss) {
	    ss = s;
	} else {
	    s = str_end;
	}
	if (!cb_ss || ss < cb_ss || (ss == cb_ss && s > cb_e)) {
	    /* we have a new current best */
	    cb_ss = ss;
	    cb_e = s;
	    TR_BEST();
	}
	RE_FILL();

    default:
	RE_bad_state("REmatch", m, u_flag);
    }
    return NULL;
}
#undef rt_push
