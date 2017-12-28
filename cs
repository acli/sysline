#!/bin/sh

# cs top bottom
# set scroll-region to lines $1 - $2 (1-based)
# the resulting free area can be used for a status-line -- bjd

# comment out or change WHICH_TPUT to whatever is appropriate

# -- GNU termcap-based tput (which I call tcput):
#WHICH_TPUT=tcput
# -- Ncurses terminfo-based tput:
WHICH_TPUT=tput


if [ ! $# -eq 2 ]
then
	echo "usage: $0 top bottom (1-based)"
	echo "e.g. cs 2 28 for a scroll-region of lines 2-28 inclusive"
	exit
fi

stty rows $((${2}-${1}+1))		# let kernel know about it

if TPUT=`type -path $WHICH_TPUT`
then
	# we have a tput...
	$TPUT clear			# clear screen
	# csr expects 0-based parameters, so decrement ours
	$TPUT csr $(($1-1)) $(($2-1))	# set scroll-region to line $1 -- $2
	$TPUT home
else
	# try this then...
	#echo -en "\033c"		# overkill, is a reset
	echo -en "\033[H\033[J"		# clear screen
	echo -en "\033[${1};${2}r"	# set scroll-region to line $1 -- $2
	echo -en "\033[${1};1H"		# position cursor at top of scroll-region
fi
