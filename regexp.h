/********************************************
regexp.h
copyright 2009-2014,2020, Thomas E. Dickey
copyright 2005, Aleksey Cheusov
copyright 1991,1993, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: regexp.h,v 1.15 2020/10/16 22:43:29 tom Exp $
 */
#ifndef  MAWK_REPL_H
#define  MAWK_REPL_H

#include <stdio.h>
#include "nstd.h"

const char *REerror(void);

#ifdef LOCAL_REGEXP
#include "rexp.h"
STATE *REcompile(char *, size_t);
void REdestroy(STATE *);
int REtest(char *, size_t, STATE *);
char *REmatch(char *, size_t, STATE *, size_t *, int);
void REmprint(STATE *, FILE *);
const char *REs_type(STATE *);
#else
PTR REcompile(char *, size_t);
void REdestroy(PTR);
int REtest(char *, size_t, PTR);
char *REmatch(char *, size_t, PTR, size_t *, int);
void REmprint(PTR, FILE *);
#endif

#endif /*  MAWK_REPL_H */
