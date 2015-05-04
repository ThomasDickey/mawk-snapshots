# $MawkId: Makefile.in,v 1.1 2015/05/03 23:10:06 tom Exp $
###############################################################################
# copyright 2010,2015 Thomas E. Dickey
#
# This is a source file for mawk, an implementation of
# the AWK programming language.
#
# Mawk is distributed without warranty under the terms of
# the GNU General Public License, version 2, 1991.
###############################################################################
# produce alternate forms from mawk's documentation.

SHELL=/bin/sh

PROG	= mawk
manext	= 1

.SUFFIXES : .html .$(manext) .man .ps .pdf .doc .txt

.$(manext).html :
	../man2html.tmp $* $(manext) man >$@

.$(manext).ps :
	$(SHELL) -c "tbl $*.$(manext) | groff -man" >$@

.$(manext).doc :
	GROFF_NO_SGR=stupid $(SHELL) -c "tbl $*.$(manext) | nroff -Tascii -man" >$@

.$(manext).txt :
	GROFF_NO_SGR=stupid $(SHELL) -c "tbl $*.$(manext) | nroff -Tascii -man | col -bx" >$@

.ps.pdf :
	ps2pdf $*.ps

ALL	= \
	$(PROG).html \
	$(PROG).pdf \
	$(PROG).ps \
	$(PROG).txt \

all:	$(PROG).doc $(ALL)

clean:
	-rm -f $(ALL)

distclean: clean
	rm -f Makefile

maintainer-clean:

$(PROG).pdf : $(PROG).ps
