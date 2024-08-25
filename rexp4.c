/*
rexp4.c
copyright 2024 Thomas E. Dickey

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
 */

/*
 * $MawkId: rexp4.c,v 1.11 2024/08/25 17:16:24 tom Exp $
 */
#include <field.h>
#include <repl.h>

char *
is_string_split(PTR q, size_t *lenp)
{
    if (q != NULL) {
	STRING *s = ((RE_NODE *) q)->sval;
	char *result = s->str;

	/* if we have only one character, it cannot be a regex */
	if (s->len == 1) {
	    *lenp = s->len;
	    return result;
	} else {
	    size_t n;
	    for (n = 0; n < s->len; ++n) {
		/* if we have a meta character, it probably is a regex */
		switch (result[n]) {
		case '\\':
		case '$':
		case '(':
		case ')':
		case '*':
		case '+':
		case '.':
		case '?':
		case '[':
		case ']':
		case '^':
		case '|':
#ifndef NO_INTERVAL_EXPR
		case L_CURL:
		case R_CURL:
#endif
		    return NULL;
		}
	    }
	    *lenp = s->len;
	    return result;
	}
    }
    return NULL;
}
