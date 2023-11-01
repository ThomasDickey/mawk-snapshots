#!/usr/bin/mawk -f
# $MawkId: deps.awk,v 1.4 2023/10/31 22:58:49 tom Exp $
# vile: notab ts=4 sw=4

# find include dependencies in C source
#
# mawk -f deps.awk  C_source_files
#         -- prints a dependency list suitable for make

BEGIN {
    stack_index = 0 # stack[] holds the input files

    for(i = 1 ; i < ARGC ; i++)
    {
        file = ARGV[i]
        if ( file !~ /\.[cC]$/ )  continue  # skip it
        outfile = substr(file, 1, length(file)-2) ".o"

        # INCLUDED[] stores the set of included files
        # -- start with the empty set
        for( j in INCLUDED ) delete INCLUDED[j]

        while ( 1 )
        {
            if ( getline line < file <= 0 )  # no open or EOF
            {
                if ( stack_index == 0 )  break # empty stack
                else
                {
                    file = stack[ stack_index-- ]
                    continue
                }
            }

            if ( line ~ /^#include[ \t]+".*"/ )
                split(line, X, "\"")  # filename is in X[2]
            else if ( line ~ /^#include[ \t]+<.*>/ )
                split(line, X, "[<>]")  # filename is in X[2]
            else
                continue;

            if ( X[2] in INCLUDED ) # we've already included it
                continue
            if ( getline line < X[2] <= 0 )  # no open or EOF
                continue;

            #push current file
            stack[ ++stack_index ] = file
            INCLUDED[ file = X[2] ] = ""
        }  # end of while

        # test if INCLUDED is empty
        flag = 0 # on once the front is printed
        for( j in INCLUDED )
        {
            if ( ! flag )
            { printf "%s : %s" , outfile, j ; flag = 1 }
            else  printf " %s" , j
            close(j);
        }

        if ( flag )  print ""

    }# end of loop over files in ARGV[i]

}
