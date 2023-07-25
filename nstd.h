/********************************************
nstd.h
copyright 2009-2017,2023 Thomas E. Dickey
copyright 1995, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/* Never Standard.h

   This has all the prototypes that are supposed to
   be in a standard place but never are, and when they are
   the standard place isn't standard
*/

/*
 * $MawkId: nstd.h,v 1.12 2023/07/22 22:27:11 tom Exp $
 */

#ifndef  NSTD_H
#define  NSTD_H		1

#include <config.h>

/* types */

typedef void *PTR;

#ifdef   SIZE_T_STDDEF_H
#include <stddef.h>
#else
#ifdef   SIZE_T_TYPES_H
#include <sys/types.h>
#else
typedef unsigned size_t;
#endif
#endif

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>

/* if have to diddle with errno to get errors from the math library */
#ifndef STDC_MATHERR
#if (defined(FPE_TRAPS_ON) && !defined(HAVE_MATHERR))
#define STDC_MATHERR   1
#else
#define STDC_MATHERR   0
#endif
#endif

#endif /* NSTD_H */
