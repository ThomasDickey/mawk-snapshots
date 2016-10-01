/********************************************
kw.c
copyright 2008-2012,2016, Thomas E. Dickey
copyright 1991-1993, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: kw.c,v 1.7 2016/09/29 23:02:51 tom Exp $
 */

/* kw.c */

#include "mawk.h"
#include "symtype.h"
#include "parse.h"
#include "init.h"
/* *INDENT-OFF* */
static const struct kw
{
    const char *text;
    short kw;
}
keywords[] =
{
    { "print",    PRINT },
    { "printf",   PRINTF },
    { "do",       DO },
    { "while",    WHILE },
    { "for",      FOR },
    { "break",    BREAK },
    { "continue", CONTINUE },
    { "if",       IF },
    { "else",     ELSE },
    { "in",       IN },
    { "delete",   DELETE },
    { "split",    SPLIT },
    { "match",    MATCH_FUNC },
    { "BEGIN",    BEGIN },
    { "END",      END },
    { "exit",     EXIT },
    { "next",     NEXT },
    { "nextfile", NEXTFILE },
    { "return",   RETURN },
    { "getline",  GETLINE },
    { "sub",      SUB },
    { "gsub",     GSUB },
    { "function", FUNCTION },
    { (char *) 0, 0 }
};
/* *INDENT-ON* */

/* put keywords in the symbol table */
void
kw_init(void)
{
    register const struct kw *p = keywords;
    register SYMTAB *q;

    while (p->text) {
	q = insert(p->text);
	q->type = ST_KEYWORD;
	q->stval.kw = p++->kw;
    }
}

/* find a keyword to emit an error message */
const char *
find_kw_str(int kw_token)
{
    const struct kw *p;

    for (p = keywords; p->text; p++)
	if (p->kw == kw_token)
	    return p->text;
    /* search failed */
    return (char *) 0;
}
