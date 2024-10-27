BEGIN {
	upper_bound    = 1024
	total_integers = 0
	step           = 1
	for (i = 0; i < upper_bound; i += step) {
		integer_sequence[++total_integers] = i
		step                               += 1
	}

	shift_operator = ARGV[1]
	if (shift_operator != "lshift" && shift_operator != "rshift") {
		print "Unrecognized shift operator: " shift_operator
		exit
	}
	for (i = 1; i <= total_integers; i++) {
		for (j = 0; j <= 32; j++) {
			printf("%d;", (shift_operator == "lshift" ? lshift(integer_sequence[i], j) : rshift(integer_sequence[i], j)))
		}
	}
}
