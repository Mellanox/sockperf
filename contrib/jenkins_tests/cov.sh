#!/bin/bash -xeEl

source $(dirname $0)/globals.sh

echo "Checking for coverity ..."

do_module "tools/cov-2020.06"

cd $WORKSPACE

rm -rf $cov_dir
mkdir -p $cov_dir
cd $cov_dir

cov_exclude_file_list="tests"

cov_build_id="cov_build_${BUILD_NUMBER}"
cov_build="${cov_dir}/$cov_build_id"

set +eE

eval "${WORKSPACE}/configure --prefix=${cov_dir}/install $jenkins_test_custom_configure > ${cov_dir}/cov.log 2>&1"
make clean >> "${cov_dir}/cov.log" 2>&1
sleep 1
eval "cov-configure --config ${cov_dir}/coverity_config.xml --gcc >> ${cov_dir}/cov.log 2>&1"
sleep 1
eval "cov-build --config ${cov_dir}/coverity_config.xml --dir ${cov_build} make $make_opt >> ${cov_dir}/cov.log 2>&1"
rc=$(($rc+$?))

for excl in $cov_exclude_file_list; do
    cov-manage-emit --config ${cov_dir}/coverity_config.xml --dir ${cov_build} --tu-pattern "file('$excl')" delete >> "${cov_dir}/cov.log" 2>&1
    sleep 1
done

# List of translated units
eval "cov-manage-emit --config ${cov_dir}/coverity_config.xml --dir ${cov_build} list >> ${cov_dir}/cov.log 2>&1"
sleep 1

eval "cov-analyze --config ${cov_dir}/coverity_config.xml \
	--all --aggressiveness-level low \
	--enable-fnptr --fnptr-models --paths 20000 \
	--disable-parse-warnings \
	--dir ${cov_build}"
rc=$(($rc+$?))

set -eE

nerrors=$(cov-format-errors --dir ${cov_build} | awk '/Processing [0-9]+ errors?/ { print $2 }')
rc=$(($rc+$nerrors))

index_html=$(cd $cov_build && find . -name index.html | cut -c 3-)
cov_file="$cov_build/${index_html}"

coverity_tap=${WORKSPACE}/${prefix}/coverity.tap

echo 1..1 > $coverity_tap
if [ $rc -gt 0 ]; then
    echo "not ok 1 Coverity Detected $nerrors failures at ${cov_file}" >> $coverity_tap
    do_err "coverity" "${cov_build}/output/summary.txt"
else
    echo ok 1 Coverity found no issues >> $coverity_tap
fi

module unload "tools/cov-2020.06"

do_archive "$( find ${cov_build}/output -type f -name "*.txt" -or -name "*.html" -or -name "*.xml" )"

echo "[${0##*/}]..................exit code = $rc"
exit $rc
