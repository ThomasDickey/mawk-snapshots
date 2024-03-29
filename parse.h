#ifndef _yy_defines_h_
#define _yy_defines_h_

#define UNEXPECTED 257
#define BAD_DECIMAL 258
#define NL 259
#define SEMI_COLON 260
#define LBRACE 261
#define RBRACE 262
#define LBOX 263
#define RBOX 264
#define COMMA 265
#define IO_OUT 266
#define ASSIGN 267
#define ADD_ASG 268
#define SUB_ASG 269
#define MUL_ASG 270
#define DIV_ASG 271
#define MOD_ASG 272
#define POW_ASG 273
#define QMARK 274
#define COLON 275
#define OR 276
#define AND 277
#define IN 278
#define MATCH 279
#define EQ 280
#define NEQ 281
#define LT 282
#define LTE 283
#define GT 284
#define GTE 285
#define CAT 286
#define GETLINE 287
#define PLUS 288
#define MINUS 289
#define MUL 290
#define DIV 291
#define MOD 292
#define NOT 293
#define UMINUS 294
#define IO_IN 295
#define PIPE 296
#define POW 297
#define INC_or_DEC 298
#define DOLLAR 299
#define FIELD 300
#define LPAREN 301
#define RPAREN 302
#define DOUBLE 303
#define STRING_ 304
#define RE 305
#define ID 306
#define D_ID 307
#define FUNCT_ID 308
#define BUILTIN 309
#define LENGTH 310
#define PRINT 311
#define PRINTF 312
#define SPLIT 313
#define MATCH_FUNC 314
#define SUB 315
#define GSUB 316
#define DO 317
#define WHILE 318
#define FOR 319
#define BREAK 320
#define CONTINUE 321
#define IF 322
#define ELSE 323
#define DELETE 324
#define BEGIN 325
#define END 326
#define EXIT 327
#define NEXT 328
#define NEXTFILE 329
#define RETURN 330
#define FUNCTION 331
#ifdef YYSTYPE
#undef  YYSTYPE_IS_DECLARED
#define YYSTYPE_IS_DECLARED 1
#endif
#ifndef YYSTYPE_IS_DECLARED
#define YYSTYPE_IS_DECLARED 1
typedef union YYSTYPE{
  CELL     *cp ;
  SYMTAB   *stp ;
  int      start ;   /* code starting address as offset from code_base */
  PF_CP    fp ;      /* ptr to a (print/printf) or (sub/gsub) function */
  const BI_REC *bip ; /* ptr to info about a builtin */
  FBLOCK   *fbp  ;   /* ptr to a function block */
  ARG2_REC *arg2p ;
  CA_REC   *ca_p  ;
  int      ival ;
  PTR      ptr ;
} YYSTYPE;
#endif /* !YYSTYPE_IS_DECLARED */
extern YYSTYPE yylval;

#endif /* _yy_defines_h_ */
