/********************************************
version.c
copyright 2008-2024,2025   Thomas E. Dickey
copyright 1991-1996,2014   Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: version.c,v 1.39 2025/01/20 20:37:58 tom Exp $
 */

#include <mawk.h>
#include <init.h>
#include <patchlev.h>

#define	 VERSION_STRING	 \
  "mawk %d.%d%s %s\n\
Copyright 2008-2024,2025, Thomas E. Dickey\n\
Copyright 1991-1996,2014, Michael D. Brennan\n\n"

#define FMT_N "%-20s%.0f\n"
#define FMT_S "%-20s%s\n"

/* print VERSION and exit */
void
print_version(FILE *fp)
{
    fprintf(fp, VERSION_STRING, PATCH_BASE, PATCH_LEVEL, PATCH_STRING, DATE_STRING);
    fflush(fp);

#define SHOW_RANDOM "random-funcs:"
#if defined(NAME_RANDOM)
    fprintf(fp, FMT_S, SHOW_RANDOM, NAME_RANDOM);
#else
    fprintf(fp, FMT_S, SHOW_RANDOM, "internal");
#endif

#define SHOW_REGEXP "regex-funcs:"
#ifdef LOCAL_REGEXP
    fprintf(fp, FMT_S, SHOW_REGEXP, "internal");
#else
    fprintf(fp, FMT_S, SHOW_REGEXP, "external");
#endif

    fprintf(fp, "\ncompiled limits:\n");
    fprintf(fp, FMT_N, "sprintf buffer", (double) SPRINTF_LIMIT);
    fprintf(fp, FMT_N, "maximum-integer", (double) MAX__INT);
#if defined(VERBOSE_VERSION)
    /* we could show these, but for less benefit: */
    fprintf(fp, FMT_N, "maximum-unsigned", (double) MAX__UINT);
    fprintf(fp, FMT_N, "maximum-long", (double) MAX__LONG);
    fprintf(fp, "\nactual limits:\n");
    fprintf(fp, FMT_N, "sprintf buffer", (double) (sprintf_limit - sprintf_buff));
#endif
    mawk_exit(0);
}
