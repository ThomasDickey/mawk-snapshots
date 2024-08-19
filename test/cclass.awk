# $MawkId: cclass.awk,v 1.3 2024/07/24 08:10:05 tom Exp $
# count character-classes

function count_one(s,r,	rc,regex) {
	rc = 0;
	regex = "[[:" r ":]]+";
	while (match(s,regex)) {
		# printf "\t%s: %s\n", regex, substr(s, RSTART, RLENGTH);
		rc += RLENGTH;
		s = substr(s,RSTART + RLENGTH);
	}
	return rc;
}

function count_all(s) {
	total += length(s);
	_alnum += count_one(s, "alnum");
	_alpha += count_one(s, "alpha");
	_blank += count_one(s, "blank");
	_cntrl += count_one(s, "cntrl");
	_digit += count_one(s, "digit");
	_graph += count_one(s, "graph");
	_lower += count_one(s, "lower");
	_print += count_one(s, "print");
	_punct += count_one(s, "punct");
	_space += count_one(s, "space");
	_upper += count_one(s, "upper");
	_xdigit += count_one(s, "xdigit");
}
function report(name,value) {
	printf("%-8s %*d %5.1f%%\n", name ":", width, value, 100 * value / total);
}
BEGIN{
	total = 0;
	_alnum = 0;
}
/./{ count_all($0); }
/$/{ count_all(RS); }
END{
	if (total == 0) {
		for (c = 0; c < 255; ++c) {
			count_all(sprintf("%c", c));
		}
	}
	printf "total: %d\n", total;
	scale = total;
	width = 1;
	while (scale > 1) {
		scale /= 10;
		width++;
	}
	report("alnum", _alnum);
	report("alpha", _alpha);
	report("blank", _blank);
	report("cntrl", _cntrl);
	report("digit", _digit);
	report("graph", _graph);
	report("lower", _lower);
	report("print", _print);
	report("punct", _punct);
	report("space", _space);
	report("upper", _upper);
	report("xdigit", _xdigit);
}
