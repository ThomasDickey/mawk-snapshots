/********************************************
sizes.h
copyright 2009-2020,2024  Thomas E. Dickey
copyright 1991-1995,2014.  Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: sizes.h,v 1.25 2024/08/25 17:16:24 tom Exp $
 */

/*  sizes.h  */

#ifndef  SIZES_H
#define  SIZES_H
/* *INDENT-OFF* */

#include <config.h>
#include <float.h>
#include <limits.h>

#ifndef SIZEOF_LONG
#define SIZEOF_LONG 4
#endif

#ifndef SIZEOF_LONG_LONG
#define SIZEOF_LONG_LONG 4
#endif

#if defined(HAVE_STDINT_H) && defined(HAVE_INT64_T) && defined(HAVE_UINT64_T)

#include <stdint.h>

#if defined(INT64_MAX)
#define  MAX__INT       INT64_MAX
#define  MIN__INT       INT64_MIN
#elif defined(LLONG_MAX)
#define  MAX__INT       LLONG_MAX
#define  MIN__INT       LLONG_MIN
#elif defined(LONG_LONG_MAX)
#define  MAX__INT       LONG_LONG_MAX
#define  MIN__INT       LONG_LONG_MIN
#endif

#if defined(UINT64_MAX)
#define  MAX__UINT      UINT64_MAX
#elif defined(LLONG_MAX)
#define  MAX__UINT      ULLONG_MAX
#elif defined(LONG_LONG_MAX)
#define  MAX__UINT      ULONG_LONG_MAX
#endif

#define  MAX__LONG      MAX__INT
#define  MIN__LONG      MIN__INT
#define  MAX__ULONG     MAX__UINT

typedef int64_t         Int;
typedef int64_t         Long;
#define  Max_Int        MAX__INT
#define  Min_Int        MIN__INT
#define  Max_Long       MAX__LONG
#define  Min_Long       MIN__LONG

typedef uint64_t        UInt;
typedef uint64_t        ULong;
#define  Max_UInt       MAX__UINT
#define  Max_ULong      MAX__ULONG

#if (SIZEOF_LONG_LONG > SIZEOF_LONG) || defined(__APPLE__) || defined(__OpenBSD__)
#define  INT_FMT        "%lld"
#define  UINT_FMT       "%llu"
#define  LONG_FMT       "%lld"
#define  ULONG_FMT      "%llu"
#define  USE_LL_FORMAT  1
#else
#define  INT_FMT        "%ld"
#define  UINT_FMT       "%lu"
#define  LONG_FMT       "%ld"
#define  ULONG_FMT      "%lu"
#endif

#else /* !defined(HAVE_STDINT_H), etc */

#ifndef MAX__INT
#define  MAX__INT       INT_MAX
#define  MIN__INT       INT_MIN
#define  MAX__LONG      LONG_MAX
#define  MIN__LONG      LONG_MIN
#define  MAX__UINT      UINT_MAX
#define  MAX__ULONG     ULONG_MAX
#endif /* MAX__INT */

#if  MAX__INT <= 0x7fff
#define  SHORT_INTS
#define  INT_FMT        "%ld"
typedef long            Int;
typedef long            Long;
#define  Max_Int        MAX__LONG
#define  Min_Int        MIN__LONG
#define  Max_Long       MAX__LONG
#define  Min_Long       MIN__LONG
#else
#define  INT_FMT        "%d"
typedef int             Int;
typedef long            Long;
#define  Max_Int        MAX__INT
#define  Min_Int        MIN__INT
#define  Max_Long       MAX__LONG
#define  Min_Long       MIN__LONG
#endif

#if  MAX__UINT <= 0xffff
#define  SHORT_UINTS
#define  UINT_FMT       "%lu"
typedef unsigned long   UInt;
typedef unsigned long   ULong;
#define  Max_UInt       MAX__ULONG
#define  Max_ULong      MAX__ULONG
#else
#define  UINT_FMT       "%u"
typedef unsigned int    UInt;
typedef unsigned long   ULong;
#define  Max_UInt       MAX__UINT
#define  Max_ULong      MAX__ULONG
#endif

#define  LONG_FMT       "%ld"
#define  ULONG_FMT      "%lu"

#endif /* HAVE_STDINT_H */

#define TOTAL_BITS_IN_TYPE(x)                   (sizeof(x) * CHAR_BIT)
#define TOTAL_BITS_IN_LONG                      TOTAL_BITS_IN_TYPE(Long)
#define TOTAL_BITS_IN_ULONG                     TOTAL_BITS_IN_TYPE(ULong)
#define TOTAL_MAGNITUDE_BITS_IN_LONG            (TOTAL_BITS_IN_LONG - 1) /* size minus the sign bit */
#define TOTAL_SIGNIFICAND_BITS_IN_DOUBLE        DBL_MANT_DIG /* the non-sign, non-exponent bits in a double */
#define Max_Double_Safe_Long                                         \
    (TOTAL_SIGNIFICAND_BITS_IN_DOUBLE > TOTAL_MAGNITUDE_BITS_IN_LONG \
        ? Max_Long                                                   \
        : (((Long) 1 << TOTAL_SIGNIFICAND_BITS_IN_DOUBLE) - 1))
#define Min_Double_Safe_Long                                         \
    (TOTAL_SIGNIFICAND_BITS_IN_DOUBLE > TOTAL_MAGNITUDE_BITS_IN_LONG \
        ? Min_Long                                                   \
        : -(((Long) 1 << TOTAL_SIGNIFICAND_BITS_IN_DOUBLE) - 1))
#define Max_Double_Safe_ULong                                        \
    (TOTAL_SIGNIFICAND_BITS_IN_DOUBLE > TOTAL_BITS_IN_ULONG          \
        ? Max_ULong                                                  \
        : (((ULong) 1 << TOTAL_SIGNIFICAND_BITS_IN_DOUBLE) - 1))

#define EVAL_STACK_SIZE 1024	/* initial size , can grow */

/*
 * FBANK_SZ, the number of fields at startup, must be a power of 2.
 *
 */
#ifndef FB_SHIFT
#define  FB_SHIFT	  10	/* lg(FBANK_SZ) */
#endif
#define  FBANK_SZ	(1 << FB_SHIFT)

/*
 * hardwired limit on sprintf size, can be overridden with -Ws=xxx
 * TBD to remove hard wired limit
 */
#ifndef SPRINTF_LIMIT
#define  SPRINTF_LIMIT	8192
#endif

#define  BUFFSZ         (SPRINTF_LIMIT / 2)
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

/* *INDENT-ON* */

#endif /* SIZES_H */
