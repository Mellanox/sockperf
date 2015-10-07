#!/bin/bash -e

base_dir=`pwd`
script_dir=`dirname $(readlink -f $0)`
cd $script_dir/..

source ./build/versioning.sh
APP_NAME=sockperf
APP_NAME_GIT=$APP_NAME-$GIT_REF
APP_VERSION=${VERSION}-${VER_GIT}
APP_SPEC=./build/$APP_NAME-$APP_VERSION.spec
TEMP_DIR=/tmp/$APP_NAME_GIT
rm -rf $TEMP_DIR
mkdir $TEMP_DIR
cp -a * $TEMP_DIR/ # do not copy hidden files like .git

cd $TEMP_DIR
echo $APP_VERSION > ./build/current-version
echo $GIT_REF >> ./build/current-version
sed -e s/__GIT_REF__/$GIT_REF/g -e s/__VERSION__/$VERSION/g -e s/__VER_GIT__/$VER_GIT/g ./build/$APP_NAME.spec.in > $APP_SPEC

if [ $# -lt 1 ]; then
        RPM_DIR=`rpm --eval '%{_topdir}'`
else
        RPM_DIR=$1
fi
tar -zcf $RPM_DIR/SOURCES/$APP_NAME_GIT.tar.gz --exclude .git  -C .. $APP_NAME_GIT
rpmbuild -ba --rmsource --rmspec --define "_topdir $RPM_DIR" $APP_SPEC
cd $base_dir
rm -rf $TEMP_DIR
