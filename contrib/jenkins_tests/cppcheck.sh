#!/bin/bash -xeEl

source $(dirname $0)/globals.sh

echo "Checking for cppcheck ..."

tool_app=cppcheck

# This unit requires cppcheck so check for existence
if [ $(command -v ${tool_app} >/dev/null 2>&1 || echo $?) ]; then
    set +e
    eval "timeout -s SIGKILL 20s https://github.com/danmar/cppcheck.git cppcheck " > /dev/null 2>&1
    if [ $? -eq 0 ]; then
        eval "cd cppcheck && checkout 2.1 " > /dev/null 2>&1
        if [ $? -eq 0 ]; then
            eval "make $make_opt FILESDIR=$PWD HAVE_RULES=yes " > /dev/null 2>&1
            if [ $? -eq 0 ]; then
                tool_app=$PWD/cppcheck
            fi
        fi
        cd ..
    fi
    set -e

    if [ $(command -v ${tool_app} >/dev/null 2>&1 || echo $?) ]; then
        echo "[SKIP] cppcheck tool does not exist"
        exit 1
    fi
fi

echo $(${tool_app} --version)

cd $WORKSPACE

rm -rf $cppcheck_dir
mkdir -p $cppcheck_dir
cd $cppcheck_dir

${WORKSPACE}/configure $jenkins_test_custom_configure > "${cppcheck_dir}/cppcheck.log" 2>&1

set +eE
eval "find ${WORKSPACE}/src -name '*.h' -o -name '*.cpp' -o -name '*.c' -o -name '*.hpp' -o -name '*.inl' | \
	${tool_app} --std=c99 --std=c++11 --language=c++ --force --enable=information \
	-I${WORKSPACE}/src \
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
else
    echo ok 1 cppcheck found no issues >> $cppcheck_tap
    info="cppcheck found no issues"
    status="success"
fi

do_archive "${cppcheck_dir}/cppcheck.err" "${cppcheck_dir}/cppcheck.log"

echo "[${0##*/}]..................exit code = $rc"
exit $rc
