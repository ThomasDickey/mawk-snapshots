/********************************************
memory.c
copyright 2009-2024,2026  Thomas E. Dickey
copyright 1991-1992,1993  Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: memory.c,v 1.11 2026/01/17 00:25:57 tom Exp $
 */

#define Visible_STRING

#include <mawk.h>
#include <memory.h>

STRING null_str =
{0, 1, ""};

static STRING *
xnew_STRING(size_t len)
{
    STRING *sval = (STRING *) zmalloc(SizePlus(len, STRING_OH));

    sval->len = len;
    sval->ref_cnt = 1;
    return sval;
}

/* allocate space for a STRING */

STRING *
new_STRING0(size_t len)
{
    if (len == 0) {
	null_str.ref_cnt++;
	return &null_str;
    } else {
	STRING *sval = xnew_STRING(len);
	sval->str[len] = 0;
	return sval;
    }
}

/*
 * Create a new string which may contain embedded nulls.
 */
STRING *
new_STRING1(const char *s, size_t len)
{
    if (len == 0) {
	null_str.ref_cnt++;
	return &null_str;
    } else {
	STRING *sval = xnew_STRING(len);
	memcpy(sval->str, s, len);
	sval->str[len] = 0;
	return sval;
    }
}

/* convert char* to STRING* */

STRING *
new_STRING(const char *s)
{

    if (s[0] == 0) {
	null_str.ref_cnt++;
	return &null_str;
    } else {
	STRING *sval = xnew_STRING(strlen(s));
	strcpy(sval->str, s);
	return sval;
    }
}

#ifdef	 DEBUG

void
DB_free_STRING(STRING * sval)
{
    if (--sval->ref_cnt == 0 &&
	sval != &null_str) {
	zfree(sval, sval->len + STRING_OH);
    }
}

#endif
