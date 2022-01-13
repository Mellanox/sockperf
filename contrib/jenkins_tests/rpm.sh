#!/bin/bash -eExl

source $(dirname $0)/globals.sh

echo "Checking for rpm ..."

cd $WORKSPACE

rm -rf $rpm_dir
mkdir -p $rpm_dir
cd $rpm_dir

rpm_tap=${WORKSPACE}/${prefix}/rpm.tap

cd ${build_dir}/0

if [ -x /usr/bin/dpkg-buildpackage ]; then
    echo "Build on debian"
    set +e
    ${WORKSPACE}/build/build-rpm.sh "$rpm_dir" 2> "${rpm_dir}/rpm.err" 1> "${rpm_dir}/rpm.log"
    rc=$((rc + $?))
    ${WORKSPACE}/build/build-rpm-to-deb.sh $(find $rpm_dir/SRPMS -maxdepth 1 -type f -name "sockperf*.src.rpm") 2> "${rpm_dir}/rpm-deb.err" 1> "${rpm_dir}/rpm-deb.log"
    rc=$((rc + $?))
    do_archive "${rpm_dir}/*.err" "${rpm_dir}/*.log"
    set -e
	echo "1..1" > $rpm_tap
	if [ $rc -gt 0 ]; then
	    echo "not ok 1 Debian package" >> $rpm_tap
	else
	    echo ok 1 Debian package >> $rpm_tap
	fi
else
    echo "Build rpm"
    set +e
    ${WORKSPACE}/build/build-rpm.sh "$rpm_dir" 2> "${rpm_dir}/rpm.err" 1> "${rpm_dir}/rpm.log"
    rc=$((rc + $?))
    do_archive "${rpm_dir}/*.err" "${rpm_dir}/*.log"
    set -e
	echo "1..1" > $rpm_tap
	if [ $rc -gt 0 ]; then
	    echo "not ok 1 rpm package" >> $rpm_tap
	else
	    echo ok 1 rpm package >> $rpm_tap
	fi
fi


echo "[${0##*/}]..................exit code = $rc"
exit $rc
