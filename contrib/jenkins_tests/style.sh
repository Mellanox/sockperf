#!/bin/bash -xeEl

source $(dirname $0)/globals.sh

do_check_filter "Checking for codying style ..." "on"

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

echo "1..$(echo $check_files | wc -w)" > $style_tap
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
    i=$((i+1))

    file=$(basename ${file})
    if [ $ret -gt 0 ]; then
        echo "not ok $i $file # See: ${file}.diff" >> $style_tap
    else
        echo "ok $i $file" >> $style_tap
    fi
done

do_archive "${style_dir}/*.diff"
rc=$(($rc+$nerrors))

echo "[${0##*/}]..................exit code = $rc"
exit $rc
