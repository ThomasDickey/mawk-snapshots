/********************************************
bi_funct.c
copyright 2008-2023,2024, Thomas E. Dickey
copyright 1991-1995,1996, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: bi_funct.c,v 1.134 2024/09/05 17:44:48 tom Exp $
 */

#define Visible_ARRAY
#define Visible_BI_REC
#define Visible_CELL
#define Visible_RE_DATA
#define Visible_STRING
#define Visible_SYMTAB

#include <mawk.h>
#include <bi_funct.h>
#include <bi_vars.h>
#include <memory.h>
#include <init.h>
#include <files.h>
#include <fin.h>
#include <field.h>
#include <regexp.h>

#include <ctype.h>
#include <math.h>
#include <time.h>

#if defined(mawk_srand) || defined(mawk_rand)
#define USE_SYSTEM_SRAND
#endif

#if defined(HAVE_BSD_STDLIB_H) && defined(USE_SYSTEM_SRAND)
#include <bsd/stdlib.h>		/* prototype arc4random */
#endif

#if defined(WINVER) && (WINVER >= 0x501)
#include <windows.h>
#endif

#if OPT_TRACE > 0
#define return_CELL(func, cell) TRACE(("..." func " ->")); \
				TRACE_CELL(cell); \
				return cell
#else
#define return_CELL(func, cell) return cell
#endif

/* global for the disassembler */
/* *INDENT-OFF* */
const BI_REC bi_funct[] =
{				/* info to load builtins */

   { "index",    bi_index,    2, 2 },
   { "substr",   bi_substr,   2, 3 },
   { "sprintf",  bi_sprintf,  1, MAX_ARGS },
   { "sin",      bi_sin,      1, 1 },
   { "cos",      bi_cos,      1, 1 },
   { "atan2",    bi_atan2,    2, 2 },
   { "exp",      bi_exp,      1, 1 },
   { "log",      bi_log,      1, 1 },
   { "int",      bi_int,      1, 1 },
   { "sqrt",     bi_sqrt,     1, 1 },
   { "rand",     bi_rand,     0, 0 },
   { "srand",    bi_srand,    0, 1 },
   { "close",    bi_close,    1, 1 },
   { "system",   bi_system,   1, 1 },
   { "toupper",  bi_toupper,  1, 1 },
   { "tolower",  bi_tolower,  1, 1 },
   { "fflush",   bi_fflush,   0, 1 },

   /* useful gawk extension (time functions) */
   { "systime",  bi_systime,  0, 0 },
#ifdef HAVE_MKTIME
   { "mktime",   bi_mktime,   1, 1 },
#endif
#ifdef HAVE_STRFTIME
   { "strftime", bi_strftime, 0, 3 },
#endif

   { "",         (PF_CP) 0, 0, 0 }
};
/* *INDENT-ON* */

/* load built-in functions in symbol table */
void
bi_funct_init(void)
{
    register const BI_REC *p;
    register SYMTAB *stp;

    for (p = bi_funct; p->name[0]; p++) {
	stp = insert(p->name);
	stp->type = ST_BUILTIN;
	stp->stval.bip = p;
    }

#ifndef NO_INIT_SRAND
    /* seed rand() off the clock */
    {
	CELL c[2];

	memset(c, 0, sizeof(c));
	c[1].type = C_NOINIT;
	bi_srand(c + 1);
    }
#endif

}

/**************************************************
 string builtins (except split (in split.c) and [g]sub (at end))
 **************************************************/

CELL *
bi_length(CELL *sp)
{
    size_t len;

    TRACE_FUNC2("bi_length", sp, 1);

    if (sp->type < C_STRING)
	cast1_to_s(sp);
    len = string(sp)->len;

    free_STRING(string(sp));
    sp->type = C_DOUBLE;
    sp->dval = (double) len;

    return_CELL("bi_length", sp);
}

/* length (size) of an array */
CELL *
bi_alength(CELL *sp)
{
    TRACE_FUNC2("bi_alength", sp, 1);

    sp->type = C_DOUBLE;
    sp->dval = (double) ((ARRAY) sp->ptr)->size;

    return_CELL("bi_alength", sp);
}

char *
str_str(char *target, size_t target_len, const char *key, size_t key_len)
{
    register int k = key[0];
    int k1;
    const char *prior;
    char *result = 0;

    switch (key_len) {
    case 0:
	break;
    case 1:
	if (target_len != 0) {
	    result = memchr(target, k, target_len);
	}
	break;
    case 2:
	k1 = key[1];
	prior = target;
	while (target_len >= key_len && (target = memchr(target, k, target_len))) {
	    target_len = target_len - (size_t) (target - prior) - 1;
	    prior = ++target;
	    if (target_len > 0 && target[0] == k1) {
		result = target - 1;
		break;
	    }
	}
	break;
    default:
	key_len--;
	prior = target;
	while (target_len > key_len && (target = memchr(target, k, target_len))) {
	    target_len = target_len - (size_t) (target - prior) - 1;
	    prior = ++target;
	    if (target_len >= key_len && memcmp(target, key + 1, key_len) == 0) {
		result = target - 1;
		break;
	    }
	}
	break;
    }
    return result;
}

CELL *
bi_index(CELL *sp)
{
    size_t idx;
    size_t len;

    TRACE_FUNC2("bi_index", sp, 2);

    sp--;
    if (TEST2(sp) != TWO_STRINGS)
	cast2_to_s(sp);

    if ((len = string(sp + 1)->len)) {
	const char *p;
	idx = (size_t) ((p = str_str(string(sp)->str,
				     string(sp)->len,
				     string(sp + 1)->str,
				     len))
			? p - string(sp)->str + 1
			: 0);
    } else {			/* index of the empty string */
	idx = 1;
    }

    free_STRING(string(sp));
    free_STRING(string(sp + 1));
    sp->type = C_DOUBLE;
    sp->dval = (double) idx;
    return_CELL("bi_index", sp);
}

/*  substr(s, i, n)
    if l = length(s)  then get the characters
    from  max(1,i) to min(l,n-i-1) inclusive */

CELL *
bi_substr(CELL *sp)
{
    int n_args, len;
    register int i, n;
    STRING *sval;		/* substr(sval->str, i, n) */

    TRACE_FUNC("bi_substr", sp);

    n_args = sp->type;
    sp -= n_args;
    if (sp->type != C_STRING)
	cast1_to_s(sp);
    /* don't use < C_STRING shortcut */
    sval = string(sp);

    if ((len = (int) sval->len) == 0)	/* substr on null string */
    {
	if (n_args == 3) {
	    cell_destroy(sp + 2);
	}
	cell_destroy(sp + 1);
	return_CELL("bi_substr", sp);
    }

    if (n_args == 2) {
	n = len;
	if (sp[1].type != C_DOUBLE) {
	    cast1_to_d(sp + 1);
	}
    } else {
	if (TEST2(sp + 1) != TWO_DOUBLES)
	    cast2_to_d(sp + 1);
	n = d_to_i(sp[2].dval);
    }
    i = d_to_i(sp[1].dval) - 1;	/* i now indexes into string */

    /*
     * If the starting index is past the end of the string, there is nothing
     * to extract other than an empty string.
     */
    if (i > len) {
	n = 0;
    }

    /*
     * Workaround in case someone's written a script that does substr(0,last-1)
     * by transforming it into substr(1,last).
     */
    if (i < 0) {
	n -= i + 1;
	i = 0;
    }

    /*
     * Keep 'n' from extending past the end of the string.
     */
    if (n > len - i) {
	n = len - i;
    }

    if (n <= 0)			/* the null string */
    {
	sp->ptr = (PTR) & null_str;
	null_str.ref_cnt++;
    } else {			/* got something */
	sp->ptr = (PTR) new_STRING0((size_t) n);
	memcpy(string(sp)->str, sval->str + i, (size_t) n);
    }

    free_STRING(sval);
    return_CELL("bi_substr", sp);
}

/*
  match(s,r)
  sp[0] holds r, sp[-1] holds s
*/

CELL *
bi_match(CELL *sp)
{
    char *p;
    size_t length;

    TRACE_FUNC2("bi_match", sp + 1, 2);

    if (sp->type != C_RE)
	cast_to_RE(sp);
    if ((--sp)->type < C_STRING)
	cast1_to_s(sp);

    cell_destroy(RSTART);
    cell_destroy(RLENGTH);
    RSTART->type = C_DOUBLE;
    RLENGTH->type = C_DOUBLE;

    p = REmatch(string(sp)->str,
		string(sp)->len,
		cast_to_re((sp + 1)->ptr),
		&length,
		0);

    if (p) {
	sp->dval = (double) (p - string(sp)->str + 1);
	RLENGTH->dval = (double) length;
    } else {
	sp->dval = 0.0;
	RLENGTH->dval = -1.0;	/* posix */
    }

    free_STRING(string(sp));
    sp->type = C_DOUBLE;

    RSTART->dval = sp->dval;

    return_CELL("bi_match", sp);
}

#define BI_TOCASE(case) \
CELL * \
bi_to##case(CELL *sp) \
{ \
    STRING *old; \
    size_t len; \
    register char *p, *q; \
\
    TRACE_FUNC2("bi_to" #case, sp, 1); \
\
    if (sp->type != C_STRING) \
        cast1_to_s(sp); \
    old = string(sp); \
    len = old->len; \
    sp->ptr = (PTR) new_STRING0(len); \
\
    q = string(sp)->str; \
    p = old->str; \
    while (len--) \
	*q++ = (char) to##case((UChar) *p++); \
    free_STRING(old); \
    return_CELL("bi_to" #case, sp); \
}
/* *INDENT-OFF* */
BI_TOCASE(upper)
BI_TOCASE(lower)
#undef BI_TOCASE
/* *INDENT-ON* */

/*
 * Like gawk...
 */
CELL *
bi_systime(CELL *sp)
{
    time_t result;
    time(&result);

    TRACE_FUNC2("bi_systime", sp, 0);

    sp++;
    sp->type = C_DOUBLE;
    sp->dval = (double) result;
    return_CELL("bi_systime", sp);
}

#ifdef HAVE_MKTIME
/*  mktime(datespec)
    Turns datespec into a time stamp of the same form as returned by systime().
    The datespec is a string of the form
        YYYY MM DD HH MM SS [DST].
*/
CELL *
bi_mktime(CELL *sp)
{
    time_t result;
    struct tm my_tm;
    STRING *sval = string(sp);

    TRACE_FUNC2("bi_mktime", sp, 1);

    if (!sval)
	goto error;

    memset(&my_tm, 0, sizeof(my_tm));
    switch (sscanf(sval->str, "%d %d %d %d %d %d %d",
		   &my_tm.tm_year,
		   &my_tm.tm_mon,
		   &my_tm.tm_mday,
		   &my_tm.tm_hour,
		   &my_tm.tm_min,
		   &my_tm.tm_sec,
		   &my_tm.tm_isdst)) {
    case 7:
	break;
    case 6:
	my_tm.tm_isdst = -1;	/* ask mktime to get timezone */
	break;
    default:
	goto error;		/* not enough data */
    }

    if (0) {
      error:
	result = -1;
    } else {
	my_tm.tm_year -= 1900;
	my_tm.tm_mon -= 1;
	result = mktime(&my_tm);
    }
    TRACE(("...bi_mktime(%s) ->%s", sval ? sval->str : "?", ctime(&result)));

    cell_destroy(sp);
    sp->type = C_DOUBLE;
    sp->dval = (double) result;
    return_CELL("bi_mktime", sp);
}
#endif

/*  strftime(format, timestamp, utc)
    should be equal to gawk strftime. all parameters are optional:
        format: ansi c strftime format descriptor. default is "%c"
        timestamp: seconds since unix epoch. default is now
        utc: when set and != 0 date is utc otherwise local. default is 0
*/
#ifdef HAVE_STRFTIME
CELL *
bi_strftime(CELL *sp)
{
    const char *format = "%c";
    time_t rawtime;
    struct tm *ptm;
    int n_args;
    int utc;
    STRING *sval = 0;		/* strftime(sval->str, timestamp, utc) */
    size_t result;

    TRACE_FUNC("bi_strftime", sp);

    n_args = sp->type;
    sp -= n_args;

    if (n_args > 0) {
	if (sp->type != C_STRING)
	    cast1_to_s(sp);
	/* don't use < C_STRING shortcut */
	sval = string(sp);

	if ((int) sval->len != 0)	/* strftime on valid format */
	    format = sval->str;
    } else {
	sp->type = C_STRING;
    }

    if (n_args > 1) {
	if (sp[1].type != C_DOUBLE)
	    cast1_to_d(sp + 1);
	rawtime = d_to_l(sp[1].dval);
    } else {
	time(&rawtime);
    }

    if (n_args > 2) {
	if (sp[2].type != C_DOUBLE)
	    cast1_to_d(sp + 2);
	utc = d_to_i(sp[2].dval);
    } else {
	utc = 0;
    }

    if (utc != 0)
	ptm = gmtime(&rawtime);
    else
	ptm = localtime(&rawtime);

    result = strftime(sprintf_buff,
		      (size_t) (sprintf_limit - sprintf_buff),
		      format,
		      ptm);
    TRACE(("...bi_strftime (%s, \"%d.%d.%d %d.%d.%d %d\", %d) ->%s\n",
	   format,
	   ptm->tm_year,
	   ptm->tm_mon,
	   ptm->tm_mday,
	   ptm->tm_hour,
	   ptm->tm_min,
	   ptm->tm_sec,
	   ptm->tm_isdst,
	   utc,
	   sprintf_buff));

    if (sval)
	free_STRING(sval);

    sp->ptr = (PTR) new_STRING1(sprintf_buff, result);

    while (n_args > 1) {
	n_args--;
	cell_destroy(sp + n_args);
    }
    return_CELL("bi_strftime", sp);
}
#endif /* HAVE_STRFTIME */

/************************************************
  arithmetic builtins
 ************************************************/

#if STDC_MATHERR
static void
fplib_err(
	     char *fname,
	     double val,
	     char *error)
{
    rt_error("%s(%g) : %s", fname, val, error);
}
#endif

CELL *
bi_sin(CELL *sp)
{
    TRACE_FUNC2("bi_sin", sp, 1);

#if ! STDC_MATHERR
    if (sp->type != C_DOUBLE)
	cast1_to_d(sp);
    sp->dval = sin(sp->dval);
#else
    {
	double x;

	errno = 0;
	if (sp->type != C_DOUBLE)
	    cast1_to_d(sp);
	x = sp->dval;
	sp->dval = sin(sp->dval);
	if (errno)
	    fplib_err("sin", x, "loss of precision");
    }
#endif
    return_CELL("bi_sin", sp);
}

CELL *
bi_cos(CELL *sp)
{
    TRACE_FUNC2("bi_cos", sp, 1);

#if ! STDC_MATHERR
    if (sp->type != C_DOUBLE)
	cast1_to_d(sp);
    sp->dval = cos(sp->dval);
#else
    {
	double x;

	errno = 0;
	if (sp->type != C_DOUBLE)
	    cast1_to_d(sp);
	x = sp->dval;
	sp->dval = cos(sp->dval);
	if (errno)
	    fplib_err("cos", x, "loss of precision");
    }
#endif
    return_CELL("bi_cos", sp);
}

CELL *
bi_atan2(CELL *sp)
{
    TRACE_FUNC2("bi_atan2", sp, 2);

#if  !	STDC_MATHERR
    sp--;
    if (TEST2(sp) != TWO_DOUBLES)
	cast2_to_d(sp);
    sp->dval = atan2(sp->dval, (sp + 1)->dval);
#else
    {
	errno = 0;
	sp--;
	if (TEST2(sp) != TWO_DOUBLES)
	    cast2_to_d(sp);
	sp->dval = atan2(sp->dval, (sp + 1)->dval);
	if (errno)
	    rt_error("atan2(0,0) : domain error");
    }
#endif
    return_CELL("bi_atan2", sp);
}

CELL *
bi_log(CELL *sp)
{
    TRACE_FUNC2("bi_log", sp, 1);

#if ! STDC_MATHERR
    if (sp->type != C_DOUBLE)
	cast1_to_d(sp);
    sp->dval = log(sp->dval);
#else
    {
	double x;

	errno = 0;
	if (sp->type != C_DOUBLE)
	    cast1_to_d(sp);
	x = sp->dval;
	sp->dval = log(sp->dval);
	if (errno)
	    fplib_err("log", x, "domain error");
    }
#endif
    return_CELL("bi_log", sp);
}

CELL *
bi_exp(CELL *sp)
{
    TRACE_FUNC2("bi_exp", sp, 1);

#if  ! STDC_MATHERR
    if (sp->type != C_DOUBLE)
	cast1_to_d(sp);
    sp->dval = exp(sp->dval);
#else
    {
	double x;

	errno = 0;
	if (sp->type != C_DOUBLE)
	    cast1_to_d(sp);
	x = sp->dval;
	sp->dval = exp(sp->dval);
	if (errno && sp->dval)
	    fplib_err("exp", x, "overflow");
	/* on underflow sp->dval==0, ignore */
    }
#endif
    return_CELL("bi_exp", sp);
}

CELL *
bi_int(CELL *sp)
{
    TRACE_FUNC2("bi_int", sp, 1);

    if (sp->type != C_DOUBLE)
	cast1_to_d(sp);
    sp->dval = sp->dval >= 0.0 ? floor(sp->dval) : ceil(sp->dval);
    return_CELL("bi_int", sp);
}

CELL *
bi_sqrt(CELL *sp)
{
    TRACE_FUNC2("bi_sqrt", sp, 1);

#if  ! STDC_MATHERR
    if (sp->type != C_DOUBLE)
	cast1_to_d(sp);
    sp->dval = sqrt(sp->dval);
#else
    {
	double x;

	errno = 0;
	if (sp->type != C_DOUBLE)
	    cast1_to_d(sp);
	x = sp->dval;
	sp->dval = sqrt(sp->dval);
	if (errno)
	    fplib_err("sqrt", x, "domain error");
    }
#endif
    return_CELL("bi_sqrt", sp);
}

#if !(defined(mawk_srand) || defined(mawk_rand))
/* For portability, we'll use our own random number generator , taken
   from:  Park, SK and Miller KW, "Random Number Generators:
   Good Ones are Hard to Find", CACM, 31, 1192-1201, 1988.
*/

static long seed;		/* must be >=1 and < 2^31-1 */
static CELL cseed;		/* argument of last call to srand() */

#define		M	0x7fffffff	/* 2^31-1 */
#define		MX	0xffffffff
#define		A	16807
#define	  	Q	127773	/* M/A */
#define	  	R	2836	/* M%A */

#if M == MAX__LONG
#define crank(s)   s = A * (s % Q) - R * (s / Q) ;\
		   if ( s <= 0 ) s += M
#else
/* 64 bit longs */
#define crank(s)	{ unsigned long t = (unsigned long) s ;\
			  t = (A * (t % Q) - R * (t / Q)) & MX ;\
			  if ( t >= M ) t = (t+M)&M ;\
			  s = (long) t ;\
			}
#endif /* M == MAX__LONG */
#endif /* defined(mawk_srand) || defined(mawk_rand) */

static double
initial_seed(void)
{
    double result;
#if defined(HAVE_CLOCK_GETTIME)
    struct timespec data;
    if (clock_gettime(CLOCK_REALTIME, &data) == 0)
	result = (data.tv_sec * 1000000000L) + data.tv_nsec;
    else
	result = 0.0;
#elif defined(HAVE_GETTIMEOFDAY)
    struct timeval data;
    if (gettimeofday(&data, (struct timezone *) 0) == 0)
	result = (data.tv_sec * 1000000) + data.tv_usec;
    else
	result = 0.0;
#elif defined(WINVER) && (WINVER >= 0x501)
    union {
	FILETIME ft;
	long long since1601;	/* time since 1 Jan 1601 in 100ns units */
    } data;

    GetSystemTimeAsFileTime(&data.ft);
    result = (double) (data.since1601 / 10LL);
#else
    time_t now;
    (void) time(&now);
    result = (double) now;
#endif
    return result;
}

CELL *
bi_srand(CELL *sp)
{
#ifdef USE_SYSTEM_SRAND
    static CELL cseed =
    {
	C_DOUBLE, 0, 0, 1.0
    };
    double seed32;
#endif

    CELL c;

    TRACE_FUNC2("bi_srand", sp, (sp->type != C_NOINIT));

    if (sp->type == C_NOINIT)	/* seed off clock */
    {
	cellcpy(sp, &cseed);
	cell_destroy(&cseed);
	cseed.type = C_DOUBLE;
	cseed.dval = initial_seed();
    } else {			/* user seed */
	sp--;
	/* swap cseed and *sp ; don't need to adjust ref_cnts */
	c = *sp;
	*sp = cseed;
	cseed = c;
	if (cseed.type != C_DOUBLE)
	    cast1_to_d(&cseed);
    }

#ifdef USE_SYSTEM_SRAND
    seed32 = fmod(cseed.dval, (double) Max_UInt);
    mawk_srand((unsigned) seed32);
#else
    /* The old seed is now in *sp ; move the value in cseed to
       seed in range [1,M) */

    cellcpy(&c, &cseed);
    if (c.type == C_NOINIT)
	cast1_to_d(&c);

    seed = ((c.type == C_DOUBLE)
	    ? (long) (d_to_l(c.dval) & M) % M + 1
	    : (long) hash(string(&c)->str) % M + 1);
    if (seed == M)
	seed = M - 1;

    cell_destroy(&c);

    /* crank it once so close seeds don't give a close
       first result  */
    crank(seed);
#endif

    return_CELL("bi_srand", sp);
}

CELL *
bi_rand(CELL *sp)
{
    TRACE_FUNC2("bi_rand", sp, 0);

#ifdef USE_SYSTEM_SRAND
    {
	long value = (long) mawk_rand();
	sp++;
	sp->type = C_DOUBLE;
	sp->dval = ((double) value) / ((unsigned long) MAWK_RAND_MAX);
    }
#else
    crank(seed);
    sp++;
    sp->type = C_DOUBLE;
    sp->dval = (double) seed / (double) M;
#endif

    return_CELL("bi_rand", sp);
}

#undef	 A
#undef	 M
#undef   MX
#undef	 Q
#undef	 R
#undef   crank

/*************************************************
 miscellaneous builtins
 close, system and getline
 fflush
 *************************************************/

CELL *
bi_close(CELL *sp)
{
    int x;

    TRACE_FUNC2("bi_close", sp, 1);

    if (sp->type < C_STRING)
	cast1_to_s(sp);
    x = file_close((STRING *) sp->ptr);
    free_STRING(string(sp));
    sp->type = C_DOUBLE;
    sp->dval = (double) x;

    return_CELL("bi_close", sp);
}

CELL *
bi_fflush(CELL *sp)
{
    int ret = 0;

    TRACE_FUNC2("bi_fflush", sp, (sp->type != C_NOINIT));

    if (sp->type == C_NOINIT)
	fflush(stdout);
    else {
	sp--;
	if (sp->type < C_STRING)
	    cast1_to_s(sp);
	ret = file_flush(string(sp));
	free_STRING(string(sp));
    }

    sp->type = C_DOUBLE;
    sp->dval = (double) ret;

    return_CELL("bi_fflush", sp);
}

CELL *
bi_system(CELL *sp GCC_UNUSED)
{
    int ret_val;

    TRACE_FUNC2("bi_system", sp + 1, 1);

    if (sp->type < C_STRING)
	cast1_to_s(sp);

#ifdef HAVE_REAL_PIPES
    flush_all_output();
    ret_val = wait_status(system(string(sp)->str));
#elif defined(__MINGW32__)
    flush_all_output();
    ret_val = system(string(sp)->str);
#elif defined(MSDOS)
    ret_val = DOSexec(string(sp)->str);
#else
    ret_val = -1;
#endif

    cell_destroy(sp);
    sp->type = C_DOUBLE;
    sp->dval = (double) ret_val;

    return_CELL("bi_system", sp);
}

/*  getline()  */

/*  if type == 0 :  stack is 0 , target address

    if type == F_IN : stack is F_IN, expr(filename), target address
    if type == PIPE_IN : stack is PIPE_IN, target address, expr(pipename)
*/

CELL *
bi_getline(CELL *sp)
{
    CELL tc;
    CELL *cp = 0;
    char *p = 0;
    size_t len = 0;
    FIN *fin_p;

    TRACE_FUNC("bi_getline", sp);

    switch (sp->type) {
    case C_NOINIT:
	sp--;
	if (!main_fin)
	    open_main();

	if (!(p = FINgets(main_fin, &len)))
	    goto eof;

	cp = (CELL *) sp->ptr;
	if (TEST2(NR) != TWO_DOUBLES)
	    cast2_to_d(NR);
	NR->dval += 1.0;
	rt_nr++;
	FNR->dval += 1.0;
	rt_fnr++;
	break;

    case F_IN:
	sp--;
	if (sp->type < C_STRING)
	    cast1_to_s(sp);
	fin_p = (FIN *) file_find(string(sp), F_IN);
	free_STRING(string(sp));
	sp--;

	if (!fin_p)
	    goto open_failure;
	if (!(p = FINgets(fin_p, &len))) {
	    FINsemi_close(fin_p);
	    goto eof;
	}
	cp = (CELL *) sp->ptr;
	break;

    case PIPE_IN:
	sp -= 2;
	if (sp->type < C_STRING)
	    cast1_to_s(sp);
	fin_p = (FIN *) file_find(string(sp), PIPE_IN);
	free_STRING(string(sp));

	if (!fin_p)
	    goto open_failure;
	if (!(p = FINgets(fin_p, &len))) {
	    FINsemi_close(fin_p);
#ifdef  HAVE_REAL_PIPES
	    /* reclaim process slot */
	    wait_for(0);
#endif
	    goto eof;
	}
	cp = (CELL *) (sp + 1)->ptr;
	break;

    default:
	bozo("type in bi_getline");

    }

    /* we've read a line , store it */

    if (len == 0) {
	tc.type = C_STRING;
	tc.ptr = (PTR) & null_str;
	null_str.ref_cnt++;
    } else {
	tc.type = C_MBSTRN;
	tc.ptr = (PTR) new_STRING0(len);
	memcpy(string(&tc)->str, p, len);
    }

    slow_cell_assign(cp, &tc);

    cell_destroy(&tc);

    sp->dval = 1.0;
    goto done;
  open_failure:
    sp->dval = -1.0;
    goto done;
  eof:
    sp->dval = 0.0;		/* fall thru to done  */

  done:
    sp->type = C_DOUBLE;

    return_CELL("bi_getline", sp);
}

/**********************************************
 sub() and gsub()
 **********************************************/

/* entry:  sp[0] = address of CELL to sub on
	   sp[-1] = substitution CELL
	   sp[-2] = regular expression to match
*/

CELL *
bi_sub(CELL *sp)
{
    CELL *cp;			/* pointer to the replacement target */
    CELL tc;			/* build the new string here */
    CELL sc;			/* copy of the target CELL */
    char *front, *middle;	/* pieces */
    size_t middle_len;

    TRACE_FUNC("bi_sub", sp);

    sp -= 2;
    if (sp->type != C_RE)
	cast_to_RE(sp);
    if (sp[1].type != C_REPL && sp[1].type != C_REPLV)
	cast_to_REPL(sp + 1);
    cp = (CELL *) (sp + 2)->ptr;
    /* make a copy of the target, because we won't change anything
       including type unless the match works */
    cellcpy(&sc, cp);
    if (sc.type < C_STRING)
	cast1_to_s(&sc);
    front = string(&sc)->str;

    middle = REmatch(front,
		     string(&sc)->len,
		     cast_to_re(sp->ptr),
		     &middle_len,
		     0);

    if (middle != 0) {
	size_t front_len = (size_t) (middle - front);
	char *back = middle + middle_len;
	size_t back_len = string(&sc)->len - front_len - middle_len;

	if ((sp + 1)->type == C_REPLV) {
	    STRING *sval = new_STRING0(middle_len);

	    memcpy(sval->str, middle, middle_len);
	    replv_to_repl(sp + 1, sval);
	    free_STRING(sval);
	}

	tc.type = C_STRING;
	tc.ptr = (PTR) new_STRING0(front_len + string(sp + 1)->len + back_len);

	{
	    char *p = string(&tc)->str;

	    if (front_len) {
		memcpy(p, front, front_len);
		p += front_len;
	    }
	    if (string(sp + 1)->len) {
		memcpy(p, string(sp + 1)->str, string(sp + 1)->len);
		p += string(sp + 1)->len;
	    }
	    if (back_len)
		memcpy(p, back, back_len);
	}

	slow_cell_assign(cp, &tc);

	free_STRING(string(&tc));
    }

    free_STRING(string(&sc));
    repl_destroy(sp + 1);
    sp->type = C_DOUBLE;
    sp->dval = middle != (char *) 0 ? 1.0 : 0.0;

    return_CELL("bi_sub", sp);
}

static unsigned repl_cnt;	/* number of global replacements */

static STRING *
gsub3(PTR re, CELL *repl, CELL *target)
{
    int j;
    CELL xrepl;
    STRING *input = string(target);
    STRING *output;
    STRING *buffer;
    STRING *sval;
    size_t have;
    size_t used = 0;
    size_t guess = input->len;
    size_t limit = guess;

    int skip0 = -1;
    size_t howmuch;

    TRACE(("called gsub3\n"));

    /*
     * If the replacement is constant, do it only once.
     */
    if (repl->type != C_REPLV) {
	cellcpy(&xrepl, repl);
    } else {
	memset(&xrepl, 0, sizeof(xrepl));
    }

    repl_cnt = 0;
    output = new_STRING0(limit);

    for (j = 0; j <= (int) input->len; ++j) {
	char *where = REmatch(input->str + j,
			      input->len - (size_t) j,
			      cast_to_re(re),
			      &howmuch,
			      (j != 0));
	/*
	 * REmatch returns a non-null pointer if it found a match.  But
	 * that can be an empty string, e.g., for "*" or "?".  The length
	 * is in 'howmuch'.
	 */
	if (where != 0) {
	    have = (size_t) (where - (input->str + j));
	    if (have) {
		skip0 = -1;
		TRACE(("..before match:%d:", (int) have));
		TRACE_STRING2(input->str + j, have);
		TRACE(("\n"));
		memcpy(output->str + used, input->str + j, have);
		used += have;
	    }

	    TRACE(("REmatch %d vs %d len=%d:", (int) j, skip0, (int) howmuch));
	    TRACE_STRING2(where, howmuch);
	    TRACE(("\n"));

	    if (repl->type == C_REPLV) {
		if (xrepl.ptr == 0 ||
		    string(&xrepl)->len != howmuch ||
		    (howmuch != 0 &&
		     memcmp(string(&xrepl)->str, where, howmuch))) {
		    if (xrepl.ptr != 0)
			repl_destroy(&xrepl);
		    sval = new_STRING1(where, howmuch);
		    cellcpy(&xrepl, repl);
		    replv_to_repl(&xrepl, sval);
		    free_STRING(sval);
		}
	    }

	    have = string(&xrepl)->len;
	    TRACE(("..replace:"));
	    TRACE_STRING2(string(&xrepl)->str, have);
	    TRACE(("\n"));

	    if (howmuch || (j != skip0)) {
		++repl_cnt;

		/*
		 * If this new chunk is longer than its replacement, add that
		 * to the estimate of the length.  Then, if the estimate goes
		 * past the allocated length, reallocate and copy the existing
		 * data.
		 */
		if (have > howmuch) {	/* growing */
		    guess += (have - howmuch);
		    if (guess >= limit) {
			buffer = output;
			limit = (++guess) * 2;	/* FIXME - too coarse? */
			output = new_STRING0(limit);
			memcpy(output->str, buffer->str, used);
			free_STRING(buffer);
		    }
		} else if (howmuch > have) {	/* shrinking */
		    guess -= (howmuch - have);
		}

		/*
		 * Finally, copy the new chunk.
		 */
		memcpy(output->str + used, string(&xrepl)->str, have);
		used += have;
	    }

	    if (howmuch) {
		j = (int) ((size_t) (where - input->str) + howmuch) - 1;
	    } else {
		j = (int) (where - input->str);
		if (j < (int) input->len) {
		    TRACE(("..emptied:"));
		    TRACE_STRING2(input->str + j, 1);
		    TRACE(("\n"));
		    output->str[used++] = input->str[j];
		}
	    }
	    skip0 = (howmuch != 0) ? (j + 1) : -1;
	} else {
	    have = (input->len - (size_t) j);
	    TRACE(("..after match:%d:", (int) have));
	    TRACE_STRING2(input->str + j, have);
	    TRACE(("\n"));
	    memcpy(output->str + used, input->str + j, have);
	    used += have;
	    break;
	}
    }

    TRACE(("..input %d ->output %d\n",
	   (int) input->len,
	   (int) output->len));

    repl_destroy(&xrepl);
    if (output->len > used) {
	buffer = output;
	output = new_STRING1(output->str, used);
	free_STRING(buffer);
    }
    TRACE(("..done gsub3\n"));
    return output;
}

/* set up for call to gsub() */
CELL *
bi_gsub(CELL *sp)
{
    CELL *cp;			/* pts at the replacement target */
    CELL sc;			/* copy of replacement target */
    CELL tc;			/* build the result here */
    STRING *result;

    TRACE_FUNC("bi_gsub", sp);

    sp -= 2;

    if (sp->type != C_RE)
	cast_to_RE(sp);
    if ((sp + 1)->type != C_REPL && (sp + 1)->type != C_REPLV)
	cast_to_REPL(sp + 1);

    cellcpy(&sc, cp = (CELL *) (sp + 2)->ptr);
    if (sc.type < C_STRING)
	cast1_to_s(&sc);

    TRACE(("..actual gsub args:\n"));
    TRACE(("arg0: "));
    TRACE_CELL(sp);
    TRACE(("arg1: "));
    TRACE_CELL(sp + 1);
    TRACE(("arg2: "));
    TRACE_CELL(&sc);

    result = gsub3(sp->ptr, sp + 1, &sc);
    tc.ptr = (PTR) result;

    if (repl_cnt) {
	tc.type = C_STRING;
	slow_cell_assign(cp, &tc);
    }

    sp->type = C_DOUBLE;
    sp->dval = (double) repl_cnt;

    TRACE(("Result: "));
    TRACE_CELL(sp);
    TRACE(("String: "));
    TRACE_STRING(result);
    TRACE(("\n"));

    /* cleanup */
    free_STRING(string(&sc));
    free_STRING(string(&tc));
    repl_destroy(sp + 1);

    return_CELL("bi_gsub", sp);
}
