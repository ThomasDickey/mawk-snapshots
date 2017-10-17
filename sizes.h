/********************************************
sizes.h
copyright 2009-2014,2017  Thomas E. Dickey
copyright 1991-1995,2014.  Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: sizes.h,v 1.11 2017/10/17 00:43:54 tom Exp $
 */

/*  sizes.h  */

#ifndef  SIZES_H
#define  SIZES_H

#include "config.h"

#ifndef MAX__INT
#include <limits.h>
#define  MAX__INT  INT_MAX
#define  MAX__LONG LONG_MAX
#define  MAX__UINT UINT_MAX
#endif /* MAX__INT */

#if  MAX__INT <= 0x7fff
#define  SHORT_INTS
#define  INT_FMT "%ld"
typedef long Int;
#define  Max_Int MAX__LONG
#else
#define  INT_FMT "%d"
typedef int Int;
#define  Max_Int  MAX__INT
#endif

#if  MAX__UINT <= 0xffff
#define  SHORT_UINTS
#define  UINT_FMT "%lu"
typedef unsigned long UInt;
#define  Max_UInt MAX__ULONG
#else
#define  UINT_FMT "%u"
typedef unsigned UInt;
#define  Max_UInt  MAX__UINT
#endif

#define EVAL_STACK_SIZE  1024	/* initial size , can grow */

/*
 * FBANK_SZ, the number of fields at startup, must be a power of 2.
 *
 */
#define  FBANK_SZ	1024
#define  FB_SHIFT	  10	/* lg(FBANK_SZ) */

/*
 * hardwired limit on sprintf size, can be overridden with -Ws=xxx
 * TBD to remove hard wired limit
 */
#define  SPRINTF_LIMIT	8192

#define  BUFFSZ         4096
  /* starting buffer size for input files, grows if
     necessary */

#ifdef  MSDOS
/* trade some space for IO speed */
#undef  BUFFSZ
#define BUFFSZ		8192
/* maximum input buffers that will fit in 64K */
#define  MAX_BUFFS	((int)(0x10000L/BUFFSZ) - 1)
#endif

#define  HASH_PRIME  53
#define  A_HASH_PRIME 199

#define  MAX_COMPILE_ERRORS  5	/* quit if more than 4 errors */

#endif /* SIZES_H */
