#!/bin/awk -f
BEGIN {
	# this syntax will allow --assign var=val in command line
	if (!RANGE_START_USEC) RANGE_START_USEC = 20
	if (!RANGE_START_END)  RANGE_END_USEC   = 1000*1000*1000
	if (!DECIMAL_DIGITS)   DECIMAL_DIGITS   = 9
	dataStarted = 0
}

$1 == "txTime," {dataStarted = 1; print "txTime, usecLat"}
{
	if (!dataStarted) print
	else {
        	usecLat = 1000*1000*($2 - $1)/2
	        if (RANGE_START_USEC <= usecLat && usecLat <= RANGE_END_USEC )
        	        printf "%.*f, %.3f\n", DECIMAL_DIGITS, $1, usecLat

	}
}
