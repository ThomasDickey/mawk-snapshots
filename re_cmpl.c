/********************************************
re_cmpl.c
copyright 2008-2023,2024, Thomas E. Dickey
copyright 1991-1994,2014, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: re_cmpl.c,v 1.43 2024/12/14 21:21:20 tom Exp $
 */

#define Visible_CELL
#define Visible_RE_DATA
#define Visible_RE_NODE
#define Visible_STRING

#include <mawk.h>
#include <memory.h>
#include <scan.h>
#include <regexp.h>

/* a list of compiled regular expressions */
static RE_NODE *re_list;

static const char efmt[] = "regular expression compile failed (%s)\n%s";

/* compile a STRING to a regular expression machine.
   Search a list of pre-compiled strings first
*/
RE_NODE *
re_compile(STRING * sval)
{
    register RE_NODE *p;
    RE_NODE *q;
    char *s;

    /* search list */
    s = sval->str;
    p = re_list;
    q = (RE_NODE *) 0;

    while (p) {
	if (sval->len == p->sval->len
	    && memcmp(s, p->sval->str, sval->len) == 0) {
	    /* found */
	    if (!q)		/* already at front */
		goto _return;
	    else {		/* delete from list for move to front */
		q->link = p->link;
		goto found;
	    }
	} else {
	    q = p;
	    p = p->link;
	}
    }

    /* not found */
    p = ZMALLOC(RE_NODE);
    p->sval = sval;

    sval->ref_cnt++;
    p->re.anchored = (*s == '^');
    p->re.is_empty = (sval->len == 0);
    if (!(p->re.compiled = REcompile(s, sval->len))) {
	ZFREE(p);
	sval->ref_cnt--;
	if (mawk_state == EXECUTION) {
	    rt_error(efmt, REerror(), safe_string(s));
	} else {		/* compiling */
	    char *safe = safe_string(s);
	    compile_error(efmt, REerror(), safe);
	    free(safe);
	    return (PTR) 0;
	}
    }

  found:
    /* insert p at the front of the list */
    p->link = re_list;
    re_list = p;

  _return:

#ifdef DEBUG
    if (dump_RE)
	REmprint(p->re.compiled, stderr);
#endif
    return p;
}

/* this is only used by da() */

STRING *
re_uncompile(PTR m)
{
    register RE_NODE *p;

    for (p = re_list; p; p = p->link)
	if (p->re.compiled == cast_to_re(m))
	    return p->sval;
#ifdef DEBUG
    bozo("non compiled machine");
#else
    return NULL;
#endif
}

#ifdef NO_LEAKS
void
re_destroy(PTR m)
{
    RE_NODE *p = (RE_NODE *) m;
    RE_NODE *q;
    RE_NODE *r;

    if (p != NULL) {
	for (q = re_list, r = NULL; q != NULL; r = q, q = q->link) {
	    if (q == p) {
		free_STRING(p->sval);
		REdestroy(p->re.compiled);
		if (r != NULL)
		    r->link = q->link;
		else
		    re_list = q->link;
		free(q);
		break;
	    }
	}
    }
}
#endif

/*=================================================*/
/*  replacement	 operations   */

/* create a replacement CELL from a STRING *  */

/* Here the replacement string gets scanned for &
 * which calls for using the matched text in the replacement
 * So to get a literal & in the replacement requires a convention
 * which is \& is literal & not a matched text &.
 * Here are the Posix rules which this code supports:
           \&   -->   &
	   \\   -->   \
	   \c   -->   \c
	   &    -->   matched text

*/

/* FIXME  -- this function doesn't handle embedded nulls
   split_buff is obsolete, but needed by this function.
   Putting them here is temporary until the rewrite to handle nulls.
*/

#define SPLIT_SIZE  256

static STRING **split_buff;
static size_t split_size;

static CELL *
REPL_compile(STRING * sval)
{
    VCount count = 0;
    register char *p = sval->str;
    register char *q;
    register char *r;
    char *xbuff;
    CELL *cp;
    size_t limit = sval->len + 1;

    if (limit > split_size) {
	size_t new_size = limit + SPLIT_SIZE;
	if (split_buff != NULL) {
	    size_t old_size = split_size;
	    split_buff = (STRING **) zrealloc(split_buff,
					      old_size * sizeof(STRING *),
					      new_size * sizeof(STRING *));
	} else {
	    split_buff = (STRING **) zmalloc(new_size * sizeof(STRING *));
	}
	split_size = new_size;
    }

    q = xbuff = (char *) zmalloc(limit);

    while (1) {
	switch (*p) {
	case 0:
	    *q = 0;
	    goto done;

	case '\\':
	    if (p[1] == '\\') {
		int merge = 0;
		for (r = p + 2; *r; ++r) {
		    if (r[0] == '&') {
			/* gotcha! */
			merge = 1;
		    } else if (r[0] != '\\') {
			/* give up: do not alter */
			break;
		    }
		}
		if (merge) {
		    *q++ = p[1];
		    p += 2;
		} else {
		    *q++ = *p++;
		    *q++ = *p++;
		}
		continue;
	    } else if (p[1] == '&') {
		*q++ = p[1];
		p += 2;
		continue;
	    } else
		break;

	case '&':
	    /* if empty we don't need to make a node */
	    if (q != xbuff) {
		*q = 0;
		split_buff[count++] = new_STRING(xbuff);
	    }
	    /* and a null node for the '&'  */
	    split_buff[count++] = (STRING *) 0;
	    /*  reset  */
	    p++;
	    q = xbuff;
	    continue;

	default:
	    break;
	}

	*q++ = *p++;
    }

  done:
    /* if we have one empty string it will get made now */
    if (q > xbuff || count == 0)
	split_buff[count++] = new_STRING(xbuff);

    cp = ZMALLOC(CELL);
    if (count == 1 && split_buff[0]) {
	cp->type = C_REPL;
	cp->ptr = (PTR) split_buff[0];
	USED_SPLIT_BUFF(0);
    } else {
	STRING **sp = (STRING **)
	(cp->ptr = zmalloc(sizeof(STRING *) * count));
	VCount j = 0;

	while (j < count) {
	    *sp++ = split_buff[j++];
	    USED_SPLIT_BUFF(j - 1);
	}

	cp->type = C_REPLV;
	cp->vcnt = count;
    }
    zfree(xbuff, limit);
    return cp;
}

/* free memory used by a replacement CELL  */

void
repl_destroy(CELL *cp)
{
    register STRING **p;
    VCount cnt;

    if (cp->type == C_REPL) {
	free_STRING(string(cp));
    } else if (cp->ptr != NULL) {	/* an C_REPLV           */
	p = (STRING **) cp->ptr;
	for (cnt = cp->vcnt; cnt; cnt--) {
	    if (*p) {
		free_STRING(*p);
	    }
	    p++;
	}
	zfree(cp->ptr, cp->vcnt * sizeof(STRING *));
    }
}

/* copy a C_REPLV cell to another CELL */

CELL *
replv_cpy(CELL *target, CELL *source)
{
    STRING **t, **s;
    VCount cnt;

    target->type = C_REPLV;
    cnt = target->vcnt = source->vcnt;
    target->ptr = (PTR) zmalloc(cnt * sizeof(STRING *));

    t = (STRING **) target->ptr;
    s = (STRING **) source->ptr;
    while (cnt) {
	cnt--;
	if (*s)
	    (*s)->ref_cnt++;
	*t++ = *s++;
    }
    return target;
}

/* here's our old friend linked linear list with move to the front
   for compilation of replacement CELLs	 */

typedef struct repl_node {
    struct repl_node *link;
    STRING *sval;		/* the input */
    CELL *cp;			/* the output */
} REPL_NODE;

static REPL_NODE *repl_list;

/* search the list (with move to the front) for a compiled
   separator.
   return a ptr to a CELL (C_REPL or C_REPLV)
*/

CELL *
repl_compile(STRING * sval)
{
    register REPL_NODE *p;
    REPL_NODE *q;
    char *s;

    /* search the list */
    s = sval->str;
    p = repl_list;
    q = (REPL_NODE *) 0;
    while (p) {
	if (strcmp(s, p->sval->str) == 0)	/* found */
	{
	    if (!q)		/* already at front */
		return p->cp;
	    else {		/* delete from list for move to front */
		q->link = p->link;
		goto found;
	    }

	} else {
	    q = p;
	    p = p->link;
	}
    }

    /* not found */
    p = ZMALLOC(REPL_NODE);
    p->sval = sval;
    sval->ref_cnt++;
    p->cp = REPL_compile(sval);

  found:
/* insert p at the front of the list */
    p->link = repl_list;
    repl_list = p;
    return p->cp;
}

/* return the string for a CELL or type REPL or REPLV,
   this is only used by da()  */

const STRING *
repl_uncompile(CELL *cp)
{
    register REPL_NODE *p = repl_list;

    if (cp->type == C_REPL) {
	while (p) {
	    if (p->cp->type == C_REPL && p->cp->ptr == cp->ptr)
		return p->sval;
	    else
		p = p->link;
	}
    } else {
	while (p) {
	    if (p->cp->type == C_REPLV &&
		memcmp(cp->ptr, p->cp->ptr, cp->vcnt * sizeof(STRING *))
		== 0)
		return p->sval;
	    else
		p = p->link;
	}
    }

#ifdef DEBUG
    bozo("unable to uncompile an repl");
#else
    return NULL;
#endif
}

/*
  convert a C_REPLV to	C_REPL
     replacing the &s with sval
*/

CELL *
replv_to_repl(CELL *cp, STRING * sval)
{
    register STRING **p;
    STRING **sblock = (STRING **) cp->ptr;
    unsigned cnt, vcnt = cp->vcnt;
    size_t len;
    char *target;

#ifdef DEBUG
    if (cp->type != C_REPLV)
	bozo("not replv");
#endif

    p = sblock;
    cnt = vcnt;
    len = 0;
    while (cnt--) {
	if (*p)
	    len += (*p++)->len;
	else {
	    *p++ = sval;
	    sval->ref_cnt++;
	    len += sval->len;
	}
    }
    cp->type = C_REPL;
    cp->ptr = (PTR) new_STRING0(len);

    p = sblock;
    cnt = vcnt;
    target = string(cp)->str;
    while (cnt--) {
	memcpy(target, (*p)->str, (*p)->len);
	target += (*p)->len;
	free_STRING(*p);
	p++;
    }

    zfree(sblock, vcnt * sizeof(STRING *));
    return cp;
}

#ifdef NO_LEAKS
typedef struct _all_ptrs {
    struct _all_ptrs *next;
    PTR m;
} ALL_PTRS;

static ALL_PTRS *all_ptrs;
/*
 * Some regular expressions are parsed, and the pointer stored in the byte-code
 * where we cannot distinguish it from other constants.  Keep a list here, to
 * free on exit for auditing.
 */
void
no_leaks_re_ptr(PTR m)
{
    ALL_PTRS *p = calloc(1, sizeof(ALL_PTRS));
    p->next = all_ptrs;
    p->m = m;
    all_ptrs = p;
}

void
re_leaks(void)
{
    while (all_ptrs != NULL) {
	ALL_PTRS *next = all_ptrs->next;
	re_destroy(all_ptrs->m);
	free(all_ptrs);
	all_ptrs = next;
    }

    while (repl_list != NULL) {
	REPL_NODE *p = repl_list->link;
	free_STRING(repl_list->sval);
	free_cell_data(repl_list->cp);
	ZFREE(repl_list);
	repl_list = p;
    }

    if (split_size != 0) {
	zfree(split_buff, split_size * sizeof(STRING *));
    }
}
#endif
