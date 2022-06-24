#!/bin/bash
#
# Copyright (c) 2011-2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification,
# are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
# 3. Neither the name of the Mellanox Technologies Ltd nor the names of its
#    contributors may be used to endorse or promote products derived from this
#    software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
# SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
# OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
# IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
# OF SUCH DAMAGE.
#


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
