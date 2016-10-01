/********************************************
rexp1.c
copyright 2009-2010,2016, Thomas E. Dickey
copyright 1991,1993, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: rexp1.c,v 1.16 2016/09/30 21:11:29 tom Exp $
 */

/*  re machine	operations  */

#include  "rexp.h"

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
    x.start->s_len = (SLen) len;
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

    mp->start = (STATE *) RE_realloc(mp->start, sz * STATESZ);
    mp->stop = mp->start + (sz - 1);
    memcpy(mp->start + sz1, np->start, sz2 * STATESZ);
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
    memcpy(p + 1, mp->start, szm * STATESZ);
    RE_free(mp->start);
    mp->start = p;
    (mp->stop = p + szm + szn)->s_type = M_ACCEPT;
    p->s_type = M_2JA;
    p->s_data.jump = (int) (szm + 1);
    memcpy(p + szm + 1, np->start, szn * STATESZ);
    RE_free(np->start);
    (p += szm)->s_type = M_1J;
    p->s_data.jump = (int) szn;
}

/*
 * Ignore attempts to wrap an atom using zero-or-more repetitions in another
 * loop with the same condition.
 */
static int
ignore_star_star(MACHINE * mp)
{
    size_t sz = (size_t) (mp->stop - mp->start + 1);

    if (sz >= 4) {
	STATE *p = mp->start;
	STATE *q = mp->stop;

	if ((p->s_type % U_ON) != M_2JA) {
	    ;
	} else if (p->s_data.jump != 4) {
	    TRACE((".. expected jump %d\n", p->s_data.jump));
	} else if (((p + 1)->s_type % U_ON) != M_SAVE_POS) {
	    TRACE((".. expected save %s\n", REs_type(p + 1)));
	} else if (((q - 2)->s_type % U_ON) != M_CLASS &&
		   ((q - 2)->s_type % U_ON) != M_STR &&
		   ((q - 2)->s_type % U_ON) != M_U) {
	    TRACE((".. expected atom %s\n", REs_type(q - 2)));
	} else if (((q - 1)->s_type % U_ON) != M_2JC) {
	    TRACE(("ignored loop %s\n", REs_type(q - 1)));
	} else {
	    TRACE(("ignore repeated loop\n"));
	    return 1;
	}
    }
    return 0;
}

/*  UNARY  OPERATIONS	  */

/*  replace m by m*   */

void
RE_close(MACHINE * mp)
{
    register STATE *p;
    size_t sz;

    if (ignore_star_star(mp))
	return;
    /*
     *                2JA end
     * loop:
     *          SAVE_POS
     *          m
     *          2JC loop
     * end:
     *          ACCEPT
     */
    sz = (size_t) (mp->stop - mp->start + 1);
    p = (STATE *) RE_malloc((sz + 3) * STATESZ);
    memcpy(p + 2, mp->start, sz * STATESZ);
    RE_free(mp->start);
    mp->start = p;
    mp->stop = p + (sz + 2);
    p->s_type = M_2JA;
    p->s_data.jump = (int) (sz + 2);
    (++p)->s_type = M_SAVE_POS;
    (p += sz)->s_type = M_2JC;
    p->s_data.jump = -(int) sz;
    (p + 1)->s_type = M_ACCEPT;
}

/*  replace m  by  m+  (positive closure)   */

void
RE_poscl(MACHINE * mp)
{
    register STATE *p;
    size_t sz;

    if (ignore_star_star(mp))
	return;
    /*
     * loop:
     *          SAVE_POS
     *          m
     *          2JC loop
     *          ACCEPT
     */
    sz = (size_t) (mp->stop - mp->start + 1);
    p = (STATE *) RE_malloc((sz + 2) * STATESZ);
    memcpy(p + 1, mp->start, sz * STATESZ);
    RE_free(mp->start);
    mp->start = p;
    mp->stop = p + (sz + 1);
    p++->s_type = M_SAVE_POS;
    p += sz - 1;
    p->s_type = M_2JC;
    p->s_data.jump = -((int) sz);
    (p + 1)->s_type = M_ACCEPT;
}

/* replace  m  by  m? (zero or one)  */

void
RE_01(MACHINE * mp)
{
    size_t sz;
    register STATE *p;

    sz = (size_t) (mp->stop - mp->start + 1);
    p = (STATE *) RE_malloc((sz + 1) * STATESZ);
    memcpy(p + 1, mp->start, sz * STATESZ);
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
    if (p == 0)
	RE_error_trap(MEMORY_FAILURE);
    return p;
}

PTR
RE_realloc(PTR p, size_t sz)
{
    PTR q;

    q = realloc(p, sz);
    TRACE(("RE_realloc(%p, %lu) ->%p\n", p, (unsigned long) sz, q));
    if (q == 0)
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
#endif
