# Test-script for MAWK
###############################################################################
# copyright 2026, Thomas E. Dickey
#
# This is a source file for mawk, an implementation of
# the AWK programming language.
#
# Mawk is distributed without warranty under the terms of
# the GNU General Public License, version 2, 1991.
###############################################################################
function test(x) {
	printf "testing \"%f\"\n", x;
	print x;
	print "..compare to self";
	print (x == x);
	print (x != x);
	print (x <  x);
	print (x <= x);
	print (x >  x);
	print (x >= x);
	print "..compare to zero";
	print (x == 0);
	print (x != 0);
	print (x <  0);
	print (x <= 0);
	print (x >  0);
	print (x >= 0);
	print "..compare from zero";
	print (0 == x);
	print (0 != x);
	print (0 <  x);
	print (0 <= x);
	print (0 >  x);
	print (0 >= x);
	print "..functions";
	print atan2(0,x);
	print atan2(x,0);
	print cos(x);
	print exp(x);
	print int(x);
	print log(x);
	print sin(x);
	print sqrt(x)
}
BEGIN {
	test("-NaN");
	test("+NaN");
	test("+NaN" + 0);
	test("-Inf");
	test("-Inf" + "-Nan" );
	test("-Inf" * "-Nan" );
	test("-Inf" / "-Nan" );
	test("-Inf" % "-Nan" );
	test("+Inf");
	test("+Inf" + 0);
	test("+Inf" + "-Nan" );
	test("+Inf" * "-Nan" );
	test("+Inf" / "-Nan" );
	test("+Inf" % "-Nan" );
}
