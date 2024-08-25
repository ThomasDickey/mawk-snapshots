/********************************************
kw.c
copyright 2008-2023,2024, Thomas E. Dickey
copyright 1991-1993, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: kw.c,v 1.11 2024/08/25 17:21:36 tom Exp $
 */

#define Visible_SYMTAB

#include <mawk.h>
#include <symtype.h>
#include <parse.h>
#include <init.h>
/* *INDENT-OFF* */
static const struct kw
{
    const char text[12];
    short kw;
}
keywords[] =
{
    { "BEGIN",    BEGIN },
    { "END",      END },
    { "break",    BREAK },
    { "continue", CONTINUE },
    { "delete",   DELETE },
    { "do",       DO },
    { "else",     ELSE },
    { "exit",     EXIT },
    { "for",      FOR },
    { "function", FUNCTION },
    { "getline",  GETLINE },
    { "gsub",     GSUB },
    { "if",       IF },
    { "in",       IN },
    { "length",   LENGTH },
    { "match",    MATCH_FUNC },
    { "next",     NEXT },
    { "nextfile", NEXTFILE },
    { "print",    PRINT },
    { "printf",   PRINTF },
    { "return",   RETURN },
    { "split",    SPLIT },
    { "sub",      SUB },
    { "while",    WHILE },
    { "",         0 }
};
/* *INDENT-ON* */

/* put keywords in the symbol table */
void
kw_init(void)
{
    register const struct kw *p = keywords;

    while (p->text[0]) {
	register SYMTAB *q;

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

    for (p = keywords; p->text[0]; p++)
	if (p->kw == kw_token)
	    return p->text;
    /* search failed */
    return (char *) 0;
}
