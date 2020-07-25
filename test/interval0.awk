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
/b{0,0}/ 
/b{0}/ 
/b{,0}/ 
/ab{0,0}c/ 
/ab{0}c/ 
/^ab{0,1}c/ 
/ab{0,1}c/ 
/ab{0,3}c/ 
/ab{0,}c/ 
/b{0,1}c/ 
/b{0,2}c/ 
/b{0,3}c/ 
/b{0,3}/ 
/b{0}c/ 
/b{0,}/ 
/b{0}/ 
/(bbb){0,}/ 
