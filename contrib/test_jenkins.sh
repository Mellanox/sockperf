#!/bin/bash -El
#
# Testing script for SOCKPERF, to run from Jenkins CI
#
# Copyright (C) Mellanox Technologies Ltd. 2011-2020.  ALL RIGHTS RESERVED.
#
# See file LICENSE for terms.
#
#
# Environment variables set by Jenkins CI:
#  - WORKSPACE         : path to working directory
#  - BUILD_NUMBER      : jenkins build number
#  - JOB_URL           : jenkins job url
#  - JENKINS_RUN_TESTS : whether to run unit tests
#  - TARGET            : target configuration
#

echo "======================================================"
echo
echo "# starting on host --------->  $(hostname) "
echo "# arguments called with ---->  ${@}        "
echo "# path to me --------------->  ${0}        "
echo "# parent path -------------->  ${0%/*}     "
echo "# name --------------------->  ${0##*/}    "
echo

PATH=${PATH}:/hpc/local/bin:/hpc/local/oss/vma/
MODULEPATH=${MODULEPATH}:/hpc/local/etc/modulefiles
env
for f in autoconf automake libtool ; do $f --version | head -1 ; done
echo "======================================================"

source $(dirname $0)/jenkins_tests/globals.sh

set -xe
# check go/not go
#
do_check_env

rel_path=$(dirname $0)
abs_path=$(readlink -f $rel_path)

# Values: none, fail, always
#
jenkins_opt_artifacts=${jenkins_opt_artifacts:="always"}

# Values: 0..N test (max 100)
#
jenkins_opt_exit=${jenkins_opt_exit:="6"}

# Test scenario list
#
jenkins_test_build=${jenkins_test_build:="yes"}

jenkins_test_compiler=${jenkins_test_compiler:="yes"}
jenkins_test_rpm=${jenkins_test_rpm:="yes"}
jenkins_test_cov=${jenkins_test_cov:="no"}
jenkins_test_cppcheck=${jenkins_test_cppcheck:="yes"}
jenkins_test_csbuild=${jenkins_test_csbuild:="no"}
jenkins_test_run=${jenkins_test_run:="no"}
jenkins_test_style=${jenkins_test_style:="no"}


echo Starting on host: $(hostname)

cd $WORKSPACE

rm -rf ${WORKSPACE}/${prefix}
rm -rf autom4te.cache

./autogen.sh -s


for target_v in "${target_list[@]}"; do
    ret=0
    IFS=':' read target_name target_option <<< "$target_v"

    export jenkins_test_artifacts="${WORKSPACE}/${prefix}/sockperf-${BUILD_NUMBER}-$(hostname -s)-${target_name}"
    export jenkins_test_custom_configure="${target_option}"
    export jenkins_target="${target_name}"
    set +x
    echo "======================================================"
    echo "Jenkins is checking for [${target_name}] target ..."
    echo "======================================================"
    set -x

    # check building and exit immediately in case failure
    #
    if [ "$jenkins_test_build" = "yes" ]; then
        $WORKSPACE/contrib/jenkins_tests/build.sh
        ret=$?
        if [ $ret -gt 0 ]; then
           do_err "case: [build: ret=$ret]"
        fi
        rc=$((rc + $ret))
    fi

    # check other units w/o forcing exiting
    #
    set +e
    if [ 1 -lt "$jenkins_opt_exit" -o "$rc" -eq 0 ]; then
	    if [ "$jenkins_test_compiler" = "yes" ]; then
	        $WORKSPACE/contrib/jenkins_tests/compiler.sh
	        ret=$?
	        if [ $ret -gt 0 ]; then
	           do_err "case: [compiler: ret=$ret]"
	        fi
	        rc=$((rc + $ret))
	    fi
    fi
    if [ 2 -lt "$jenkins_opt_exit" -o "$rc" -eq 0 ]; then
	    if [ "$jenkins_test_rpm" = "yes" ]; then
	        $WORKSPACE/contrib/jenkins_tests/rpm.sh
	        ret=$?
	        if [ $ret -gt 0 ]; then
	           do_err "case: [rpm: ret=$ret]"
	        fi
	        rc=$((rc + $ret))
	    fi
	fi
    if [ 3 -lt "$jenkins_opt_exit" -o "$rc" -eq 0 ]; then
	    if [ "$jenkins_test_cov" = "yes" ]; then
	        $WORKSPACE/contrib/jenkins_tests/cov.sh
	        ret=$?
	        if [ $ret -gt 0 ]; then
	           do_err "case: [cov: ret=$ret]"
	        fi
	        rc=$((rc + $ret))
	    fi
    fi
    if [ 4 -lt "$jenkins_opt_exit" -o "$rc" -eq 0 ]; then
	    if [ "$jenkins_test_cppcheck" = "yes" ]; then
	        $WORKSPACE/contrib/jenkins_tests/cppcheck.sh
	        ret=$?
	        if [ $ret -gt 0 ]; then
	           do_err "case: [cppcheck: ret=$ret]"
	        fi
	        rc=$((rc + $ret))
	    fi
    fi
    if [ 5 -lt "$jenkins_opt_exit" -o "$rc" -eq 0 ]; then
	    if [ "$jenkins_test_csbuild" = "yes" ]; then
	        $WORKSPACE/contrib/jenkins_tests/csbuild.sh
	        ret=$?
	        if [ $ret -gt 0 ]; then
	           do_err "case: [csbuild: ret=$ret]"
	        fi
	        rc=$((rc + $ret))
	    fi
    fi
    if [ 6 -lt "$jenkins_opt_exit" -o "$rc" -eq 0 ]; then
	    if [ "$jenkins_test_run" = "yes" ]; then
	        $WORKSPACE/contrib/jenkins_tests/test.sh
	        ret=$?
	        if [ $ret -gt 0 ]; then
	           do_err "case: [test: ret=$ret]"
	        fi
	        rc=$((rc + $ret))
	    fi
    fi
    if [ 7 -lt "$jenkins_opt_exit" -o "$rc" -eq 0 ]; then
	    if [ "$jenkins_test_style" = "yes" ]; then
	        $WORKSPACE/contrib/jenkins_tests/style.sh
	        ret=$?
	        if [ $ret -gt 0 ]; then
	           do_err "case: [style: ret=$ret]"
	        fi
	        rc=$((rc + $ret))
	    fi
    fi
    set -e

    # Archive all logs in single file
    do_archive "${WORKSPACE}/${prefix}/${target_name}/*.tap"

    if [ "$jenkins_opt_artifacts" == "always" ] || [ "$jenkins_opt_artifacts" == "fail" -a "$rc" -gt 0 ]; then
	    set +x
	    gzip "${jenkins_test_artifacts}.tar"
	    echo "======================================================"
	    echo "Jenkins result for [${target_name}] target: return $rc"
	    echo "Artifacts: ${jenkins_test_artifacts}.tar.gz"
	    echo "======================================================"
	    set -x
    fi

done

rm -rf $WORKSPACE/config.cache

echo "[${0##*/}]..................exit code = $rc"
exit $rc
