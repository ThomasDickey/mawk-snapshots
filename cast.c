/********************************************
cast.c
copyright 2009-2016,2020, Thomas E. Dickey
copyright 1991-1995,1996, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: cast.c,v 1.26 2020/07/29 00:23:15 tom Exp $
 */

/*  cast.c  */

#include "mawk.h"
#include "field.h"
#include "memory.h"
#include "scan.h"
#include "repl.h"

const int mpow2[NUM_CELL_TYPES] =
{1, 2, 4, 8, 16, 32, 64, 128, 256, 512};

void
cast1_to_d(CELL *cp)
{
    switch (cp->type) {
    case C_NOINIT:
	cp->dval = 0.0;
	break;

    case C_DOUBLE:
	return;

    case C_MBSTRN:
    case C_STRING:
	{
	    register STRING *s = (STRING *) cp->ptr;

	    errno = 0;
#ifdef FPE_TRAPS_ON		/* look for overflow error */
	    cp->dval = strtod(s->str, (char **) 0);
	    if (errno && cp->dval != 0.0)	/* ignore underflow */
		rt_error("overflow converting %s to double", s->str);
#else
	    cp->dval = strtod(s->str, (char **) 0);
#endif
	    free_STRING(s);
	}
	break;

    case C_STRNUM:
	/* don't need to convert, but do need to free the STRING part */
	free_STRING(string(cp));
	break;

    default:
	bozo("cast on bad type");
    }
    cp->type = C_DOUBLE;
}

void
cast2_to_d(CELL *cp)
{
    register STRING *s;

    switch (cp->type) {
    case C_NOINIT:
	cp->dval = 0.0;
	break;

    case C_DOUBLE:
	goto two;
    case C_STRNUM:
	free_STRING(string(cp));
	break;

    case C_MBSTRN:
    case C_STRING:
	s = (STRING *) cp->ptr;

	errno = 0;
#ifdef FPE_TRAPS_ON		/* look for overflow error */
	cp->dval = strtod(s->str, (char **) 0);
	if (errno && cp->dval != 0.0)	/* ignore underflow */
	    rt_error("overflow converting %s to double", s->str);
#else
	cp->dval = strtod(s->str, (char **) 0);
#endif
	free_STRING(s);
	break;

    default:
	bozo("cast on bad type");
    }
    cp->type = C_DOUBLE;

  two:
    cp++;

    switch (cp->type) {
    case C_NOINIT:
	cp->dval = 0.0;
	break;

    case C_DOUBLE:
	return;
    case C_STRNUM:
	free_STRING(string(cp));
	break;

    case C_MBSTRN:
    case C_STRING:
	s = (STRING *) cp->ptr;

	errno = 0;
#ifdef FPE_TRAPS_ON		/* look for overflow error */
	cp->dval = strtod(s->str, (char **) 0);
	if (errno && cp->dval != 0.0)	/* ignore underflow */
	    rt_error("overflow converting %s to double", s->str);
#else
	cp->dval = strtod(s->str, (char **) 0);
#endif
	free_STRING(s);
	break;

    default:
	bozo("cast on bad type");
    }
    cp->type = C_DOUBLE;
}

void
cast1_to_s(CELL *cp)
{
    register Long lval;
    char xbuff[260];

    switch (cp->type) {
    case C_NOINIT:
	null_str.ref_cnt++;
	cp->ptr = (PTR) & null_str;
	break;

    case C_DOUBLE:

	lval = d_to_L(cp->dval);
	if (lval == cp->dval)
	    sprintf(xbuff, LONG_FMT, lval);
	else
	    sprintf(xbuff, string(CONVFMT)->str, cp->dval);

	cp->ptr = (PTR) new_STRING(xbuff);
	break;

    case C_STRING:
	return;

    case C_MBSTRN:
    case C_STRNUM:
	break;

    default:
	bozo("bad type on cast");
    }
    cp->type = C_STRING;
}

void
cast2_to_s(CELL *cp)
{
    register Long lval;
    char xbuff[260];

    switch (cp->type) {
    case C_NOINIT:
	null_str.ref_cnt++;
	cp->ptr = (PTR) & null_str;
	break;

    case C_DOUBLE:

	lval = d_to_L(cp->dval);
	if (lval == cp->dval)
	    sprintf(xbuff, LONG_FMT, lval);
	else
	    sprintf(xbuff, string(CONVFMT)->str, cp->dval);

	cp->ptr = (PTR) new_STRING(xbuff);
	break;

    case C_STRING:
	goto two;

    case C_MBSTRN:
    case C_STRNUM:
	break;

    default:
	bozo("bad type on cast");
    }
    cp->type = C_STRING;

  two:
    cp++;

    switch (cp->type) {
    case C_NOINIT:
	null_str.ref_cnt++;
	cp->ptr = (PTR) & null_str;
	break;

    case C_DOUBLE:

	lval = d_to_L(cp->dval);
	if (lval == cp->dval)
	    sprintf(xbuff, LONG_FMT, lval);
	else
	    sprintf(xbuff, string(CONVFMT)->str, cp->dval);

	cp->ptr = (PTR) new_STRING(xbuff);
	break;

    case C_STRING:
	return;

    case C_MBSTRN:
    case C_STRNUM:
	break;

    default:
	bozo("bad type on cast");
    }
    cp->type = C_STRING;
}

void
cast_to_RE(CELL *cp)
{
    register PTR p;

    if (cp->type < C_STRING)
	cast1_to_s(cp);

    p = re_compile(string(cp));
    no_leaks_re_ptr(p);
    free_STRING(string(cp));
    cp->type = C_RE;
    cp->ptr = p;

}

void
cast_for_split(CELL *cp)
{
#ifndef NO_INTERVAL_EXPR
    static const char meta[] = "^$.*+?|[](){}";
#else
    static const char meta[] = "^$.*+?|[]()";
#endif
    static char xbuff[] = "\\X";
    int c;
    size_t len;

    if (cp->type < C_STRING)
	cast1_to_s(cp);

    if ((len = string(cp)->len) == 1) {
	if ((c = string(cp)->str[0]) == ' ') {
	    free_STRING(string(cp));
	    cp->type = C_SPACE;
	    return;
	} else if (c == 0) {
#ifdef LOCAL_REGEXP
	    char temp[1];
	    temp[0] = (char) c;
	    free_STRING(string(cp));
	    cp->ptr = (PTR) new_STRING1(temp, (size_t) 1);
#else
	    /*
	     * A null is not a meta character, but strchr will match it anyway.
	     * For now, there's no reason to compile a null as a regular
	     * expression - just return a string containing the single
	     * character.  That is used in a special case in set_rs_shadow().
	     */
	    char temp[2];
	    temp[0] = (char) c;
	    free_STRING(string(cp));
	    cp->ptr = (PTR) new_STRING1(temp, (size_t) 1);
	    return;
#endif
	} else if ((strchr) (meta, c)) {
	    xbuff[1] = (char) c;
	    free_STRING(string(cp));
	    cp->ptr = (PTR) new_STRING(xbuff);
	}
    } else if (len == 0) {
	free_STRING(string(cp));
	cp->type = C_SNULL;
	return;
    }

    cast_to_RE(cp);
}

/* input: cp-> a CELL of type C_MBSTRN (maybe strnum)
   test it -- casting it to the appropriate type
   which is C_STRING or C_STRNUM
*/

void
check_strnum(CELL *cp)
{
    char *temp;
    register unsigned char *s, *q;

    cp->type = C_STRING;	/* assume not C_STRNUM */
    s = (unsigned char *) string(cp)->str;
    q = s + string(cp)->len;
    while (scan_code[*s] == SC_SPACE)
	s++;
    if (s == q)
	return;

    while (scan_code[q[-1]] == SC_SPACE)
	q--;
    if (scan_code[q[-1]] != SC_DIGIT &&
	q[-1] != '.')
	return;

    switch (scan_code[*s]) {
    case SC_DIGIT:
    case SC_PLUS:
    case SC_MINUS:
    case SC_DOT:

	errno = 0;
#ifdef FPE_TRAPS_ON
	cp->dval = strtod((char *) s, &temp);
	/* make overflow pure string */
	if (errno && cp->dval != 0.0)
	    break;
#else
	cp->dval = strtod((char *) s, &temp);
#endif
#ifdef ERANGE
	if (errno == ERANGE)
	    break;
#endif

	if ((char *) q <= temp) {
	    cp->type = C_STRNUM;
	    /*  <= instead of == , for some buggy strtod
	       e.g. Apple Unix */
	}
	break;
    }
}

/* cast a CELL to a replacement cell */

void
cast_to_REPL(CELL *cp)
{
    register STRING *sval;

    if (cp->type < C_STRING)
	cast1_to_s(cp);
    sval = (STRING *) cp->ptr;

    cellcpy(cp, repl_compile(sval));
    free_STRING(sval);
}

/* convert a double to Int (this is not as simple as a
   cast because the results are undefined if it won't fit).
   Truncate large values to +Max_Int or -Max_Int
   Send nans to -Max_Int
*/

Int
d_to_I(double d)
{
    Int result;

    if (d >= Max_Int) {
	result = Max_Int;
    } else if (d < 0) {
	if (-d <= Max_Int) {
	    result = (Int) d;
	} else {
	    result = -Max_Int;
	}
    } else {
	result = (Int) d;
    }
    return result;
}

Long
d_to_L(double d)
{
    Long result;

    if (d >= Max_Long) {
	result = Max_Long;
    } else if (d < 0) {
	if (-d <= Max_Long) {
	    result = (Long) d;
	} else {
	    result = -Max_Long;
	}
    } else {
	result = (Long) d;
    }
    return result;
}

ULong
d_to_UL(double d)
{
    ULong result;

    if (d >= Max_ULong) {
	result = Max_ULong;
    } else if (d < 0) {
	if (-d < Max_ULong) {
	    result = ((Max_ULong + (ULong) d) + 1);
	} else {
	    result = -Max_ULong;
	}
    } else {
	result = (ULong) d;
    }
    return result;
}

#ifdef NO_LEAKS
typedef struct _all_cells {
    struct _all_cells *next;
    char ptr;
    CELL *cp;
} ALL_CELLS;

static ALL_CELLS *all_cells;
/*
 * Some regular expressions are parsed, and the pointer stored in the byte-code
 * where we cannot distinguish it from other constants.  Keep a list here, to
 * free on exit for auditing.
 */
void
no_leaks_cell(CELL *cp)
{
    ALL_CELLS *p = calloc(1, sizeof(ALL_CELLS));
    p->next = all_cells;
    p->cp = cp;
    p->ptr = 0;
    all_cells = p;
}

void
no_leaks_cell_ptr(CELL *cp)
{
    ALL_CELLS *p = calloc(1, sizeof(ALL_CELLS));
    p->next = all_cells;
    p->cp = cp;
    p->ptr = 1;
    all_cells = p;
}

void
cell_leaks(void)
{
    while (all_cells != 0) {
	ALL_CELLS *next = all_cells->next;
	if (all_cells->ptr) {
	    zfree(all_cells->cp, sizeof(CELL));
	} else {
	    free_cell_data(all_cells->cp);
	}
	free(all_cells);
	all_cells = next;
    }
}
#endif
