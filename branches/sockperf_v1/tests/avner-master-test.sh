#!/bin/bash

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

#export BASE_ARG="-c -t 4 -m 12" 


export VMA_RX_OFFLOAD=0 VMA_UC_OFFLOAD=1
export BASE_ARG="-c -t 4 -m 12 --ping-pong --pps=max" 
export VMA_PATH=/volt/avnerb/vma/libvma.so-4.5
RUNTIME=300s
LOG_FILE=0.eth_server_ucoff-0.pingmax.rx0ff-0.ucoff-1.server-vma-custom-mc-reply-uc-client-vma4.5
MSG="ETH Server-Command: VMA_IGMP=0 VMA_RX_OFFLOAD=1 VMA_UC_OFFLOAD=0 LD_PRELOAD=/volt/avnerb/vma/libvma.so-4.5  ./sockperf -s -i 226.8.8.8 --force_unicast_reply"
#run_test

export VMA_RX_OFFLOAD=1 VMA_UC_OFFLOAD=1
export BASE_ARG="-c -t 4 -m 12 --ping-pong --pps=max" 
export VMA_PATH=/volt/avnerb/vma/libvma.so-4.5
RUNTIME=300s
LOG_FILE=1.eth_server_ucoff-0.pingmax.rx0ff-1.ucoff-1.server-vma-custom-mc-reply-uc-client-vma4.5
MSG="ETH Server-Command: VMA_IGMP=0 VMA_RX_OFFLOAD=1 VMA_UC_OFFLOAD=0 LD_PRELOAD=/volt/avnerb/vma/libvma.so-4.5  ./sockperf -s -i 226.8.8.8 --force_unicast_reply"
#run_test

export VMA_RX_OFFLOAD=0 VMA_UC_OFFLOAD=1
export BASE_ARG="-c -t 4 -m 12 --ping-pong --pps=max" 
export VMA_PATH=/volt/avnerb/vma/libvma.so-norecv
RUNTIME=300s
LOG_FILE=2.eth_server_ucoff-0.pingmax.rx0ff-0.ucoff-1.server-vma-custom-mc-reply-uc-client-vma-norecv
MSG="ETH Server-Command: VMA_IGMP=0 VMA_RX_OFFLOAD=1 VMA_UC_OFFLOAD=0 LD_PRELOAD=/volt/avnerb/vma/libvma.so-4.5  ./sockperf -s -i 226.8.8.8 --force_unicast_reply"
#run_test


export VMA_RX_OFFLOAD=0 VMA_UC_OFFLOAD=1
export BASE_ARG="-c -t 4 -m 12 --ping-pong --pps=max" 
export VMA_PATH=/volt/avnerb/vma/libvma.so-4.5
RUNTIME=300s
LOG_FILE=3.eth_server_ucoff-1.pingmax.rx0ff-0.ucoff-1.server-vma-custom-mc-reply-uc-client-vma4.5
MSG="ETH Server-Command: VMA_IGMP=0 VMA_RX_OFFLOAD=1 VMA_UC_OFFLOAD=1 LD_PRELOAD=/volt/avnerb/vma/libvma.so-4.5  ./sockperf -s -i 226.8.8.8 --force_unicast_reply"
run_test

export VMA_RX_OFFLOAD=1 VMA_UC_OFFLOAD=1
export BASE_ARG="-c -t 4 -m 12 --ping-pong --pps=max" 
export VMA_PATH=/volt/avnerb/vma/libvma.so-4.5
RUNTIME=300s
LOG_FILE=4.eth_server_ucoff-1.pingmax.rx0ff-1.ucoff-1.server-vma-custom-mc-reply-uc-client-vma4.5
MSG="ETH Server-Command: VMA_IGMP=0 VMA_RX_OFFLOAD=1 VMA_UC_OFFLOAD=1 LD_PRELOAD=/volt/avnerb/vma/libvma.so-4.5  ./sockperf -s -i 226.8.8.8 --force_unicast_reply"
run_test

export VMA_RX_OFFLOAD=0 VMA_UC_OFFLOAD=1
export BASE_ARG="-c -t 4 -m 12 --ping-pong --pps=max" 
export VMA_PATH=/volt/avnerb/vma/libvma.so-norecv
RUNTIME=300s
LOG_FILE=5.eth_server_ucoff-1.pingmax.rx0ff-0.ucoff-1.server-vma-custom-mc-reply-uc-client-vma-norecv
MSG="ETH Server-Command: VMA_IGMP=0 VMA_RX_OFFLOAD=1 VMA_UC_OFFLOAD=1 LD_PRELOAD=/volt/avnerb/vma/libvma.so-4.5  ./sockperf -s -i 226.8.8.8 --force_unicast_reply"
run_test

