#!/bin/bash -xeEl

source $(dirname $0)/globals.sh

echo "Checking for codying style ..."

do_module "dev/clang-9.0.1"

cd $WORKSPACE

rm -rf $style_dir
mkdir -p $style_dir
cd $style_dir

test_app="clang-format"

if [ $(command -v $test_app >/dev/null 2>&1 || echo $?) ]; then
    echo can not find $test_app
    exit 1
fi

style_tap=${WORKSPACE}/${prefix}/style_test.tap
rm -rf $style_tap
ln -sf $WORKSPACE/contrib/jenkins_tests/style.conf $WORKSPACE/.clang-format


check_files=$(find $WORKSPACE/src/ -name '*.c' -o -name '*.cpp' -o -name '*.h')

i=0
nerrors=0

for file in $check_files; do
    set +eE
    style_diff="${style_dir}/$(basename ${file}).diff"
    eval "env $test_app -style=file \
        ${file} \
        | diff -u ${file} - | sed -e '1s|-- |--- a/|' -e '2s|+++ -|+++ b/$file|' \
        > ${style_diff} 2>&1"
    [ -s ${style_diff} ]
    ret=$((1-$?))
    nerrors=$((nerrors+ret))
    set -eE

    file=$(basename ${file})
    if [ $ret -gt 0 ]; then
        i=$((i+1))
        echo "not ok $i $file # See: ${file}.diff" >> $style_tap
    else
        rm -rf ${style_diff}
    fi
done
if [ $nerrors -eq 0 ]; then
    echo "1..1" > $style_tap
    echo "ok 1 all $(echo "$check_files" | wc -l) files" >> $style_tap
else
    mv $style_tap ${style_tap}.backup
    echo "1..$(cat ${style_tap}.backup | wc -l)" > $style_tap
    cat ${style_tap}.backup >> $style_tap
    rm -rf ${style_tap}.backup
fi
rc=$(($rc+$nerrors))

module unload "dev/clang-9.0.1"

do_archive "${style_dir}/*.diff"

echo "[${0##*/}]..................exit code = $rc"
exit $rc
