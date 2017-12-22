#!/bin/bash -eExl

source $(dirname $0)/globals.sh

do_check_filter "Checking for test ..." "off"

if [ $(command -v ibdev2netdev >/dev/null 2>&1 || echo $?) ]; then
	echo "[SKIP] ibdev2netdev tool does not exist"
	exit 0
fi

cd $WORKSPACE

rm -rf $test_dir
mkdir -p $test_dir
cd $test_dir

test_app="$install_dir/bin/sockperf"

if [ $(command -v $test_app >/dev/null 2>&1 || echo $?) ]; then
    echo can not find $test_app
    exit 1
fi

if [ ! -z $(do_get_ip 'eth') ]; then
    test_ip_list="${test_ip_list} eth:$(do_get_ip 'eth')"
fi

test_list="tcp-pp tcp-tp tcp-ul udp-pp udp-tp udp-ul"

nerrors=0

for test_link in $test_ip_list; do
	for test in $test_list; do
		IFS=':' read test_in test_ip <<< "$test_link"
		test_name=${test_in}-${test}
		test_tap=${WORKSPACE}/${prefix}/test-${test_name}.tap

		$timeout_exe ${WORKSPACE}/tests/verifier/verifier.pl -a ${test_app} \
			-t ${test} -s ${test_ip} -l ${test_dir}/${test_name}.log \
			--progress=0

		# exclude multicast tests from final result
		# because they are not valid when server/client on the same node
		grep -e 'PASS' -e 'FAIL' ${test_dir}/${test_name}.dump | grep -v 'multicast' > ${test_dir}/${test_name}.tmp

		do_archive "${test_dir}/${test_name}.dump" "${test_dir}/${test_name}.log"

		echo "1..$(wc -l < ${test_dir}/${test_name}.tmp)" > $test_tap

		v1=1
		while read line; do
		    if [[ $(echo $line | cut -f1 -d' ') =~ 'PASS' ]]; then
		        v0='ok'
		        v2=$(echo $line | sed 's/PASS //')
		    else
		        v0='not ok'
		        v2=$(echo $line | sed 's/FAIL //')
	            nerrors=$((nerrors+1))
		    fi

		    echo -e "$v0 ${test_in}: $v2" >> $test_tap
		    v1=$(($v1+1))
		done < ${test_dir}/${test_name}.tmp
		rm -f ${test_dir}/${test_name}.tmp
	done
done

rc=$(($rc+$nerrors))

echo "[${0##*/}]..................exit code = $rc"
exit $rc
