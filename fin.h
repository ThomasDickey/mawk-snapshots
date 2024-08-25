/********************************************
fin.h
copyright 2009-2023,2024, Thomas E. Dickey
copyright 1991-1992,1993, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: fin.h,v 1.16 2024/08/25 17:11:16 tom Exp $
 */

/* fin.h */

#ifndef  FIN_H
#define  FIN_H

#include <stdio.h>

/* structure to control input files */

typedef struct _fin
#ifdef Visible_FIN
{
    int fd;			/* file-descriptor */
    FILE *fp;			/* NULL unless interactive */
    char *buff;			/* base of data read from file */
    char *buffp;		/* current position to read-next */
    char *limit;		/* points past the data in *buff */
    size_t buff_size;		/* allocated size of buff[] */
    int flags;
}
#endif
FIN;

#define  MAIN_FLAG    1		/* part of main input stream if on */
#define  EOF_FLAG     2
#define  START_FLAG   4		/* used when RS == "" */
#define  FIN_FLAG     8		/* set if fin->buff is no longer beginning */

extern FIN *FINdopen(int, int);
extern FIN *FINopen(char *, int);
extern void FINclose(FIN *);
extern void FINsemi_close(FIN *);
extern char *FINgets(FIN *, size_t *);
extern size_t fillbuff(int, char *, size_t);
extern void open_main(void);

extern FIN *main_fin;		/* for the main input stream */

#endif /* FIN_H */
