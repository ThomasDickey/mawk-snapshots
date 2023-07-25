/********************************************
zmalloc.h
copyright 2009-2010,2023 Thomas E. Dickey
copyright 1991,1993, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: zmalloc.h,v 1.8 2023/07/25 21:20:54 tom Exp $
 */

#ifndef  ZMALLOC_H
#define  ZMALLOC_H

#include <nstd.h>

extern PTR zmalloc(size_t);
extern void zfree(PTR, size_t);
extern PTR zrealloc(PTR, size_t, size_t);

#define ZMALLOC(type)  ((type*)zmalloc(sizeof(type)))
#define ZFREE(p)	zfree(p,sizeof(*(p)))

#endif /* ZMALLOC_H */
