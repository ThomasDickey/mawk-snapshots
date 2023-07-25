/********************************************
scan.h
copyright 2009-2010,2023 Thomas E. Dickey
copyright 2009, Jonathan Nieder
copyright 1991-1994,1995, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: scan.h,v 1.7 2023/07/23 08:56:43 tom Exp $
 */

/* scan.h  */

#ifndef MAWK_SCAN_H
#define MAWK_SCAN_H

#include <stdio.h>

#include  "scancode.h"
#include  "symtype.h"
#include  "parse.h"

extern double double_zero;
extern double double_one;

extern void eat_nl(void);

/* in error.c */
extern void unexpected_char(void);

#endif /* MAWK_SCAN_H */
