#!/bin/sh
# $MawkId: longline.sh,v 1.6 2024/08/25 21:45:19 tom Exp $
###############################################################################
# copyright 2024, Thomas E. Dickey
#
# This is a source file for mawk, an implementation of
# the AWK programming language.
#
# Mawk is distributed without warranty under the terms of
# the GNU General Public License, version 2, 1991.
###############################################################################

: "${AWK=./mawk}"
case $AWK in
*mawk*)
	OPT_O=-Ws=0x20000
	OPT_I=-Wi
	;;
*)
	OPT_O=
	OPT_I=
	;;
esac
$AWK $OPT_O 'BEGIN {
	chunk = sprintf("%*s", 2 * 769, "#");
	gsub(" ","#",chunk);
	for ( p = 0; p < 23; ++p) {
		record = record chunk;
		printf "%s\n", record;
	}
}' | \
$AWK $OPT_O $OPT_I 'BEGIN {
	widest = total = 0;
}
{
	this = length($0);
	total += this;
	if (this > widest) widest = this;
}
END {
	print total, widest
}'
