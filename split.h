/********************************************
split.h
copyright 2014,2023 Thomas E. Dickey
copyright 2014, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: split.h,v 1.4 2023/07/23 08:58:07 tom Exp $
 */

/* split.h */

#ifndef MAWK_SPLIT_H
#define MAWK_SPLIT_H

extern size_t null_split(const char *s, size_t slen);
extern size_t re_split(char *s, size_t slen, PTR re);
extern size_t space_split(const char *s, size_t slen);
extern void transfer_to_array(CELL cp[], size_t cnt);
extern void transfer_to_fields(size_t cnt);

#endif /* MAWK_SPLIT_H */
