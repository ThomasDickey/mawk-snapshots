/********************************************
mawk.h
copyright 2008-2024,2025 Thomas E. Dickey
copyright 1991-1995,1996 Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: mawk.h,v 1.78 2025/01/30 09:02:31 tom Exp $
 */

/*  mawk.h  */

#ifndef  MAWK_H
#define  MAWK_H

#include <nstd.h>

#include <stdio.h>
#include <stdarg.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <assert.h>

#ifdef HAVE_STDNORETURN_H
#include <stdnoreturn.h>
#undef GCC_NORETURN
#define GCC_NORETURN STDC_NORETURN
#endif

#include <repl.h>
#include <types.h>
#include <makebits.h>

#ifndef GCC_NORETURN
#define GCC_NORETURN		/* nothing */
#endif

#ifndef GCC_PRINTFLIKE
#define  GCC_PRINTFLIKE(fmt,var)	/* nothing */
#endif

#ifndef GCC_UNUSED
#define GCC_UNUSED		/* nothing */
#endif

#if defined(__GNUC__) && defined(_FORTIFY_SOURCE)
#define IGNORE_RC(func) ignore_unused = (int) func
extern int ignore_unused;
#else
#define IGNORE_RC(func) (void) func
#endif /* gcc workarounds */

#ifdef   DEBUG
#define  YYDEBUG  1
extern int yydebug;		/* print parse if on */
extern int dump_RE;
#endif

#if defined(MSDOS) || defined(__MINGW32__) || defined(_WINNT)
#define USE_BINMODE 1
#else
#define USE_BINMODE 0
#endif

extern short interactive_flag;
extern short posix_space_flag;
extern short traditional_flag;

#ifndef NO_INTERVAL_EXPR
extern short repetitions_flag;
#endif

/*----------------
 *  GLOBAL VARIABLES
 *----------------*/

/* a well known string */
extern STRING null_str;

/* a useful scratch area */
extern char string_buff[SPRINTF_LIMIT];

/* help with casts */
extern const int mpow2[];

 /* these are used by the parser, scanner and error messages
    from the compile  */

extern char *pfile_name;	/* program input file */
extern int current_token;
extern unsigned token_lineno;	/* lineno of current token */
extern unsigned compile_error_count;
extern int NR_flag;
extern int paren_cnt;
extern int brace_cnt;
extern int print_flag, getline_flag;
extern short mawk_state;
#define EXECUTION       1	/* other state is 0 compiling */

#ifdef LOCALE
extern char decimal_dot;
#endif

extern const char *progname;	/* for error messages */
extern unsigned rt_nr, rt_fnr;	/* ditto */

#define TABLESIZE(name) (sizeof(name)/sizeof(name[0]))

/* macro to test the type of two adjacent cells */
#define TEST2(cp)  (mpow2[(cp)->type]+mpow2[((cp)+1)->type])

/* macro to get at the string part of a CELL */
#define string(cp) ((STRING *)(cp)->ptr)

#ifdef   DEBUG
#define cell_destroy(cp)  DB_cell_destroy(cp)
#else
/* Note: type is only C_STRING to C_MBSTRN */
#define cell_destroy(cp) \
	do { \
	    if ( (cp)->type >= C_STRING && \
	         (cp)->type <= C_MBSTRN && \
		 string(cp) != NULL) { \
	        unsigned final = string(cp)->ref_cnt; \
		free_STRING(string(cp));  \
		if (final <= 1) { \
		    (cp)->ptr = NULL; \
		} \
	    } \
	} while (0)
#endif

/*  prototypes  */

extern void cast1_to_s(CELL *);
extern void cast1_to_d(CELL *);
extern void cast2_to_s(CELL *);
extern void cast2_to_d(CELL *);
extern void cast_to_RE(CELL *);
extern void cast_for_split(CELL *);
extern void check_strnum(CELL *);
extern void cast_to_REPL(CELL *);
extern Int d_to_I(double);
extern Long d_to_L(double);
extern ULong d_to_UL(double d);
extern double l_to_D(Long l);
extern double ul_to_D(ULong l);

#define NonNull(s)    ((s) == NULL ? "<null>" : (s))

#define d_to_i(d)     ((int)d_to_I(d))
#define d_to_l(d)     ((long)d_to_L(d))

#define IsMaxBound(n) ((n) == UNSIGNED_LIMITS || (n) == INTEGERS_LIMITS)
#define PastBound(n)  ((n) > UNSIGNED_LIMITS)

extern int test(CELL *);	/* test for null non-null */
extern CELL *cellcpy(CELL *, CELL *);
extern CELL *repl_cpy(CELL *, CELL *);
extern void DB_cell_destroy(CELL *);
extern GCC_NORETURN void overflow(const char *, unsigned);
extern GCC_NORETURN void rt_overflow(const char *, unsigned);
extern GCC_NORETURN void rt_error(const char *,...) GCC_PRINTFLIKE(1,2);
extern GCC_NORETURN void mawk_exit(int);
extern void da(INST *, FILE *);
extern INST *da_this(INST *, const INST *, FILE *);
extern char *rm_escape(char *, size_t *);
extern char *re_pos_match(char *, size_t, RE_NODE *, size_t *, int);
extern char *safe_string(char *);
extern int binmode(void);

#ifndef  REXP_H
extern char *str_str(char *, size_t, const char *, size_t);
#endif

extern void parse(void);
extern void scan_cleanup(void);

#ifndef YYBYACC
extern int yylex(void);
#endif
extern void yyerror(const char *);

extern GCC_NORETURN void bozo(const char *);
extern void errmsg(int, const char *, ...) GCC_PRINTFLIKE(2,3);
extern void compile_error(const char *, ...) GCC_PRINTFLIKE(1,2);

extern void execute(INST *, CELL *, CELL *);
extern const char *find_kw_str(int);
extern void da_string(FILE *fp, const STRING *, int);
extern void da_string2(FILE *fp, const char *, size_t, int);

#ifdef HAVE_STRTOD_OVF_BUG
extern double strtod_with_ovf_bug(const char *, char **);
#define strtod  strtod_with_ovf_bug
#endif

#ifndef OPT_CALLX
#define OPT_CALLX 0
#endif

#if OPT_TRACE > 0
extern FILE *trace_fp;
extern void Trace(const char *, ...) GCC_PRINTFLIKE(1,2);
extern void TraceVA(const char *, va_list);
#define TRACE(params) Trace params
#if OPT_TRACE > 1
#define TRACE2(params) Trace params
#endif
#endif

#ifndef TRACE
#define TRACE(params)		/* nothing */
#endif

#ifndef TRACE2
#define TRACE2(params)		/* nothing */
#endif

#if OPT_TRACE > 0
extern void TraceCell(CELL *);
extern void TraceString(STRING *);
extern void TraceString2(const char *, size_t);
#define TRACE_CELL(cp)		TraceCell(cp)
#define TRACE_STRING(cp)	TraceString(cp)
#define TRACE_STRING2(str,len)	TraceString2(str,len)
#else
#define TRACE_CELL(cp)		/* nothing */
#define TRACE_STRING(cp)	/* nothing */
#define TRACE_STRING2(str,len)	/* nothing */
#endif

#if OPT_TRACE > 0
extern void TraceFunc(const char *, CELL *, int);
#define TRACE_FUNC2(name,cp,na) TraceFunc(name,cp,na)
#define TRACE_FUNC(name,cp) TraceFunc(name,cp,cp->type)
#else
#define TRACE_FUNC2(name,cp,na)	/* nothing */
#define TRACE_FUNC(name,cp)	/* nothing */
#endif

#if OPT_TRACE > 0
extern void TraceInst(INST *, INST *);
#define TRACE_INST(cp,base) TraceInst(cp,base)
#else
#define TRACE_INST(cp,base)	/* nothing */
#endif

extern const char *da_type_name(const CELL *);
extern const char *da_op_name(const INST *);

#ifdef NO_LEAKS

extern void free_cell_data(CELL *);
extern void free_codes(const char *, INST *, size_t);
extern void no_leaks_cell(CELL *);
extern void no_leaks_cell_ptr(CELL *);
extern void no_leaks_re_ptr(PTR);

extern void array_leaks(void);
extern void bi_vars_leaks(void);
extern void cell_leaks(void);
extern void code_leaks(void);
extern void field_leaks(void);
extern void files_leaks(void);
extern void fin_leaks(void);
extern void hash_leaks(void);
extern void re_leaks(void);
extern void rexp_leaks(void);
extern void scan_leaks(void);
extern void trace_leaks(void);
extern void zmalloc_leaks(void);

#else

#define free_codes(tag, base, size) zfree(base, size)
#define no_leaks_cell(ptr)	/* nothing */
#define no_leaks_cell_ptr(ptr)	/* nothing */
#define no_leaks_re_ptr(ptr)	/* nothing */

#endif

/*
 * Sometimes split_buff[] pointers are moved rather than copied.
 * Optimize-out the assignment to clear the pointer in the array.
 */
#ifdef NO_LEAKS
#define USED_SPLIT_BUFF(n) split_buff[n] = NULL
#else
#define USED_SPLIT_BUFF(n)	/* nothing */
#endif

#endif /* MAWK_H */
