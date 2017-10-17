/********************************************
main.c
copyright 2009-2014,2017 Thomas E. Dickey
copyright 1991-1995,2014, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: main.c,v 1.31 2017/10/17 00:41:48 tom Exp $
 */

/*  main.c  */

#include "mawk.h"
#include "bi_vars.h"
#include "init.h"
#include "code.h"
#include "files.h"

#ifdef LOCALE
#include <locale.h>
#endif

short mawk_state;		/* 0 is compiling */
int exit_code;

#if defined(__GNUC__) && defined(_FORTIFY_SOURCE)
int ignore_unused;
#endif

#ifdef LOCALE
char decimal_dot;
#endif

int
main(int argc, char **argv)
{
#ifdef LOCALE
    setlocale(LC_CTYPE, "");
    setlocale(LC_NUMERIC, "C");
#endif
    initialize(argc, argv);
#ifdef LOCALE
    {
	struct lconv *data;

	decimal_dot = '\0';	/* only set to nonzero if not POSIX '.' */
	setlocale(LC_NUMERIC, "");
	data = localeconv();
	if (data != 0
	    && data->decimal_point != 0
	    && strlen(data->decimal_point) == 1) {
	    decimal_dot = data->decimal_point[0];
	} else {
	    /* back out of this if we cannot handle it */
	    setlocale(LC_NUMERIC, "C");
	}
	if (decimal_dot == '.')
	    decimal_dot = 0;
    }
#endif

    parse();

    mawk_state = EXECUTION;
    execute(execution_start, eval_stack - 1, 0);
    /* never returns */
    return 0;
}

void
mawk_exit(int x)
{
#ifdef  HAVE_REAL_PIPES
    close_out_pipes();		/* actually closes all output */
#else
#ifdef  HAVE_FAKE_PIPES
    close_fake_pipes();
#endif
#endif

#ifdef NO_LEAKS
    code_leaks();
    scan_leaks();
    cell_leaks();
    re_leaks();
    rexp_leaks();
    bi_vars_leaks();
    hash_leaks();
    array_leaks();
    files_leaks();
    fin_leaks();
    field_leaks();
    zmalloc_leaks();
#if OPT_TRACE > 0
    trace_leaks();
#endif
#endif

    exit(x);
}
