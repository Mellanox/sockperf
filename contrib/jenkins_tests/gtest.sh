#!/bin/bash -eExl

source $(dirname $0)/globals.sh

echo "Checking for gtest ..."

# Check dependencies
if [ $(test -d ${install_dir} >/dev/null 2>&1 || echo $?) ]; then
	echo "[SKIP] Not found ${install_dir} : build should be done before this stage"
	exit 1
fi

cd $WORKSPACE

rm -rf $gtest_dir
mkdir -p $gtest_dir
cd $gtest_dir

gtest_app="$PWD/tests/gtest/gtest"
gtest_lib=$install_dir/lib/${prj_lib}

set +eE

${WORKSPACE}/configure --prefix=$install_dir --enable-test
make -C tests/gtest
rc=$(($rc+$?))

$timeout_exe env GTEST_TAP=2 $gtest_app --gtest_output=xml:${WORKSPACE}/${prefix}/test-basic.xml
rc=$(($rc+$?))

set -eE

for f in $(find $gtest_dir -name '*.tap')
do
    cp $f ${WORKSPACE}/${prefix}/gtest-$(basename $f .tap).tap
done

echo "[${0##*/}]..................exit code = $rc"
exit $rc
