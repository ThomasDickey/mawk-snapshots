/********************************************
rexp2.c
copyright 2009-2024,2025, Thomas E. Dickey
copyright 2010, Jonathan Nieder
copyright 1991-1992,1993, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: rexp2.c,v 1.53 2025/01/22 00:44:07 tom Exp $
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
#if OPT_TRACE
    memset(RE_pos_stack_base + oldsize, 0,
	   (newsize - oldsize) * sizeof(RT_POS_ENTRY));
#endif

    RE_pos_stack_limit = RE_pos_stack_base + newsize;
    RE_pos_stack_empty = RE_pos_stack_base;

    return RE_pos_stack_base + oldsize;
}

#define rt_push(mx,sx,px,ux) do { \
	if (++run_entry == RE_run_stack_limit) \
		run_entry = RE_new_run_stack(); \
	run_entry->m = (mx); \
	run_entry->s = (sx); \
	run_entry->pos_index = (int) ((px) - RE_pos_stack_base); \
	run_entry->top_index = (px)->prev_offset; \
	run_entry->u = (ux); \
	TRACE2((rt_form "rt_push %s pos@%d top@%d\n", rt_args, \
		REs_type(mx), \
		run_entry->pos_index, \
		run_entry->top_index)); \
} while(0)

#define rt_pop() do { \
	TRACE2((rt_form "rt_pop\n", rt_args)); \
	run_entry--; \
} while (0)

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
    const char *old_s;
    register RT_STATE *run_entry;
    int u_flag;
    char *str_end = str + len;
    RT_POS_ENTRY *pos_entry;
    int ti;			/*convenient temps */
    STATE *tm;
#ifndef NO_INTERVAL_EXPR
    STATE *loop_stack[MAX_LOOP_LEVEL + 1];
    int loop_depth = 0;
#endif

    TRACE(("REtest: \"%s\" ~ /pattern/\n", str));

    /* handle the easy case quickly */
    if (m->s_type == M_STR && (m + 1)->s_type == M_ACCEPT) {
	TRACE(("returning str_str\n"));
	return str_str(s, len, m->s_data.str, m->s_len) != (char *) 0;
    } else {
	u_flag = U_ON;
	run_entry = RE_run_stack_empty;
	pos_entry = RE_pos_stack_empty;
	if_TRACE(memset(pos_entry, 0, 2 * sizeof(*pos_entry)));
	RE_CASE();
    }

  refill:
#ifndef NO_INTERVAL_EXPR
    if (run_entry != RE_run_stack_empty) {
#if OPT_TRACE > 1
	RT_STATE *statep;
	RT_POS_ENTRY *posp;

	for (statep = RE_run_stack_base; statep <= run_entry; ++statep) {
	    TRACE(("check - STATE %d: m %03d s \"%s\" pos@%d top@%d u %d\n",
		   (int) (statep - RE_run_stack_base),
		   (int) (statep->m - machine),
		   NonNull(statep->s),
		   statep->pos_index,
		   statep->top_index,
		   statep->u));
	}
	for (posp = RE_pos_stack_base; posp <= pos_entry; ++posp) {
	    TRACE(("check - POS %d: pos \"%s\" owner@%d prev@%d\n",
		   (int) (posp - RE_pos_stack_base),
		   NonNull(posp->pos),
		   posp->owner,
		   posp->prev_offset));
	}
#endif
	/*
	 * We're here because we had a mismatch in a loop. Find the end of the
	 * loop, and reset it if the mismatch was due to too-few matches.
	 */
	if (loop_depth > 0) {
	    STATE *enter = loop_stack[loop_depth - 1];
	    STATE *loop = enter + enter->s_data.jump;

	    TRACE2(("Found M_LOOP: %03d at loop level %d\n",
		    (int) (loop - machine), loop_depth));
	    TRACE2(("currently " INT_FMT " [" INT_FMT ".." INT_FMT "]\n",
		    loop->it_cnt, loop->it_min, loop->it_max));
	    if ((loop->it_cnt < loop->it_min) && (enter < run_entry->m)) {
		TRACE2(("too few - invoke M_ENTER\n"));
		TRACE2(("switch from %03ld to %03ld\n",
			run_entry->m - machine,
			enter - machine));
		run_entry->m = enter;
	    } else if (run_entry->m > loop) {
		loop_stack[--loop_depth] = NULL;
		TRACE2(("fall out of loop %03ld %03ld, now level %d\n",
			enter - machine,
			loop - machine,
			loop_depth));
	    }
	}
    }
#endif
    if (run_entry == RE_run_stack_empty) {
	TR_AT("accept failure");
	return 0;
    }
    m = run_entry->m;
    s = run_entry->s;
    pos_entry = RE_pos_stack_base + run_entry->pos_index;
    pos_entry->prev_offset = run_entry->top_index;
    u_flag = run_entry->u;
    rt_pop();

  reswitch:
    TRACE((rt_form "%-8s %-15s: \"%s\"\n", rt_args,
	   REs_type(m),
	   RE_u_end(u_flag),
	   s));

    switch (m->s_type + u_flag) {
    case M_STR + U_OFF + END_OFF:
	if (s > str_end
	    || (size_t) (str_end - s) < m->s_len
	    || memcmp(s, m->s_data.str, m->s_len)) {
	    TR_AT("no match");
	    RE_FILL();
	}
	s += m->s_len;
	m++;
	TR_AT("match");
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
	s = str_str(s, (size_t) (str_end - s), m->s_data.str, m->s_len);
	if (s == NULL) {
	    RE_FILL();
	}
	rt_push(m, s + 1, pos_entry, U_ON);
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
	rt_push(m, s, pos_entry, U_ON);
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

    case M_ANY + U_OFF + END_ON:
	/* NOTREACHED */
	if (s >= str_end || (s + 1) < str_end) {
	    RE_FILL();
	}
	s++;
	m++;
	RE_CASE();

    case M_ANY + U_ON + END_OFF:
	if (s >= str_end) {
	    RE_FILL();
	}
	s++;
	rt_push(m, s, pos_entry, U_ON);
	m++;
	u_flag = U_OFF;
	RE_CASE();

    case M_ANY + U_ON + END_ON:
	/* NOTREACHED */
	if (s >= str_end) {
	    RE_FILL();
	}
	s = str_end;
	m++;
	u_flag = U_OFF;
	RE_CASE();

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
	pos_push(pos_entry, run_entry, s);
	m++;
	RE_CASE();

      CASE_UANY(M_2JA):	/* take the non jump branch */
	/* don't stack an ACCEPT */
	if ((tm = m + m->s_data.jump)->s_type == M_ACCEPT) {
	    TR_AT("accept success");
	    return 1;
	}
	rt_push(tm, s, pos_entry, u_flag);
	m++;
	RE_CASE();

#ifndef NO_INTERVAL_EXPR
      CASE_UANY(M_ENTER):	/* take the jump branch if position changed */
	TRACE(("reset loop " INT_FMT " [" INT_FMT ".." INT_FMT "]\n",
	       m->it_cnt, m->it_min, m->it_max));
	(m + m->s_data.jump)->it_cnt = 0;
	/*
	 * This begins an iteration by resetting the counter.  Save a pointer
	 * to the corresponding M_LOOP to help with recovery on mismatches.
	 */
	TRACE2(("checking loop_depth " INT_FMT " vs %d\n",
		m->it_cnt, loop_depth));
	if (m->it_cnt != loop_depth) {
	    loop_depth = (int) (m->it_cnt - 1);
	    if (loop_depth >= MAX_LOOP_LEVEL)
		bozo("excessive loop-level");
	    loop_stack[loop_depth++] = m;
	}
	m++;
	RE_CASE();

      CASE_UANY(M_LOOP):	/* take the jump branch if position changed */
	m->it_cnt++;
	TRACE(("checking loop " INT_FMT " [" INT_FMT ".." INT_FMT "]\n",
	       m->it_cnt, m->it_min, m->it_max));
	if (m->it_max < MAX__INT && m->it_cnt >= m->it_max) {
	    m++;
	    TR_AT("past maximum for M_LOOP");
	    RE_CASE();		/* test the next thing */
	}
	goto fall_through;	/* workaround for gcc bug */
      fall_through:
	/* FALLTHRU */
#endif /* ! NO_INTERVAL_EXPR */

      CASE_UANY(M_2JC):	/* take the jump branch if position changed */
	pos_pop(pos_entry, run_entry, old_s);
	if (old_s == s) {
	    /* did not advance: do not jump back */
	    m++;
	    RE_CASE();
	}
	/* don't stack an ACCEPT */
	if ((tm = m + 1)->s_type == M_ACCEPT) {
	    TR_AT("accept success");
	    return 1;
	}
	rt_push(tm, s, pos_entry, u_flag);
	m += m->s_data.jump;
	RE_CASE();

      CASE_UANY(M_2JB):
	/* don't stack an ACCEPT */
	if ((tm = m + 1)->s_type == M_ACCEPT) {
	    TR_AT("accept success");
	    return 1;
	}
	rt_push(tm, s, pos_entry, u_flag);
	m += m->s_data.jump;
	RE_CASE();

      CASE_UANY(M_ACCEPT):
	TR_AT("accept success");
	return 1;

    default:
	RE_bad_state("REtest", m, u_flag);
    }
    return 0;
}

#undef rt_push

#include <field.h>

char *
is_string_split(PTR q, size_t *lenp)
{
    STATE *p = cast_to_re(q);

    if (p != NULL && (p[0].s_type == M_STR && p[1].s_type == M_ACCEPT)) {
	*lenp = p->s_len;
	return p->s_data.str;
    } else
	return (char *) 0;
}
