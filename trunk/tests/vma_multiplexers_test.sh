#!/bin/bash

# Copyright (c) 2011 Mellanox Technologies Ltd.
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

#configurable parameters
#---------------------------------------------------

MC_GROUP=224.38.37.83
PORT=5001
DURATION=10	#in seconds
BW=10G
OUTPUT_FILES_PATH="./"
INTERFACE="ib0"
OVER_VMA="yes" #[yes | not]
SOCKPERF_MSG_SIZE="2"    #Bytes
MC_GROUP_SIZES=(2 20)
VMA_IGMP_ENABLE=0
VMA_SELECT_POLL=0
VMA_RX_BUFS=200000
#####################################################

#path
#----------------------------------------------------
SOCKPERF_APP=${SOCKPERF_PATH:-sockperf}
VMA_LIB=${VMA_PATH:-libvma.so}

#other
#----------------------------------------------------
VMA_SELECT_POLL_MAX_VAL=1000000
VMA_RX_BUFS_MAX_VAL=200000
RX_FRAMES_4_SOCKPERF=1
RX_USEC=10
UMCAST_VAL=1
DST_NET="224.0.0.0"
DST_MASK="240.0.0.0"
VMA="vma"
TMP_DIR=/tmp
TMP_FILE="$TMP_DIR/multiplexer_tmp"
ERROR_MESSAGE="!!! Test Failed !!!"
ERORR_PROMT="vma_perf_envelope:"
ERROR_RESULT="null"
PREFIX=""
REM_HOST_IP=$1
COMMAND_REDIRECT="1>$TMP_FILE 2>$TMP_FILE.err"
TRUE=1
FALSE=0
SUCCSESS=1
BLOCK_FILE="$TMP_DIR/vma_tests_block_file"
script_name=$(basename $0)
user_name=`whoami`
user_id=`who -am | tr " " "\n" | tail -1 | tr -d "(|)"`
SUPER_USR=root
#####################################################

function run_sockperf_using_select_epoll_poll_with_zero_polling
{	
	local vma_select_poll_old=$VMA_SELECT_POLL
	vma_select_poll_info=""
       	save_coalesce_params
	update_coalesce_4_sockperf
	append_tmp_file_and_delete "$TMP_DIR/$log_file.prep" "$log_file"
        	print_message "===============>SOCKPERF Using Select/Poll/Epoll<==============" "$log_file"
	if [[ "$OVER_VMA" = yes ]]; then
		vma_select_poll_info="With VMA_SELECT_POLL=0"
		print_message "|----------------------------------|" "$log_file"
		print_message "|VMA_SELECT_POLL=0" "$log_file"
		print_message "|----------------------------------|" "$log_file"
	fi
	run_sockperf_using_select_helper "$vma_select_poll_info"
	run_sockperf_using_poll_helper "$vma_select_poll_info"
	run_sockperf_using_epoll_helper "$vma_select_poll_info"
	recreate_coalesce_params 
	tests_finish	
}

function run_sockperf_using_select_epoll_poll_with_full_polling_vma_only
{	
	if [[ "$OVER_VMA" = yes ]]; then
		local vma_select_poll_old=$VMA_SELECT_POLL
		vma_select_poll_info=""
		save_coalesce_params
		update_coalesce_4_sockperf
		append_tmp_file_and_delete "$TMP_DIR/$log_file.prep" "$log_file"
		change_command_prefix VMA_SELECT_POLL "$VMA_SELECT_POLL_MAX_VAL"
		vma_select_poll_info="With VMA_SELECT_POLL=$VMA_SELECT_POLL_MAX_VAL"
		print_message "===============>SOCKPERF Using Select/Poll/Epoll<==============" "$log_file"
		print_message "|----------------------------------|" "$log_file"
		print_message "|VMA_SELECT_POLL=$VMA_SELECT_POLL_MAX_VAL" "$log_file"
		print_message "|----------------------------------|" "$log_file"
		run_sockperf_using_select_helper "$vma_select_poll_info"
		run_sockperf_using_poll_helper "$vma_select_poll_info"
		run_sockperf_using_epoll_helper "$vma_select_poll_info"
		change_command_prefix VMA_SELECT_POLL vma_select_poll_old
		recreate_coalesce_params 
		tests_finish	
	fi
}

function run_sockperf_using_select_helper
{
	command_str="s"
        log_str="Select (default timeout 1msec)"
	prepare_sockperf_using_feed_file_headlines "$log_str" "$1" 
	run_sockperf_with_diff_mc_feed_files "$command_str" "$log_str"

	command_str="s --timeout 0"
        log_str="Select (timeout zero)"
	prepare_sockperf_using_feed_file_headlines "$log_str" "$1" 
	run_sockperf_with_diff_mc_feed_files "$command_str" "$log_str"

	command_str="s --timeout 1000"
        log_str="Select (timeout 1 sec)"
	prepare_sockperf_using_feed_file_headlines "$log_str" "$1" 
	run_sockperf_with_diff_mc_feed_files "$command_str" "$log_str"

	command_str="s --timeout -1"
        log_str="Select (timeout infinite)"
	prepare_sockperf_using_feed_file_headlines "$log_str" "$1" 
	run_sockperf_with_diff_mc_feed_files "$command_str" "$log_str"
}

function run_sockperf_using_poll_helper
{
	command_str="p"
        log_str="Poll (default timeout 1msec)"
	prepare_sockperf_using_feed_file_headlines "$log_str" "$1" 
	run_sockperf_with_diff_mc_feed_files "$command_str" "$log_str"

	command_str="p --timeout 0"
        log_str="Poll (timeout zero)"
	prepare_sockperf_using_feed_file_headlines "$log_str" "$1" 
	run_sockperf_with_diff_mc_feed_files "$command_str" "$log_str"

	command_str="p --timeout 1000"
        log_str="Poll (timeout 1 sec)"
	prepare_sockperf_using_feed_file_headlines "$log_str" "$1" 
	run_sockperf_with_diff_mc_feed_files "$command_str" "$log_str"

	command_str="p --timeout -1"
        log_str="Poll (timeout infinite)"
	prepare_sockperf_using_feed_file_headlines "$log_str" "$1" 
	run_sockperf_with_diff_mc_feed_files "$command_str" "$log_str"
}

function run_sockperf_using_epoll_helper
{
	command_str="e"
        log_str="Epoll (default timeout 1msec)"
	prepare_sockperf_using_feed_file_headlines "$log_str" "$1" 
	run_sockperf_with_diff_mc_feed_files "$command_str" "$log_str"

	command_str="e --timeout 0"
        log_str="Epoll (timeout zero)"
	prepare_sockperf_using_feed_file_headlines "$log_str" "$1" 
	run_sockperf_with_diff_mc_feed_files "$command_str" "$log_str"

	command_str="e --timeout 1000"
        log_str="Epoll (timeout 1 sec)"
	prepare_sockperf_using_feed_file_headlines "$log_str" "$1" 
	run_sockperf_with_diff_mc_feed_files "$command_str" "$log_str"

	command_str="e --timeout -1"
        log_str="Epoll (timeout infinite)"
	prepare_sockperf_using_feed_file_headlines "$log_str" "$1" 
	run_sockperf_with_diff_mc_feed_files "$command_str" "$log_str"
}

function run_sockperf_with_diff_mc_feed_files
{
	local size_arr_len=${#MC_GROUP_SIZES[*]}
	print_sockperf_with_feed_files_header "$2"
	for((i=0; $i < $size_arr_len; i=$((i=$i+1))))
	do
        	curr_feed_file_size=${MC_GROUP_SIZES[$i]}
		feed_file_name="$TMP_DIR/feed_file_$curr_feed_file_size"
		print_cycle_info $curr_feed_file_size "mc group"
		create_mc_feed_files "$curr_feed_file_size" "$feed_file_name"
		run_sockperf_with_feed_file "$feed_file_name" "$1"
		parse_sockperf_test_results "${MC_GROUP_SIZES[$i]}"
		remove_mc_feed_files "$feed_file_name"
	done
	clean_after_sockperf
}


function run_sockperf_with_feed_file
{
	sockperf_command_line_srv=${PREFIX}"${SOCKPERF_APP} server -m $SOCKPERF_MSG_SIZE -f $1 -F $2"
	sockperf_command_line_clt=${PREFIX}"${SOCKPERF_APP} ping-pong -m $SOCKPERF_MSG_SIZE -f $1 -F $2 -t $DURATION"
	
        (echo ${SRV_CMMND_LINE_PREF}$sockperf_command_line_srv | tee -a $log_file) >& /dev/null
	(eval "$sockperf_command_line_srv 2>&1 | tee >> $log_file &")
	sleep 15
	(ssh $REM_HOST_IP "killall sockperf") >& /dev/null
	(echo "${CLT_CMMND_LINE_PREF} $sockperf_command_line_clt" | tee -a "$TMP_DIR/$log_file.tmp") >& /dev/null
	(ssh $REM_HOST_IP "sleep 10;$sockperf_command_line_clt 2>&1 | tee >> $TMP_FILE") 
	pkill -2 -f sockperf >& /dev/null
	sleep 5
}

function print_sockperf_with_feed_files_header
{
	print_message "=====================>SOCKPERF With $1<====================" "$log_file" 
}

function tests_finish
{
	eval "cat $TMP_DIR/$log_file.post" | tee -a $log_file >& /dev/null
	echo "---------------------------------------------------------------" |tee -a $log_file
	clean
}

function print_cycle_info
{
	let "cycle_num=$i+1"
 	echo "##################### cycle $cycle_num of $size_arr_len #####################"
	echo "#$2 size is $1"
}

function parse_sockperf_test_results 
{ 
	check_sockperf_succss
	if [[ $success -eq $TRUE ]]; then
  
	        latency=`ssh $REM_HOST_IP cat $TMP_FILE |tr A-Z a-z|grep latency|tr [="="=] " " |tr -s " "|tr " " "\n"|tail -2|head -1`
		echo "#average latency is $latency usec"
        	echo $1,$latency >> $res_file           		
	else
		echo "#$ERROR_MESSAGE"
		echo "$1,${ERROR_RESULT}" >> $res_file    	
        		
	fi

	ssh $REM_HOST_IP "cat $TMP_FILE" | tee -a "$TMP_DIR/$log_file.tmp" >& /dev/null 		
       	ssh $REM_HOST_IP "rm -rf $TMP_FILE" >& /dev/null	
}

function check_sockperf_succss
{
	local res=0
	
	res=`ssh $REM_HOST_IP "cat $TMP_FILE |tr A-Z a-z |grep latency | wc -l"`

	if [[ $res -gt 0 ]]; then
		success=$TRUE
	else
		success=$FALSE		
	fi 
	 
}

function prepare_output_files
{
        date=`date +%Y_%m_%d_%H_%M_%S`        
        log_file="${OUTPUT_FILES_PATH}vma_perf_${date}_logs.txt"
        res_file="${OUTPUT_FILES_PATH}vma_perf_${date}_results.csv"
     
        touch  $log_file
        touch  $res_file       
}

function prepare_sockperf_using_feed_file_headlines
{
	echo "" >> $res_file
	echo sockperf Using $1 $2 Test Results >> $res_file 
        echo MC Group Num,Latency >> $res_file
}

function update_command_line_pref_in_log_file
{
	SRV_CMMND_LINE_PREF="[$srv_hostname "`pwd`"]"
        CLT_CMMND_LINE_PREF="[$clt_hostname "`ssh $REM_HOST_IP pwd`"]"
}

function get_hostnames
{
	clt_hostname=`ssh $REM_HOST_IP hostname`
	srv_hostname=`hostname`

	update_command_line_pref_in_log_file

}

function update_command_prefix
{
	PREFIX=""
	
	if [[ "$OVER_VMA" = yes ]] ; then
	
		if [[ $VMA_IGMP_ENABLE -eq 0 ]] ; then
			PREFIX="$PREFIX VMA_IGMP=0 "	
		fi

		if [[ $VMA_SELECT_POLL -ne 0 ]] ; then
			PREFIX="$PREFIX VMA_SELECT_POLL=$VMA_SELECT_POLL "	
		fi

		if [[ $VMA_RX_BUFS -ne 0 ]] ; then
			PREFIX="$PREFIX VMA_RX_BUFS=$VMA_RX_BUFS "	
		fi			
	
		PREFIX=${PREFIX}"LD_PRELOAD=$VMA_LIB "	
	fi	
}

function change_command_prefix
{
	eval "$1=$2"
	update_command_prefix	
}

function remove_ifaces
{
	add_curr_route_table_2_log
	iface_arr_local=`route | grep 2[24][40].0.0.0 | tr -s ' ' | cut -d ' ' -f 8`
	iface_arr_remote=`ssh $REM_HOST_IP "route | grep 2[24][40].0.0.0 | tr -s ' ' | cut -d ' ' -f 8"`	
 	
	echo "" >> "$TMP_DIR/$log_file.prep"
	echo "============>Remove interfaces from route table <=============" >> "$TMP_DIR/$log_file.prep"	

	for iface in $iface_arr_local 
	do
		command="route del -net $DST_NET netmask $DST_MASK dev $iface"
		(echo "${SRV_CMMND_LINE_PREF} $command" | tee -a $TMP_DIR/$log_file.prep) >& /dev/null
		eval "$command 2>&1 | tee >> $TMP_DIR/$log_file.prep"	
	done	

	for iface in $iface_arr_remote 
	do
		command="route del -net $DST_NET netmask $DST_MASK dev $iface" 
		(echo "${CLT_CMMND_LINE_PREF} $command" | tee -a "$TMP_DIR/$log_file.prep") >& /dev/null
		eval "ssh  $REM_HOST_IP "$command" 2>&1 | tee >>  $TMP_DIR/$log_file.prep" 
	done	
}

function add_curr_route_table_2_log
{
	(echo "${SRV_CMMND_LINE_PREF} route" | tee -a "$TMP_DIR/$log_file.prep") >& /dev/null
	eval "route 2>&1 | tee >>  $TMP_DIR/$log_file.prep"
	
	(echo "${CLT_CMMND_LINE_PREF} route" | tee -a "$TMP_DIR/$log_file.prep") >& /dev/null
	eval "ssh $REM_HOST_IP "route" 2>&1 | tee >>  $TMP_DIR/$log_file.prep"		
}

function recreate_route_table
{
	echo "" >> "$TMP_DIR/$log_file.post"
	echo "===================>Recreate route table <====================" >> "$TMP_DIR/$log_file.post"
	command="route del -net $DST_NET netmask $DST_MASK dev $INTERFACE"
	(echo "${SRV_CMMND_LINE_PREF} $command" | tee -a "$TMP_DIR/$log_file.post") >& /dev/null
	eval "$command 2>&1 | tee >> $TMP_DIR/$log_file.post"
	(echo "${CLT_CMMND_LINE_PREF} $command" | tee -a "$TMP_DIR/$log_file.post") >& /dev/null
 	ssh  $REM_HOST_IP "$command" 2>&1 | tee >> "$TMP_DIR/$log_file.post"
		
	for iface in $iface_arr_local 
	do
		command="route add -net $DST_NET netmask $DST_MASK dev $iface"
		(echo "${SRV_CMMND_LINE_PREF} $command" | tee -a "$TMP_DIR/$log_file.post") >& /dev/null
		eval "$command 2>&1 | tee >> $TMP_DIR/$log_file.post"	
	done

	for iface in $iface_arr_remote 
	do
		command="route add -net $DST_NET netmask $DST_MASK dev $iface"
 		(echo "${CLT_CMMND_LINE_PREF} $command" | tee -a "$TMP_DIR/$log_file.post") >& /dev/null
		(eval "ssh  $REM_HOST_IP "$command" 2>&1 | tee >>  $TMP_DIR/$log_file.post") >& /dev/null 
	done
	eval "cat $TMP_DIR/$log_file.post" | tee -a $log_file >& /dev/null
	clean
}

function prepare_route_table
{	
	echo "" >> "$TMP_DIR/$log_file.prep"
	echo "=====================>Route table info <======================" >> "$TMP_DIR/$log_file.prep"
	remove_ifaces
	echo "" >> "$TMP_DIR/$log_file.prep"
	echo "============>Add work interface to route table <==============" >> "$TMP_DIR/$log_file.prep"
	command="route add -net $DST_NET netmask $DST_MASK dev $INTERFACE"
	(echo "${SRV_CMMND_LINE_PREF} $command" | tee -a "$TMP_DIR/$log_file.prep") >& /dev/null
	eval "$command 2>&1 | tee >> $TMP_DIR/$log_file.prep"
	(echo "${CLT_CMMND_LINE_PREF} $command" | tee -a "$TMP_DIR/$log_file.prep") >& /dev/null
 	eval "ssh  $REM_HOST_IP "$command" 2>&1 | tee >>  $TMP_DIR/$log_file.prep"  
	eval "cat $TMP_DIR/$log_file.prep" | tee -a $log_file >& /dev/null
	clean
}

function save_coalesce_params
{
	local_coalesce_params_saved=$TRUE
	remote_coalesce_params_saved=$TRUE
	command1="ethtool -c $INTERFACE"
	
	echo "" >> "$TMP_DIR/$log_file.prep"
	echo "===================>Coalesce params info<=====================" >> "$TMP_DIR/$log_file.prep"

	save_local_coalesce_params "$command1" rx-frames: initial_rx_frames_local
	save_remote_coalesce_params "$command1" rx-frames: initial_rx_frames_remote
	
	rm -f $TMP_FILE >& /dev/null
	rm -f $TMP_FILE.err >& /dev/null

}

function save_local_coalesce_params
{
	(echo "${SRV_CMMND_LINE_PREF} $1" | tee -a "$TMP_DIR/$log_file.prep") >& /dev/null
	eval "$1 $COMMAND_REDIRECT"
	check_succsess_and_save_param $2 get_coalesce_param
	eval "$3=$?"
	if [[ $SUCCSESS -eq $FALSE ]] ;then
		local_coalesce_params_saved=$FALSE		
	fi
	
}

function save_remote_coalesce_params
{
	(echo "${CLT_CMMND_LINE_PREF} $1" | tee -a "$TMP_DIR/$log_file.prep") >& /dev/null
	eval "(ssh $REM_HOST_IP "$1") $COMMAND_REDIRECT"
	check_succsess_and_save_param $2 get_coalesce_param
	eval "$3=$?" 
	if [[ $SUCCSESS -eq $FALSE ]]; then
		remote_coalesce_params_saved=$FALSE
	fi
}

function get_coalesce_param
{
	ret_val=`cat $TMP_FILE | grep $1 | cut -d " " -f 2 2>/dev/null` 
}

function get_umcast_val
{
	umcast_val=`cat $TMP_FILE | tr -d "\n" 2>/dev/null` 
}

function update_coalesce_4_sockperf
{
	local_coalesce_params_changed=$FALSE
	remote_coalesce_params_changed=$FALSE
	echo "" >> "$TMP_DIR/$log_file.prep"
	echo "============>Prepare coalesce params for sockperf<=============" >> "$TMP_DIR/$log_file.prep"
	update_coalesce_params $RX_FRAMES_4_SOCKPERF	
}

function update_coalesce_params
{
	command="ethtool -C $INTERFACE rx-frames $1"

	if [[ $local_coalesce_params_saved -eq $TRUE ]]; then
		if [[ $initial_rx_frames_local -ne $1 ]]; then
			update_local_coalesce_params
		fi
	fi

	if [[ $remote_coalesce_params_saved -eq $TRUE ]]; then
		if [[ $initial_rx_frames_remote -ne $1 ]]; then
			update_remote_coalesce_params
		fi
	fi
}

function update_local_coalesce_params
{
	(echo "${SRV_CMMND_LINE_PREF} $command" | tee -a "$TMP_DIR/$log_file.prep") >& /dev/null
 	eval "$command $COMMAND_REDIRECT"
	check_command_succss
	
	if [[ $SUCCSESS -eq $TRUE ]]; then
		local_coalesce_params_changed=$TRUE
	else
		local_coalesce_params_changed=$FALSE
	fi
	
}

function update_remote_coalesce_params
{
	(echo "${CLT_CMMND_LINE_PREF} $command" | tee -a "$TMP_DIR/$log_file.prep") >& /dev/null
	eval "(ssh  $REM_HOST_IP "$command") $COMMAND_REDIRECT"
	check_command_succss
	if [[ $SUCCSESS -eq $TRUE ]]; then
		remote_coalesce_params_changed=$TRUE
	else
		remote_coalesce_params_changed=$FALSE
	fi
}

function recreate_coalesce_params
{
	echo "" >> "$TMP_DIR/$log_file.post"
	echo "==================>Recreate coalesce params<==================" >> "$TMP_DIR/$log_file.post"

	if [[ $local_coalesce_params_changed -eq $TRUE ]]; then
		recreate_local_coalesce_params
	fi

	if [[ $remote_coalesce_params_changed -eq $TRUE ]]; then
		recreate_remote_coalesce_params	
	fi	
}

function recreate_local_coalesce_params
{
	local command="ethtool -C $INTERFACE rx-frames $initial_rx_frames_local"

	(echo "${SRV_CMMND_LINE_PREF} $command" | tee -a "$TMP_DIR/$log_file.post") >& /dev/null
	eval $command >& /dev/null	
}

function recreate_remote_coalesce_params
{
	local command="ethtool -C $INTERFACE rx-frames $initial_rx_frames_remote"

	(echo "${CLT_CMMND_LINE_PREF} $command" | tee -a "$TMP_DIR/$log_file.post") >& /dev/null
	ssh  $REM_HOST_IP "$command" >& /dev/null
}

function save_umcast
{
	local_umcast_saved=$FALSE
	remote_umcast_saved=$FALSE
	echo "" >> "$TMP_DIR/$log_file.prep"
	echo "========================>Umcast info<=========================" >> "$TMP_DIR/$log_file.prep"
	check_if_infbnd_iface
	if [[ $is_infiniband -eq $TRUE ]]; then
		save_local_umcast_val
		save_remote_umcast_val				
	fi
	eval "cat $TMP_DIR/$log_file.prep" | tee -a $log_file >& /dev/null
	clean
}

function save_local_umcast_val
{
	local command="cat /sys/class/net/$INTERFACE/umcast"
	(echo "${SRV_CMMND_LINE_PREF} $command" | tee -a "$TMP_DIR/$log_file.prep") >& /dev/null
	eval "cat /sys/class/net/$INTERFACE/umcast 1>$TMP_FILE 2>$TMP_FILE.err "
	check_succsess_and_save_param initial_local_umcast_val get_umcast_val
	if [[ $SUCCSESS -eq $FALSE ]]; then
		local_umcast_saved=$FALSE
	else
		initial_local_umcast_val=$umcast_val
		local_umcast_saved=$TRUE
	fi 
}

function save_remote_umcast_val
{
	local command="cat /sys/class/net/$INTERFACE/umcast"
	(echo "${CLT_CMMND_LINE_PREF} $command" | tee -a "$TMP_DIR/$log_file.prep") >& /dev/null
	(eval "ssh $REM_HOST_IP cat /sys/class/net/$INTERFACE/umcast") 1>$TMP_FILE 2>$TMP_FILE.err  	
	check_succsess_and_save_param initial_remote_umcast_val get_umcast_val
	if [[ $SUCCSESS -eq $FALSE ]]; then
		remote_umcast_saved=$FALSE
	else
		initial_remote_umcast_val=$umcast_val
		remote_umcast_saved=$TRUE
	fi	
}


function update_umcast
{
	echo "" >> "$TMP_DIR/$log_file.prep"
	echo "===================>Prepare umcast param<====================" >> "$TMP_DIR/$log_file.prep"
	local_umcast_changed=$FALSE
	remote_umcast_changed=$FALSE
	
	if [[ $initial_local_umcast_val -ne $UMCAST_VAL ]]; then
		update_local_umcast
	fi
	
	if [[ $initial_remote_umcast_val -ne $UMCAST_VAL ]]; then
		update_remote_umcast
	fi

	eval "cat $TMP_DIR/$log_file.prep" | tee -a $log_file >& /dev/null
	clean
}

function update_local_umcast
{
	local command="echo $UMCAST_VAL 1>&/sys/class/net/$INTERFACE/umcast"
	if [[ $local_umcast_saved -eq $TRUE ]]; then
		(echo "${SRV_CMMND_LINE_PREF} $command" | tee -a "$TMP_DIR/$log_file.prep") >& /dev/null
		eval "$command 1>$TMP_FILE 2>$TMP_FILE.err" 
		check_command_succss
		if [[ $SUCCSESS -eq $TRUE ]]; then
			local_umcast_changed=$TRUE
		else
			local_umcast_changed=$FALSE
		fi
	fi
}

function update_remote_umcast
{
	local command="echo $UMCAST_VAL 1>&/sys/class/net/$INTERFACE/umcast"
	if [[ $local_umcast_saved -eq $TRUE ]]; then
		(echo "${SRV_CMMND_LINE_PREF} $command" | tee -a "$TMP_DIR/$log_file.prep") >& /dev/null
		ssh $REM_HOST_IP "$command" | eval "1>$TMP_FILE 2>$TMP_FILE.err "	
		check_command_succss
		if [[ $SUCCSESS -eq $TRUE ]]; then
			remote_umcast_changed=$TRUE
		else
			remote_umcast_changed=$FALSE
		fi
	fi
}

function recreate_umcast
{
	echo "" >> "$TMP_DIR/$log_file.post"
	echo "====================>Recreate umcast value<===================" >> "$TMP_DIR/$log_file.post"
		
	if [[ $local_umcast_changed -eq $TRUE ]]; then
		recreate_local_umcast_val
	fi

	if [[ $remote_umcast_changed -eq $TRUE ]]; then
		recreate_remote_umcast_val	
	fi

	eval "cat $TMP_DIR/$log_file.post" | tee -a $log_file >& /dev/null	
}

function recreate_local_umcast_val
{
	local command="echo $initial_local_umcast_val 1>&/sys/class/net/$INTERFACE/umcast"
	(echo "${SRV_CMMND_LINE_PREF} $command" | tee -a "$TMP_DIR/$log_file.post") >& /dev/null
	eval "$command" >& /dev/null	
}

function recreate_remote_umcast_val
{
	local command="echo $initial_remote_umcast_val 1>&/sys/class/net/$INTERFACE/umcast"
	(echo "${CLT_CMMND_LINE_PREF} $command" | tee -a "$TMP_DIR/$log_file.post") >& /dev/null
	ssh  $REM_HOST_IP $command >& /dev/null
}

function clean_after_sockperf
{	
	ssh $REM_HOST_IP killall sockperf >& /dev/null
       	pkill -2 -f sockperf >& /dev/null 
        sleep 10 
        cat "$TMP_DIR/$log_file.tmp" | tee -a $log_file >& /dev/null 
        rm -f "$TMP_DIR/$log_file.tmp" >& /dev/null

}

function collect_nodes_info_to_file
{
	collect_local_node_info_to_file "$1"
	collect_remote_node_info_to_file "$1"
}

function collect_local_node_info_to_file
{
	(echo "======================>Local node info<======================" | tee -a $1) >& /dev/null
	(echo "--------------------" | tee -a $1) >& /dev/null
	(hostname | tee -a $1) >& /dev/null
	(echo -n "OS: " >> $1;cat /etc/issue | grep We | tee -a $1) >& /dev/null
	(echo -n "CPU: " >> $1;cat /proc/cpuinfo | grep 'model name' | sort -u | awk '{print $4, $5, $6, $7, $9}' | tee -a $1) >& /dev/null
	(echo -n "Number of CPUs: " >> $1;cat /proc/cpuinfo |grep proce |wc |awk '{print $1}' | tee -a $1) >& /dev/null
	(echo -n "CPU Type: " >> $1;uname -a | awk '{print $12}' | tee -a $1) >& /dev/null
	(cat /proc/meminfo |grep [M,m]em | tee -a $1) >& /dev/null
	(echo -n "Kernel: " >> $1;uname -a | awk '{print $3}' | tee -a $1) >& /dev/null
	(cat /usr/voltaire/version | tee -a $1) >& /dev/null
	(ibstat | grep -e "CA type" -e "Firmware version" | tee -a $1) >& /dev/null
	(ibstatus | grep -e rate -e state | grep -v 'phys state' | tee -a $1) >& /dev/null
	check_if_infbnd_iface
	if [[ $is_infiniband -eq $TRUE ]]; then
		(echo -n "IPoIB mode: " >> $1 ; cat "/sys/class/net/$INTERFACE/mode" | tee -a $1) >& /dev/null
	fi
	(ifconfig $INTERFACE | grep MTU | awk '{print $5}' | tee -a $1) >& /dev/null
	(echo -n "OFED:" >> $1;ofed_info | head -6 | grep OFED | tee -a $1) >& /dev/null
	(echo -n "VMA:" >> $1;rpm -qa | grep $VMA | tee -a $1) >& /dev/null	

}

function collect_remote_node_info_to_file
{
	(echo "=====================>Remote node info<======================" | tee -a $1) >& /dev/null
	(echo "--------------------" | tee -a $1) >& /dev/null
	(ssh $REM_HOST_IP "hostname" | tee -a $1) >& /dev/null
	(echo -n "OS: " >> $1;ssh $REM_HOST_IP cat /etc/issue | grep We | tee -a $1) >& /dev/null
	(echo -n "CPU: " >> $1;ssh $REM_HOST_IP cat /proc/cpuinfo | grep 'model name' | sort -u | awk '{print $4, $5, $6, $7, $9}' | tee -a $1) >& /dev/null
	(echo -n "Number of CPUs: " >> $1;ssh $REM_HOST_IP cat /proc/cpuinfo |grep proce |wc |awk '{print $1}' | tee -a $1) >& /dev/null
	(echo -n "CPU Type: " >> $1;ssh $REM_HOST_IP uname -a | awk '{print $12}' | tee -a $1) >& /dev/null
	(ssh $REM_HOST_IP cat /proc/meminfo |grep [M,m]em | tee -a $1) >& /dev/null
	(echo -n "Kernel: " >> $1;ssh $REM_HOST_IP uname -a | awk '{print $3}' | tee -a $1) >& /dev/null
	(ssh $REM_HOST_IP "cat /usr/voltaire/version" | tee -a $1) >& /dev/null
	(ssh $REM_HOST_IP "ibstat | grep -e "CA type" -e 'Firmware version'" | tee -a $1) >& /dev/null
	(ssh $REM_HOST_IP "ibstatus | grep -e rate -e state | grep -v 'phys state'" | tee -a $1) >& /dev/null
	to_print="/sys/class/net/$INTERFACE/mode"
	check_if_infbnd_iface
	if [[ $is_infiniband -eq $TRUE ]]; then
		(echo -n "IPoIB mode: " >> $1 ;ssh $REM_HOST_IP "cat $to_print" | tee -a $1) >& /dev/null
	fi
	(ssh $REM_HOST_IP ifconfig $INTERFACE | grep MTU | awk '{print $5}' | tee -a $1) >& /dev/null
	(echo -n "OFED:" >> $1;ssh $REM_HOST_IP ofed_info | head -6 | grep OFED | tee -a $1) >& /dev/null
	(echo -n "VMA:" >> $1;ssh $REM_HOST_IP rpm -qa | grep $VMA | tee -a $1) >& /dev/null
}

function print_message
{
	echo $1 | tee -a $2
        echo ""| tee -a $2 
}

function append_tmp_file_and_delete
{
	cat $1 | tee -a $2 >& /dev/null
	rm -f  $1	
}

function check_command_succss
{	
	if [ -s  $TMP_FILE.err ]; then
		eval "cat $TMP_FILE.err 2>&1 | tee >> $TMP_DIR/$log_file.prep"
		LAST_ERROR=`cat $TMP_FILE.err`
		SUCCSESS=$FALSE		
	else
		if [ -s $TMP_FILE ]; then
			eval "cat $TMP_FILE 2>&1 | tee >> $TMP_DIR/$log_file.prep"
		fi
		SUCCSESS=$TRUE	
	fi

	rm -f $TMP_FILE.err >& /dev/null	
}

function check_succsess_and_save_param
{
	local ret_val=0

	check_command_succss

	if [[ $SUCCSESS -eq $TRUE ]]; then
		$2 $1			
	fi

	return $ret_val 
} 

function check_if_infbnd_iface
{
	is_infiniband=$FALSE
	
	if [[ $INTERFACE  =~ "ib*" ]]; then
		is_infiniband=$TRUE		
	fi
}

function discover_local_work_if_ip
{
	LOCAL_IP=`ifconfig  $INTERFACE | grep inet |grep -v inet6 | cut -d ':' -f 2| cut -d " " -f 1`  >& /dev/null
}

function calc_file_age
{
	creation_time=`stat -c %Z /tmp/vma_utils_block_file` 
	now=`date +%s`
	block_file_age=$(($now-$creation_time)) 		
}

function get_operator_pid
{
	pid=$$
	#pid=`ps -eF |grep $script_name| tr -s ' ' | cut -d  ' ' -f 2|head -1`
}

function write_to_file_operator_details
{
	operating_machine_hostname=`hostname`
        get_operator_pid
	rm -f  "$2" >& /dev/null
	touch "$2" >& /dev/null
	echo -n "$1 " >> "$2" 2>/dev/null 
	echo -n "$pid " >> "$2" 2>/dev/null 
	echo -n "$script_name " >> "$2" 2>/dev/null
        echo -n "$user_name " >> "$2" 2>/dev/null
        echo -n "$user_id " >> "$2" 2>/dev/null
	echo -n "$operating_machine_hostname " >> "$2" 2>/dev/null

	if [[ $1 == "local" ]]; then
		st_timestamp=`date`
	else
		st_timestamp=`ssh $REM_HOST_IP "date" `
	fi
	
	echo "$st_timestamp" | tr " " "_" >> "$2" 2>/dev/null 	
}

function read_block_file
{
	blocking_pid=`awk -F " " '{print $2}' "$1"`
	blocking_app=`awk -F " " '{print $3}' "$1"`
        blocking_username=`awk -F " " '{print $4}' "$1"`
        blocking_id=`awk -F " " '{print $5}' "$1"`
	blocking_hostname=`awk -F " " '{print $6}' "$1"`
        blocking_st_time=`awk -F " " '{print $7}' "$1"`
}

function print_block_files_details
{
	echo "Blocked Host:$blocked_host"
	echo "You blocked by:"
	echo "-	application: ${blocking_app} "
        echo "-	user: ${blocking_username} "
	echo "-	users local host ip:${blocking_id} "
	echo "-	blocking proccess with pid: ${blocking_pid} running on host ${blocking_hostname} "
        echo "-	starting time:${blocking_st_time} "
	echo "-	blocking file:${BLOCK_FILE}"		
}

function update_remote_block_file
{
	write_to_file_operator_details "$LOCAL_IP" "${BLOCK_FILE}.rem"
	scp "${BLOCK_FILE}.rem" "${REM_HOST_IP}:${BLOCK_FILE}" >& /dev/null
	rm -f  "${BLOCK_FILE}.rem" >& /dev/null
}

function update_local_block_file
{
	write_to_file_operator_details "local" "$BLOCK_FILE"
}

function update_block_files
{
	discover_local_work_if_ip
	if [ "$LOCAL_IP" == "" ]; then
		echo "WARNING: Will be executed without blocking..."
	else
		update_local_block_file
		update_remote_block_file
	fi	
}

function unblock_local_host
{
	rm -f $BLOCK_FILE >& /dev/null
	rm -f "${BLOCK_FILE}.rem" >& /dev/null
}

function unblock_remote_host
{
	ssh $REM_HOST_IP "rm -f $BLOCK_FILE >& /dev/null" >& /dev/null
	rm -f "${BLOCK_FILE}.rem" >& /dev/null
}

function unblock
{
	unblock_local_host
	unblock_remote_host
}

function check_connection_to_remote_host
{
	ping -w 3 "$1" >& /dev/null
	test $? -eq 0 && RES=OK || RES=NOT 
}

function check_if_another_proccess_running_on_local_host
{
	eval "ps -eF| grep '$blocking_pid'|grep -v grep|wc -l" > $TMP_FILE
	RES=`cat $TMP_FILE`
}

function check_if_another_proccess_running_on_remote_host
{
	RES=0
	check_connection_to_remote_host "$1"
	if [ $RES == "OK" ]; then
		RES=`sudo ssh ${SUPER_USR}@${1} "ps -eF| grep ${blocking_pid}|grep -v grep|wc -l"`
	else
		RES=1
	fi
}

function get_operating_host_ip_or_hostname
{
	RES=0
	operating_host_ip=`awk -F " " '{print $1}' "$1"`
	if [[ $operating_host_ip != "local" ]]; then
		check_connection_to_remote_host "$operating_host_ip"
		if [ $RES != "OK" ]; then
			operating_host_ip=`awk -F " " '{print $6}' "$1"`
		fi	
	fi
}	

function check_if_operating_host_running_another_proccess 
{
	if [ $1 == "local" ]; then
		check_if_another_proccess_running_on_local_host	
	else
		check_if_another_proccess_running_on_remote_host "$1"
	fi
}

function adjust_operating_host_ip_of_remote_machine
{
	local tmp=""
	
	if [ "$1" == "local" ]; then
		operating_host_ip=$REM_HOST_IP
	else
		tmp=`ifconfig|grep $1`
		if [ "$tmp" != "" ]; then
			operating_host_ip="local"		
		fi	
	fi
}

function check_if_remote_host_is_blocked
{	
	RES=0
	ssh $REM_HOST_IP "cat  ${BLOCK_FILE} 2>/dev/null" > "${BLOCK_FILE}.rem"

	if [ -s "${BLOCK_FILE}.rem" ]; then
		read_block_file "${BLOCK_FILE}.rem"
		get_operating_host_ip_or_hostname "${BLOCK_FILE}.rem"
		adjust_operating_host_ip_of_remote_machine "$operating_host_ip"
		check_if_operating_host_running_another_proccess "$operating_host_ip"
		
		if [[ $RES -le 0 ]]; then
			unblock_remote_host 
		else
			read_block_file "$BLOCK_FILE.rem"
			blocked_host=$REM_HOST_IP
			print_block_files_details
			rm -f "${BLOCK_FILE}.rem" >& /dev/null
			clean
			exit 1
		fi	
	fi
}

function check_if_local_host_is_blocked
{	
	RES=0
	if [[ -e $BLOCK_FILE ]]; then
		read_block_file "$BLOCK_FILE"
		get_operating_host_ip_or_hostname "${BLOCK_FILE}"
		check_if_operating_host_running_another_proccess "$operating_host_ip"
		if [[ $RES -le 0 ]]; then
			unblock_local_host 
		else
			blocked_host=`hostname`
			print_block_files_details
			clean
			exit 1
		fi	
	fi	
}

function block 
{
	check_if_local_host_is_blocked	
	check_if_remote_host_is_blocked
	update_block_files
}


function pre_test_checks
{
	check_connection_2_remote_ip
	clean
}

function check_connection_2_remote_ip
{
	ssh -o "BatchMode yes" $REM_HOST_IP exit 2>$TMP_FILE.err
	check_command_succss	
	if [[ $SUCCSESS -ne $TRUE ]]; then
		echo "vma_perf_envelope error:$LAST_ERROR"
		clean
		unblock
		exit 1	
	fi		
}

function write_mc_feed_file
{
	x=0  # num of mc groups
	y=1  # the third number of the mc group
	port=10005
	num=3  # the last number of the mc group


	while [ $x -lt $1 ]
	do
		if [ $num -ge 254 ]; then
			y=$(($y+1)) 
			num=3
		fi
		
		echo 224.4.$y.$num:$port >> $2
		x=$(($x+1))
		port=$(($port+1))
		num=$(($num+1))
	done
}

function create_mc_feed_files
{
	write_mc_feed_file $1 $2
	copy_feed_file_2_remote_machine	$2 >& /dev/null	
}

function copy_feed_file_2_remote_machine
{
	scp $1 	"$REM_HOST_IP:/$TMP_DIR"
}

function remove_mc_feed_files
{
	rm -f $1 >& /dev/null 
	ssh $REM_HOST_IP "rm -f $1" >& /dev/null
}

function clean
{
	rm -f $TMP_FILE.err >& /dev/null 
	rm -f "$TMP_DIR/$log_file.prep" >& /dev/null 
	rm -f "$TMP_DIR/$log_file.post" >& /dev/null	
}

function write_date_2_log_file
{
	echo "=============================>Date<===========================" >> "$log_file"
	(echo "${SRV_CMMND_LINE_PREF} date" | tee -a "$log_file") >& /dev/null
	(date | tee -a $log_file) >& /dev/null
}

function pre_vma_perf
{	
	pre_test_checks
	get_hostnames
	prepare_output_files
	write_date_2_log_file
	collect_nodes_info_to_file "$log_file"	
	prepare_route_table
	save_umcast
	update_umcast
	clean			
	update_command_prefix		
}

function final_test_message
{
	echo "####################### Generated files #######################"
	echo "#Test results	:	$res_file"
	echo "#Test logs	:	$log_file"
	echo "---------------------------------------------------------------"
}

function post_vma_perf
{
	collect_nodes_info_to_file "$res_file"	
	recreate_route_table
	recreate_umcast
	final_test_message
	write_date_2_log_file
	clean	
}

function vma_perf
{
	run_sockperf_using_select_epoll_poll_with_zero_polling
        run_sockperf_using_select_epoll_poll_with_full_polling_vma_only
}	

#main

if [ $# = 1 ]; then
	block      
        pre_vma_perf
	vma_perf
	post_vma_perf
	unblock
else
      echo "Usage: perf <ip of remote host>"
      exit
fi
