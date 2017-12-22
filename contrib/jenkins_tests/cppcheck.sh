#!/bin/bash -xeEl

source $(dirname $0)/globals.sh

do_check_filter "Checking for cppcheck ..." "on"

# This unit requires cppcheck so check for existence
if [ $(command -v cppcheck >/dev/null 2>&1 || echo $?) ]; then
	echo "[SKIP] cppcheck tool does not exist"
	exit 0
fi

cd $WORKSPACE

rm -rf $cppcheck_dir
mkdir -p $cppcheck_dir
cd $cppcheck_dir

set +eE
eval "find ${WORKSPACE}/src -name '*.h' -o -name '*.cpp' -o -name '*.c' -o -name '*.hpp' -o -name '*.inl' | \
	cppcheck --std=c99 --std=c++11 --language=c++ --force --enable=information \
	--inline-suppr --suppress=memleak:config_parser.y \
	--template='{severity}: {id}: {file}:{line}: {message}' \
	--file-list=- 2> ${cppcheck_dir}/cppcheck.err 1> ${cppcheck_dir}/cppcheck.log"
rc=$(($rc+$?))
set -eE

nerrors=$(cat ${cppcheck_dir}/cppcheck.err | grep error | wc -l)
rc=$(($rc+$nerrors))

cppcheck_tap=${WORKSPACE}/${prefix}/cppcheck.tap

echo 1..1 > $cppcheck_tap
if [ $rc -gt 0 ]; then
    echo "not ok 1 cppcheck Detected $nerrors failures # ${cppcheck_dir}/cppcheck.err" >> $cppcheck_tap
    do_err "cppcheck" "${cppcheck_dir}/cppcheck.err"
    info="cppcheck found $nerrors errors"
    status="error"
    do_err "$1" "${5}.err"
else
    echo ok 1 cppcheck found no issues >> $cppcheck_tap
    info="cppcheck found no issues"
    status="success"
fi

do_archive "${cppcheck_dir}/cppcheck.err" "${cppcheck_dir}/cppcheck.log"

echo "[${0##*/}]..................exit code = $rc"
exit $rc
