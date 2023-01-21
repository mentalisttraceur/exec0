#!/bin/sh -
no_arguments=': need command or option argument' &&
no_such=': No such file or directory'
s=`! exec0 2>&1` && l=`! exec0 -- 2>&1` && test x"$s" = x"$l" &&
test x"$s" = x'exec0'"$no_arguments" &&
s=`exec0 -h 2>&1` && l=`exec0 --help 2>&1` && test x"$s" = x"$l" &&
s=`exec0 -V 2>&1` && l=`exec0 --version 2>&1` && test x"$s" = x"$l" &&
exec0 --version | sed '1 q' | grep -qE '^exec0 [0-9]+\.[0-9]*\.[0-9]$' &&
test x"`exec0 --version | sed '1 d'`" = x &&
error=`! exec0 -x 2>&1` && test "$error" = 'exec0: unrecognized option: -x' &&
s=`! exec0 exec0 2>&1` && test x"$s" = x"$no_arguments" &&
s=`! exec0 -- exec0 2>&1` && test x"$s" = x"$no_arguments" &&
s=`! exec0 exec0 q 2>&1` && test x"$s" = xq"$no_arguments" &&
s=`! exec0 exec0 q -- 2>&1` && test x"$s" = xq"$no_arguments" &&
s=`! exec0 -- exec0 q 2>&1` && test x"$s" = xq"$no_arguments" &&
s=`! exec0 -- exec0 q -- 2>&1` && test x"$s" = xq"$no_arguments" &&
s=`! exec0 exec0 -- 2>&1` && test x"$s" = x--"$no_arguments" &&
s=`! exec0 exec0 -- -- 2>&1` && test x"$s" = x--"$no_arguments" &&
s=`! exec0 -- exec0 -- 2>&1` && test x"$s" = x--"$no_arguments" &&
s=`! exec0 -- exec0 -- -- 2>&1` && test x"$s" = x--"$no_arguments" &&
s=`! exec0 e 2>&1` && test x"$s" = x'exec0: e'"$no_such" &&
s=`! exec0 -- e 2>&1` && test x"$s" = x'exec0: e'"$no_such" &&
s=`! exec0 -- -- 2>&1` && test x"$s" = x'exec0: --'"$no_such" &&
echo 'All passed'
