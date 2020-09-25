#!/usr/bin/mawk -f
# $MawkId: eatc.awk,v 1.3 2020/09/19 11:51:23 tom Exp $
#  eatc.awk
#  another program to remove comments

{  while( ( t = index($0 , "/*") ) > 0 )
   {
     if ( t > 1 )
       printf "%s" , substr($0,1,t-1)
     $0 = eat_comment( ( t + 2 <= length($0) ) ? substr($0, t+2) : "" )
   }

   print
}


function eat_comment(s,		t2)
{
  #replace comment by one space
  printf " "

  while ( (t2 = index(s, "*/")) == 0 )
	if ( getline s == 0 )
	{ # input error -- unterminated comment
          system("/bin/sh -c 'echo unterminated comment' 1>&2")
	  exit 1
	}

  return  ( t2 + 2 <= length(s) ) ? substr(s,t2+2) : ""
}
