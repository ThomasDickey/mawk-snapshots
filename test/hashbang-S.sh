#!/usr/bin/env -S mawk -Wv
# On some systems, the "env" utility adds a "-S" option which lets the above
# line run mawk and pass following tokens as options rather than as part of
# the command-name.
#
# The "-S" option came from FreeBSD in 2005, was adopted by coreutils in 2018.
#
# As of August 2024, it is also recognized by MacOS (seen in its manpage for
# March 2021), but not NetBSD, OpenBSD, or busybox.
#
# FreeBSD:
# commit 8fbe7ebf7d04da629529ae7d2f662a2c46d61b97
# Author: Garance A Drosehn <gad@FreeBSD.org>
# Date:   Mon Jun 20 03:43:25 2005 +0000
#
#     Add the '-S' and '-P' options.  The '-S' option can be used to split
#     apart a string, and supports some text substitutions.  This can be
#     used to provide all the flexibility (and more!) that was lost by recent
#     changes to how the kernel parses #!-lines in shell scripts.
#
#     The '-P' option provides a way to specify an alternate set of directories
#     to use when searching for the 'utility' program to run.  This way you can
#     be sure what directories are used for that search, without changing the
#     value of PATH that the user has set.  Note that on FreeBSD 6.0, this
#     option is worthless unless the '-S' option is also used.
#
#     Approved by:    re (blanket `env')
#
# coreutils:
# commit 668306ed86c8c79b0af0db8b9c882654ebb66db2
# Author: Assaf Gordon <assafgordon@gmail.com>
# Date:   Fri Apr 20 20:58:28 2018 -0600
# 
#     env: add -S/--split-string option
# 
#     Adopted from FreeBSD's env(1), useful for specifing multiple
#     parameters on a shebang (#!) script line, e.g:
# 
#        #!/usr/bin/env -S perl -w -T
# 
#     Discussed in https://lists.gnu.org/r/coreutils/2018-04/msg00011.html
# 
#     * src/env.c (valid_escape_sequence,escape_char,scan_varname,
#       extract_varname,validate_split_str,build_argv,
#       parse_split_string): New functions.
#       (main): Process new option and call parse_split_string.
#       (usage): Mention new option.
#     * tests/misc/env-S.pl: Test new option from the command line.
#     * tests/misc/env-S-script.sh: Test new option from shebang scripts.
#     * tests/local.mk (all_tests): Add new tests.
#     * man/env.x (OPTIONS): Show a brief example of -S usage and point to
#       the full documentation for more information.
#     * doc/coreutils.texi (env invocation): Detail usage of -S/--split-string
#       option.
#     * NEWS: Mention new option.
