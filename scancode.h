/********************************************
scancode.h
copyright 2023, Thomas E. Dickey
copyright 2009, Jonathan Nieder
copyright 1991, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: scancode.h,v 1.3 2023/07/22 22:31:02 tom Exp $
 */

/* scancode.h  */

#ifndef  SCANCODE_H_INCLUDED
#define  SCANCODE_H_INCLUDED   1

extern char scan_code[256];

/*  the scan codes to compactify the main switch */

typedef enum {
    SC_SPACE = 1
    ,SC_NL
    ,SC_SEMI_COLON
    ,SC_FAKE_SEMI_COLON
    ,SC_LBRACE
    ,SC_RBRACE
    ,SC_QMARK
    ,SC_COLON
    ,SC_OR
    ,SC_AND
    ,SC_PLUS
    ,SC_MINUS
    ,SC_MUL
    ,SC_DIV
    ,SC_MOD
    ,SC_POW
    ,SC_LPAREN
    ,SC_RPAREN
    ,SC_LBOX
    ,SC_RBOX
    ,SC_IDCHAR
    ,SC_DIGIT
    ,SC_DQUOTE
    ,SC_ESCAPE
    ,SC_COMMENT
    ,SC_EQUAL
    ,SC_NOT
    ,SC_LT
    ,SC_GT
    ,SC_COMMA
    ,SC_DOT
    ,SC_MATCH
    ,SC_DOLLAR
    ,SC_UNEXPECTED
} SCAN_CODES;

#endif /* SCANCODE_H_INCLUDED */
