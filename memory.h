/********************************************
memory.h
copyright 2009-2010,2023 Thomas E. Dickey
copyright 1991,1993, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: memory.h,v 1.10 2023/07/22 22:28:48 tom Exp $
 */

/*  memory.h  */

#ifndef  MAWK_MEMORY_H
#define  MAWK_MEMORY_H

#include "types.h"
#include "zmalloc.h"

STRING *new_STRING(const char *);
STRING *new_STRING0(size_t);
STRING *new_STRING1(const char *, size_t);

#ifdef   DEBUG
void DB_free_STRING(STRING *);

#define  free_STRING(s)  DB_free_STRING(s)

#else

#define  free_STRING(sval) \
	    do { \
		if ( -- (sval)->ref_cnt == 0 && \
		    sval != &null_str ) \
		    zfree(sval, (sval)->len + STRING_OH) ; \
	    } while (0)
#endif

#endif /* MAWK_MEMORY_H */
