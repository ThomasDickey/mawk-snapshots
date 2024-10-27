BEGIN {
	upper_bound    = 1024
	total_integers = 0
	step           = 1
	for (i = 0; i < upper_bound; i += step) {
		integer_sequence[++total_integers] = i
		step++
	}

	bit_operator = ARGV[1]
	if (bit_operator != "and" && bit_operator != "or" && bit_operator != "xor") {
		print "Unrecognized logical bit operator: " bit_operator
		exit
	}
	for (i = 1; i <= total_integers; i++) {
		for (j = 1; j <= total_integers; j++) {
			if (bit_operator == "and")
				printf("%d;", and(integer_sequence[i], integer_sequence[j]))
			else if (bit_operator == "or")
				printf("%d;", or(integer_sequence[i], integer_sequence[j]))
			else
				printf("%d;", xor(integer_sequence[i], integer_sequence[j]))
		}
	}
	print ""
	for (i = 1; i <= total_integers; i++) {
		for (j = 1; j <= total_integers; j++) {
			for (k = 1; k <= total_integers; k++) {
				if (bit_operator == "and")
					printf("%d;", and(integer_sequence[i], integer_sequence[j], integer_sequence[k]))
				else if (bit_operator == "or")
					printf("%d;", or(integer_sequence[i], integer_sequence[j], integer_sequence[k]))
				else
					printf("%d;", xor(integer_sequence[i], integer_sequence[j], integer_sequence[k]))
			}
		}
	}
}
