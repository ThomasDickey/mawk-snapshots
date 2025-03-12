/********************************************
rexp1.c
copyright 2009-2023,2024, Thomas E. Dickey
copyright 1991,1993, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: rexp1.c,v 1.31 2024/12/30 15:46:23 tom Exp $
 */

/*  re machine	operations  */

#include  <rexp.h>

/* initialize a two state machine */
static void
new_TWO(
	   int type,
	   MACHINE * mp)	/* init mp-> */
{
    mp->start = (STATE *) RE_malloc(2 * STATESZ);
    mp->stop = mp->start + 1;
    mp->start->s_type = (SType) type;
    mp->stop->s_type = M_ACCEPT;
#ifndef NO_INTERVAL_EXPR
    mp->start->it_min = 1;
    mp->start->it_max = MAX__INT;
    mp->start->it_cnt = 0;
#endif
}

/*  build a machine that recognizes any	 */
MACHINE
RE_any(void)
{
    MACHINE x;

    new_TWO(M_ANY, &x);
    return x;
}

/*  build a machine that recognizes the start of string	 */
MACHINE
RE_start(void)
{
    MACHINE x;

    new_TWO(M_START, &x);
    return x;
}

MACHINE
RE_end(void)
{
    MACHINE x;

    new_TWO(M_END, &x);
    return x;
}

/*  build a machine that recognizes a class  */
MACHINE
RE_class(BV * bvp)
{
    MACHINE x;

    new_TWO(M_CLASS, &x);
    x.start->s_data.bvp = bvp;
    return x;
}

MACHINE
RE_u(void)
{
    MACHINE x;

    new_TWO(M_U, &x);
    return x;
}

MACHINE
RE_str(char *str, size_t len)
{
    MACHINE x;

    new_TWO(M_STR, &x);
    x.start->s_len = len;
    x.start->s_data.str = str;
    return x;
}

/*  replace m and n by a machine that recognizes  mn   */
void
RE_cat(MACHINE * mp, MACHINE * np)
{
    size_t sz1, sz2, sz;

    sz1 = (size_t) (mp->stop - mp->start);
    sz2 = (size_t) (np->stop - np->start + 1);
    sz = sz1 + sz2;
#ifndef NO_INTERVAL_EXPR
    ++sz;			/* allocate dummy for workaround */
#endif
    mp->start = (STATE *) RE_realloc(mp->start, sz * STATESZ);
#ifndef NO_INTERVAL_EXPR
    --sz;
#endif
    mp->stop = mp->start + (sz - 1);
    RE_copy_states(mp->start + sz1, np->start, sz2);
    RE_free(np->start);
}

 /*  replace m by a machine that recognizes m|n  */

void
RE_or(MACHINE * mp, MACHINE * np)
{
    register STATE *p;
    size_t szm, szn;

    szm = (size_t) (mp->stop - mp->start + 1);
    szn = (size_t) (np->stop - np->start + 1);

    p = (STATE *) RE_malloc((szm + szn + 1) * STATESZ);
    RE_copy_states(p + 1, mp->start, szm);
    RE_free(mp->start);
    mp->start = p;
    (mp->stop = p + szm + szn)->s_type = M_ACCEPT;
    p->s_type = M_2JA;
    p->s_data.jump = (int) (szm + 1);
    RE_copy_states(p + szm + 1, np->start, szn);
    RE_free(np->start);
    (p += szm)->s_type = M_1J;
    p->s_data.jump = (int) szn;
}

#ifndef NO_INTERVAL_EXPR
#define is_LOOP_TYPE(type) ((type) == M_2JC || (type) == M_LOOP)
#else
#define is_LOOP_TYPE(type) ((type) == M_2JC)
#endif

/*
 * Ignore attempts to wrap an atom using zero-or-more repetitions in another
 * loop with the same condition.
 */
#define ignore_star_star(mp, sz) \
{ \
    if (sz == 5) { \
	STATE *ps = mp->start; \
 \
	if ((ps->s_type % U_ON) != M_2JA) { \
	    ; \
	} else if (ps->s_data.jump != 4) { \
	    TRACE((".. expected jump %d\n", ps->s_data.jump)); \
	} else if (((ps + 1)->s_type % U_ON) != M_SAVE_POS) { \
	    TRACE((".. expected save %s\n", REs_type(ps + 1))); \
	} else if (((ps + 2)->s_type % U_ON) != M_CLASS && \
		   ((ps + 2)->s_type % U_ON) != M_STR && \
		   ((ps + 2)->s_type % U_ON) != M_U) { \
	    TRACE((".. expected atom %s\n", REs_type(ps + 2))); \
	} else if (is_LOOP_TYPE((ps + 3)->s_type)) { \
	    TRACE(("ignore repeated loop\n")); \
	} else { \
	    TRACE((".. expected loop %s\n", REs_type(ps + 3))); \
	    return NULL; \
	} \
    } \
}

/*  UNARY  OPERATIONS	  */

/*  replace m by m*  (zero or more) */

STATE *
RE_close(MACHINE * mp)
{
    register STATE *p;
    size_t sz;

    /*
     *          2JA end
     * loop:
     *          SAVE_POS
     *          m
     *          2JC loop
     * end:
     *          ACCEPT
     */
    sz = (size_t) (mp->stop - mp->start + 1);
    ignore_star_star(mp, sz);

    p = (STATE *) RE_malloc((sz + 3) * STATESZ);
    RE_copy_states(p + 2, mp->start, sz);
    RE_free(mp->start);
    mp->start = p;
    mp->stop = p + (sz + 2);
    p->s_type = M_2JA;
    p->s_data.jump = (int) (sz + 2);
    (++p)->s_type = M_SAVE_POS;
    (p += sz)->s_type = M_2JC;
#ifndef NO_INTERVAL_EXPR
    p->it_min = 1;
    p->it_max = MAX__INT;
#endif
    p->s_data.jump = -(int) sz;
    (p + 1)->s_type = M_ACCEPT;

    return p;
}

/*  replace m  by  m+  (positive closure - one or more)  */

STATE *
RE_poscl(MACHINE * mp)
{
    register STATE *p;
    size_t sz;

    /*
     * loop:
     *          SAVE_POS
     *          m
     *          2JC loop
     *          ACCEPT
     */
    sz = (size_t) (mp->stop - mp->start + 1);
    ignore_star_star(mp, sz);

    p = (STATE *) RE_malloc((sz + 2) * STATESZ);
    RE_copy_states(p + 1, mp->start, sz);
    RE_free(mp->start);
    mp->start = p;
    mp->stop = p + (sz + 1);
    p++->s_type = M_SAVE_POS;
    p += sz - 1;
    p->s_type = M_2JC;
#ifndef NO_INTERVAL_EXPR
    p->it_min = 1;
    p->it_max = MAX__INT;
#endif
    p->s_data.jump = -((int) sz);
    (p + 1)->s_type = M_ACCEPT;

    return p;
}

/* replace  m  by  m? (zero or one)  */

void
RE_01(MACHINE * mp)
{
    size_t sz;
    register STATE *p;

    /*
     *          2JB end (jump desirable if not found)
     *          m
     * end:
     *          ACCEPT
     */
    sz = (size_t) (mp->stop - mp->start + 1);
    p = (STATE *) RE_malloc((sz + 1) * STATESZ);
    RE_copy_states(p + 1, mp->start, sz);
    RE_free(mp->start);
    mp->start = p;
    mp->stop = p + sz;
    p->s_type = M_2JB;
    p->s_data.jump = (int) sz;
}

/*===================================
MEMORY	ALLOCATION
 *==============================*/

PTR
RE_malloc(size_t sz)
{
    PTR p;

    p = malloc(sz);
    TRACE(("RE_malloc(%lu) ->%p\n", (unsigned long) sz, p));
    if (p == NULL)
	RE_error_trap(MEMORY_FAILURE);
#ifdef OPT_TRACE
    memset(p, 0, sz);
#endif
    return p;
}

PTR
RE_realloc(PTR p, size_t sz)
{
    PTR q;

    TRACE(("RE_realloc(%p, %lu) ", p, (unsigned long) sz));
    q = realloc(p, sz);
    TRACE(("->%p\n", q));
    if (q == NULL)
	RE_error_trap(MEMORY_FAILURE);
    return q;
}

#ifdef NO_LEAKS
void
RE_free(PTR p)
{
    TRACE(("RE_free(%p)\n", p));
    free(p);
}

/* when copying states, ensure strings are new copies so we can destroy ok */
void
RE_copy_states(STATE * target, const STATE * source, size_t length)
{
    size_t n;

    for (n = 0; n < length; ++n) {
	memcpy(target + n, source + n, STATESZ);
	switch (source[n].s_type) {
	case M_STR:
	    target[n].s_data.str = strdup(target[n].s_data.str);
	    break;
	}
    }
}
#endif
