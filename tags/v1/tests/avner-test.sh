#!/bin/bash


if [ $# != 1 ] 
then 
	echo "Usage: $0 <Server-IP>"
	exit 1
fi;
IP=$1

function clean_up {
	# Perform program exit housekeeping
	kill %1 %2 %3 %4 # kill any running child
	exit
}

trap clean_up SIGINT SIGTERM SIGHUP

echo \# FILTER = percentile  75
#echo \# FILTER = Message Rate

echo \# VMA_PATH = $VMA_PATH
echo \# BASE_ARG = $BASE_ARG
echo \# Server-IP = $IP

export | awk '/VMA/ {print "#", $0}'

while true
do 
	CMD="./sockperf "; ARG="${BASE_ARG} -i $IP"; 
	sleep 1
	echo -n "OS  : ";_D_PRELOAD=$VMA_PATH $CMD $ARG 2> /dev/null | awk '/dropped packets/ && $6+$11+$16 {printf("#%s\n#", $0) } /percentile  75/{print $6}'
	#echo -n "OS  : ";_D_PRELOAD=$VMA_PATH $CMD $ARG 2> /dev/null | awk '/Message Rate/{print $6}'
	sleep 1
	echo -n "VMA : ";LD_PRELOAD=$VMA_PATH $CMD $ARG 2> /dev/null | awk '/dropped packets/ && $6+$11+$16 {printf("#%s\n#", $0) } /percentile  75/{print $6}'
	#echo -n "VMA : ";LD_PRELOAD=$VMA_PATH $CMD $ARG 2> /dev/null | awk '/Message Rate/{print $6}'

	echo ---
done
