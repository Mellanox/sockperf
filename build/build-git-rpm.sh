#!/bin/bash -e

base_dir=`pwd`
script_dir=`dirname $(readlink -f $0)`
cd $script_dir/..

source ./build/versioning.sh
sockperf_dir=sockperf-$GIT_REF
rm -rf /tmp/$sockperf_dir
mkdir /tmp/$sockperf_dir
cp -a * /tmp/$sockperf_dir/ # do not copy hidden files like .git

cd /tmp/$sockperf_dir
echo ${VERSION}-${VER_GIT} > ./build/current-version
echo $GIT_REF >> ./build/current-version
sed -e s/__GIT_REF__/$GIT_REF/g -e s/__VERSION__/$VERSION/g -e s/__VER_GIT__/$VER_GIT/g ./build/sockperf.spec.in > ./build/sockperf.spec

tar -zcf ../sockperf-$GIT_REF.tar.gz --exclude .git  -C .. $sockperf_dir
mv ../sockperf-$GIT_REF.tar.gz ~/rpmbuild/SOURCES/ # TEMP

rpmbuild -ba --rmsource --rmspec ./build/sockperf.spec
cd $base_dir
