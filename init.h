/********************************************
init.h
copyright 2009-2012,2016, Thomas E. Dickey
copyright 1991, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: init.h,v 1.6 2016/09/30 23:37:13 tom Exp $
 */

/* init.h  */

#ifndef  INIT_H
#define  INIT_H

#include "symtype.h"

/* nodes to link file names for multiple
   -f option */

typedef struct pfile {
    struct pfile *link;
    char *fname;
} PFILE;

extern PFILE *pfile_list;

extern char *sprintf_buff, *sprintf_limit;

void initialize(int, char **);
void code_init(void);
void code_cleanup(void);
void compile_cleanup(void);
void scan_init(const char *);
void bi_vars_init(void);
void bi_funct_init(void);
void print_init(void);
void kw_init(void);
void field_init(void);
void fpe_init(void);
void load_environ(ARRAY);
void set_stdio(void);

void print_version(void);
int is_cmdline_assign(char *);

#endif /* INIT_H  */
