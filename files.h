/********************************************
files.h
copyright 2009-2020,2024, Thomas E. Dickey
copyright 1991-1994,1996, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: files.h,v 1.16 2024/08/25 17:17:31 tom Exp $
 */

#ifndef   MAWK_FILES_H
#define   MAWK_FILES_H

#include <nstd.h>
#include <types.h>

/* IO redirection types */
#define  F_IN           (-5)
#define  PIPE_IN        (-4)
#define  PIPE_OUT       (-3)
#define  F_APPEND       (-2)
#define  F_TRUNC        (-1)
#define  IS_OUTPUT(type)  ((type)>=PIPE_OUT)

extern const char *shell;	/* for pipes and system() */

extern PTR file_find(STRING *, int);
extern int file_close(STRING *);
extern int file_flush(STRING *);
extern int flush_all_output(void);
extern PTR get_pipe(char *, int, int *);
extern int wait_for(int);
extern int wait_status(int);
extern void close_out_pipes(void);

#ifdef  HAVE_FAKE_PIPES
extern void close_fake_pipes(void);
extern int close_fake_outpipe(char *, int);
extern char *tmp_file_name(int, char *);
#endif

#ifdef MSDOS
extern int DOSexec(char *);
extern void enlarge_output_buffer(FILE *);
#endif

#if USE_BINMODE
extern int binmode(void);
extern void set_binmode(long);
extern void stdout_init(void);
#endif

#endif /* MAWK_FILES_H */
