#!/bin/sh
# $MawkId: longline.sh,v 1.3 2024/08/19 23:15:25 tom Exp $
###############################################################################
# copyright 2024, Thomas E. Dickey
#
# This is a source file for mawk, an implementation of
# the AWK programming language.
#
# Mawk is distributed without warranty under the terms of
# the GNU General Public License, version 2, 1991.
###############################################################################

# Just test this with Linux -
# BSDs and Solaris run out of memory, perhaps due to pipe implementation.
if [ ! -f /proc/meminfo ]
then
	echo "SKIPPED because of memory requirements"
	exit
fi

: "${AWK=./mawk}"
case $AWK in
*mawk)
	OPT_O=-Ws=0x20000
	OPT_I=-Wi
	;;
*)
	OPT_O=
	OPT_I=
	;;
esac
$AWK $OPT_O 'BEGIN { record = ""; min = 0; max = 19994; for ( p = min; p < max; ++p) { record = record "#"; printf "%d:%s\n", p, record; } }' | \
$AWK $OPT_O $OPT_I 'BEGIN { widest = total = 0; }{ this = length($0); total += this; if (this > widest) widest = this; } END { print total, widest }'
