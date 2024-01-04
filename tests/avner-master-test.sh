#!/bin/bash
#
# Copyright (c) 2011-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

LOG_DIR=./log/`hostname`
mkdir -p $LOG_DIR

function clean_up {
	# Perform program exit housekeeping
	kill %1 %2 %3 %4 # kill any running child
	exit
}

trap clean_up SIGINT SIGTERM SIGHUP

function run_test()
{
	echo -e "\n# >> running test for $RUNTIME; LOG_FILE is: $LOG_DIR/$LOG_FILE\n"
	echo "# $MSG" | tee -a $LOG_DIR/$LOG_FILE
	./avner-test.sh $IP | tee -a $LOG_DIR/$LOG_FILE &
	
	sleep $RUNTIME
	kill %1
	echo -e \\a #beep
}

#export VMA_PATH=/volt/avnerb/vma/libvma.so-4.5
#export VMA_PATH=/volt/avnerb/vma/libvma.so-nosend-norecv
#export VMA_PATH=/volt/avnerb/vma/libvma.so-nosocket
#export VMA_PATH=/volt/avnerb/vma/libvma.so-norecv
#export VMA_PATH=/volt/avnerb/vma/libvma.so-nosend
#export VMA_PATH=/volt/avnerb/vma/libvma.so-nosend-norecv-nobind

#export BASE_ARG="-c -t 4 -m 14" 


export VMA_RX_OFFLOAD=0 VMA_UC_OFFLOAD=1
export BASE_ARG="-c -t 4 -m 14 --ping-pong --pps=max" 
export VMA_PATH=/volt/avnerb/vma/libvma.so-4.5
RUNTIME=300s
LOG_FILE=0.eth_server_ucoff-0.pingmax.rx0ff-0.ucoff-1.server-vma-custom-mc-reply-uc-client-vma4.5
MSG="ETH Server-Command: VMA_IGMP=0 VMA_RX_OFFLOAD=1 VMA_UC_OFFLOAD=0 LD_PRELOAD=/volt/avnerb/vma/libvma.so-4.5  ./sockperf -s -i 226.8.8.8 --force_unicast_reply"
#run_test

export VMA_RX_OFFLOAD=1 VMA_UC_OFFLOAD=1
export BASE_ARG="-c -t 4 -m 14 --ping-pong --pps=max" 
export VMA_PATH=/volt/avnerb/vma/libvma.so-4.5
RUNTIME=300s
LOG_FILE=1.eth_server_ucoff-0.pingmax.rx0ff-1.ucoff-1.server-vma-custom-mc-reply-uc-client-vma4.5
MSG="ETH Server-Command: VMA_IGMP=0 VMA_RX_OFFLOAD=1 VMA_UC_OFFLOAD=0 LD_PRELOAD=/volt/avnerb/vma/libvma.so-4.5  ./sockperf -s -i 226.8.8.8 --force_unicast_reply"
#run_test

export VMA_RX_OFFLOAD=0 VMA_UC_OFFLOAD=1
export BASE_ARG="-c -t 4 -m 14 --ping-pong --pps=max" 
export VMA_PATH=/volt/avnerb/vma/libvma.so-norecv
RUNTIME=300s
LOG_FILE=2.eth_server_ucoff-0.pingmax.rx0ff-0.ucoff-1.server-vma-custom-mc-reply-uc-client-vma-norecv
MSG="ETH Server-Command: VMA_IGMP=0 VMA_RX_OFFLOAD=1 VMA_UC_OFFLOAD=0 LD_PRELOAD=/volt/avnerb/vma/libvma.so-4.5  ./sockperf -s -i 226.8.8.8 --force_unicast_reply"
#run_test


export VMA_RX_OFFLOAD=0 VMA_UC_OFFLOAD=1
export BASE_ARG="-c -t 4 -m 14 --ping-pong --pps=max" 
export VMA_PATH=/volt/avnerb/vma/libvma.so-4.5
RUNTIME=300s
LOG_FILE=3.eth_server_ucoff-1.pingmax.rx0ff-0.ucoff-1.server-vma-custom-mc-reply-uc-client-vma4.5
MSG="ETH Server-Command: VMA_IGMP=0 VMA_RX_OFFLOAD=1 VMA_UC_OFFLOAD=1 LD_PRELOAD=/volt/avnerb/vma/libvma.so-4.5  ./sockperf -s -i 226.8.8.8 --force_unicast_reply"
run_test

export VMA_RX_OFFLOAD=1 VMA_UC_OFFLOAD=1
export BASE_ARG="-c -t 4 -m 14 --ping-pong --pps=max" 
export VMA_PATH=/volt/avnerb/vma/libvma.so-4.5
RUNTIME=300s
LOG_FILE=4.eth_server_ucoff-1.pingmax.rx0ff-1.ucoff-1.server-vma-custom-mc-reply-uc-client-vma4.5
MSG="ETH Server-Command: VMA_IGMP=0 VMA_RX_OFFLOAD=1 VMA_UC_OFFLOAD=1 LD_PRELOAD=/volt/avnerb/vma/libvma.so-4.5  ./sockperf -s -i 226.8.8.8 --force_unicast_reply"
run_test

export VMA_RX_OFFLOAD=0 VMA_UC_OFFLOAD=1
export BASE_ARG="-c -t 4 -m 14 --ping-pong --pps=max" 
export VMA_PATH=/volt/avnerb/vma/libvma.so-norecv
RUNTIME=300s
LOG_FILE=5.eth_server_ucoff-1.pingmax.rx0ff-0.ucoff-1.server-vma-custom-mc-reply-uc-client-vma-norecv
MSG="ETH Server-Command: VMA_IGMP=0 VMA_RX_OFFLOAD=1 VMA_UC_OFFLOAD=1 LD_PRELOAD=/volt/avnerb/vma/libvma.so-4.5  ./sockperf -s -i 226.8.8.8 --force_unicast_reply"
run_test

