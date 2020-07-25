# $MawkId: repetitions.awk,v 1.3 2020/07/25 14:55:28 tom Exp $
# Test-script for MAWK
###############################################################################
# copyright 2016, jlp765
#
# This is a source file for mawk, an implementation of
# the AWK programming language.
#
# Mawk is distributed without warranty under the terms of
# the GNU General Public License, version 2, 1991.
###############################################################################
/b{,1}/ 
/ab{1,3}c/ 
/^ab{1,}c/ 
/ab{1,}c/ 
/ab{1}c/ 
/ab{2,3}c/ 
/a(b){2}c/ 
/a(b{2})c/ 
/ab{2}c/ 
/(ab){2,}c/ 
/ab{2,}c/ 
/ab{3}c/ 
/ab{3,5}c/ 
/(abb){1,}/ 
/b{1,}/ 
/b{2,}c/ 
/(b){2,}c/ 
/b{6,}c/ 
/b{7,}c/ 
/(bb){1,}c/ 
/(bb){1}c/ 
/bb{1}c/ 
/(bb){2,}c/ 
/(bb){2}c/ 
/bb{2}c/ 
/(bbb){1,}/ 
/(qwerty){4}/
/(qwerty){5}/
/(wabxcz){220}w/ 
/(wabxcz){220}/ 
/(wabxcz){221}/ 
/wabxcz{219}w/
