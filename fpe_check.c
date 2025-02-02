/********************************************
fpe_check.c
copyright 2008-2023,2024 Thomas E. Dickey
copyright 1996, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/* This code attempts to figure out what the default
   floating point exception handling does.
*/

/*
 * $MawkId: fpe_check.c,v 1.19 2024/11/17 22:11:48 tom Exp $
 */

#include "confdefs.h"

#include <stdlib.h>
#include <stdio.h>
#include <setjmp.h>
#include <signal.h>
#include <string.h>
#include <math.h>

#ifdef HAVE_SIGINFO_H
#include <siginfo.h>
#endif

#ifdef HAVE_SIGACTION_SA_SIGACTION
#define FPE_ARGS int sig, siginfo_t *sip, void *data
#define FPE_DECL int why = sip->si_code
#else
#define FPE_ARGS int sig, int why
#define FPE_DECL		/* nothing */
#endif

static void
message(const char *s)
{
    printf("\t%s\n", s);
}

jmp_buf jbuff;
int may_be_safe_to_look_at_why = 0;
int why_v;
int checking_for_strtod_ovf_bug = 0;

static void catch_FPEs(void);
static int is_nan(double);
static void check_strtod_ovf(void);

static double
div_by(double x, double y)
{
    return x / y;
}

static double
overflow(double x)
{
    double y;

    do {
	y = x;
	x *= x;
    } while (y != x);
    return x;
}

static void
check_fpe_traps(void)
{
    int traps = 0;

    if (setjmp(jbuff) == 0) {
	div_by(44.0, 0.0);
	message("division by zero does not generate an exception");
    } else {
	traps = 1;
	message("division by zero generates an exception");
	catch_FPEs();		/* set again if sysV */
    }

    if (setjmp(jbuff) == 0) {
	overflow(1000.0);
	message("overflow does not generate an exception");
    } else {
	traps |= 2;
	message("overflow generates an exception");
	catch_FPEs();
    }

    if (traps == 0) {
	double maybe_nan;

	maybe_nan = sqrt(-8.0);
	if (is_nan(maybe_nan)) {
	    message("math library supports ieee754");
	} else {
	    traps |= 4;
	    message("math library does not support ieee754");
	}
    }

    exit(traps);
}

static int
is_nan(double d)
{
    int result;
    char command[128];

#ifdef HAVE_ISNAN
    result = isnan(d);
#else
    if (!(d == d)) {
	result = 1;
    } else {
	/* on some systems with an ieee754 bug, we need to make another check */
	sprintf(command,
		"echo '%f' | egrep '[nN][aA][nN]|\\?' >/dev/null", d);
	result = system(command) == 0;
    }
#endif
    return result;
}

/*
Only get here if we think we have Berkeley type signals so we can
look at a second argument to fpe_catch() to get the reason for
an exception
*/
static void
get_fpe_codes(void)
{
    int divz = 0;
    int ovf = 0;

    may_be_safe_to_look_at_why = 1;

    if (setjmp(jbuff) == 0)
	div_by(1000.0, 0.0);
    else {
	divz = why_v;
	catch_FPEs();
    }

    if (setjmp(jbuff) == 0)
	overflow(1000.0);
    else {
	ovf = why_v;
	catch_FPEs();
    }

    /* make some guesses if sane values */
    if (divz > 0 && ovf > 0 && divz != ovf) {
	printf("X FPE_ZERODIVIDE %d\n", divz);
	printf("X FPE_OVERFLOW %d\n", ovf);
	exit(0);
    } else
	exit(1);
}

int
main(int argc, char *argv[])
{
#ifdef HAVE_MATH__LIB_VERSION
    _LIB_VERSION = _IEEE_;
#endif

    catch_FPEs();
    switch (argc) {
    case 1:
	check_fpe_traps();
	break;
    case 2:
	get_fpe_codes();
	break;
    default:
	check_strtod_ovf();
	break;
    }
    /* not reached */
    return 0;
}

static RETSIGTYPE
fpe_catch(FPE_ARGS)
{
    FPE_DECL;

    if (checking_for_strtod_ovf_bug)
	exit(1);
    if (may_be_safe_to_look_at_why)
	why_v = why;
    longjmp(jbuff, 1);
}

static void
catch_FPEs(void)
{
#if defined(HAVE_SIGACTION_SA_SIGACTION)
    {
	struct sigaction x;

	memset(&x, 0, sizeof(x));
	x.sa_sigaction = fpe_catch;
	x.sa_flags = SA_SIGINFO;

	sigaction(SIGFPE, &x, (struct sigaction *) 0);
    }
#else
    signal(SIGFPE, fpe_catch);
#endif
}

char longstr[] =
"1234567890\
1234567890\
1234567890\
1234567890\
1234567890\
1234567890\
1234567890\
1234567890\
1234567890\
1234567890\
1234567890\
1234567890\
1234567890\
1234567890\
1234567890\
1234567890\
1234567890\
1234567890\
1234567890\
1234567890\
1234567890\
1234567890\
1234567890\
1234567890\
1234567890\
1234567890\
1234567890\
1234567890\
1234567890\
1234567890\
1234567890\
1234567890\
1234567890\
1234567890\
1234567890\
1234567890\
1234567890\
1234567890\
1234567890\
1234567890";

#ifdef  USE_IEEEFP_H
#include <ieeefp.h>
#endif

static void
check_strtod_ovf(void)
{
#ifdef USE_IEEEFP_H
    fpsetmask(fpgetmask() | FP_X_OFL | FP_X_DZ);
#endif

    checking_for_strtod_ovf_bug = 1;
    strtod(longstr, (char **) 0);
    exit(0);
}
