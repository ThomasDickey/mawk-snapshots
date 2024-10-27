BEGIN {
	upper_bound                  = 9223372036854775808
	incremental_step_upper_bound = 32767

	step           = 1
	max_per_row    = 1000
	bit_operator   = ARGV[1] # right now we only support compl()
	for (i = 0; ; i += step) {
		printf("%d;", compl(i))
		if (++j >= max_per_row) {
			print ""
			j = 0
		}
		if (step > incremental_step_upper_bound)
			step *= 2
		else
			step++
		if (i > upper_bound - step)
			break
	}
}
