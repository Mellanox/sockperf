#!/bin/bash -xeEl

source $(dirname $0)/globals.sh

do_check_filter "Checking for csbuild ..." "on"

# This unit requires csbuild so check for existence
if [ $(command -v csbuild >/dev/null 2>&1 || echo $?) ]; then
	echo "[SKIP] csbuild tool does not exist"
	exit 0
fi

# There is a bug in gcc less than 4.5
if [ $(echo `gcc -dumpversion | cut -f1-2 -d.` \< 4.5 | bc ) -eq 1 ]; then
	echo "[SKIP] csbuild tool can not launch on this gcc"
	exit 0
fi

cd $WORKSPACE

rm -rf $csbuild_dir
mkdir -p $csbuild_dir
cd $csbuild_dir

set +eE

${WORKSPACE}/configure --prefix=${csbuild_dir}/install $jenkins_test_custom_configure > "${csbuild_dir}/csbuild.log" 2>&1
make clean

eval "csbuild --cswrap-timeout=180 --no-clean -c \"make $make_opt \" > \"${csbuild_dir}/csbuild.log\" 2>&1"
rc=$(($rc+$?))

eval "csgrep --quiet --event 'error|warning' \
	--path '^${WORKSPACE}' --strip-path-prefix '${WORKSPACE}' \
	--remove-duplicates '${csbuild_dir}/csbuild.log' | \
	csgrep --invert-match --path '^ksh-.*[0-9]+\.c$' | \
	csgrep --invert-match --checker CLANG_WARNING --event error | \
	csgrep --invert-match --checker CLANG_WARNING --msg \"internal warning\" | \
	csgrep --invert-match --checker COMPILER_WARNING --msg \"-Woverloaded-virtual\" | \
	csgrep --invert-match --checker COMPILER_WARNING --msg \"-Wformat-nonliteral\" | \
	csgrep --invert-match --checker CPPCHECK_WARNING --event 'preprocessorErrorDirective|syntaxError' | \
	csgrep --mode=grep --invert-match --event 'internal warning' --prune-events=1 | \
	cssort --key=path > ${csbuild_dir}/csbuild.err 2>&1 \
	"
eval "grep 'timed out' ${csbuild_dir}/csbuild.log >> ${csbuild_dir}/csbuild.err 2>&1"

set -eE

nerrors=$(cat ${csbuild_dir}/csbuild.err | grep 'Error:\|error:' | wc -l)
rc=$(($rc+$nerrors))

csbuild_tap=${WORKSPACE}/${prefix}/csbuild.tap

echo 1..1 > $csbuild_tap
if [ $rc -gt 0 ]; then
    echo "not ok 1 csbuild Detected $nerrors failures # ${csbuild_dir}/csbuild.err" >> $csbuild_tap
    do_err "csbuild" "${csbuild_dir}/csbuild.err"
    info="csbuild found $nerrors errors"
    status="error"
else
    echo ok 1 csbuild found no issues >> $csbuild_tap
    info="csbuild found no issues"
    status="success"
fi

do_archive "${csbuild_dir}/csbuild.err" "${csbuild_dir}/csbuild.log"

echo "[${0##*/}]..................exit code = $rc"
exit $rc
