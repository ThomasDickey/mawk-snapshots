/********************************************
fin.h
copyright 2009-2014,2023 Thomas E. Dickey
copyright 1991-1992,1993, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: fin.h,v 1.14 2023/07/22 22:27:30 tom Exp $
 */

/* fin.h */

#ifndef  FIN_H
#define  FIN_H

#include <stdio.h>

/* structure to control input files */

typedef struct {
    int fd;			/* file-descriptor */
    FILE *fp;			/* NULL unless interactive */
    char *buff;			/* base of data read from file */
    char *buffp;		/* current position to read-next */
    char *limit;		/* points past the data in *buff */
    unsigned nbuffs;		/* sizeof *buff in BUFFSZs */
    int flags;
} FIN;

#define  MAIN_FLAG    1		/* part of main input stream if on */
#define  EOF_FLAG     2
#define  START_FLAG   4		/* used when RS == "" */
#define  FIN_FLAG     8		/* set if fin->buff is no longer beginning */

FIN *FINdopen(int, int);
FIN *FINopen(char *, int);
void FINclose(FIN *);
void FINsemi_close(FIN *);
char *FINgets(FIN *, size_t *);
size_t fillbuff(int, char *, size_t);

extern FIN *main_fin;		/* for the main input stream */
void open_main(void);

#endif /* FIN_H */
