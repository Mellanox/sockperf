#!/bin/bash -e

BASE_DIR=`pwd`
script_dir=`dirname $(readlink -f $0)`
cd $script_dir/..

source ./build/versioning.sh
APP_NAME=sockperf
MAINTAINER="Mellanox Technologies Ltd. <support@mellanox.com>"
DATE=`date -R`

APP_NAME_GIT=$APP_NAME-$GIT_REF
APP_VERSION=${VERSION}-${RELEASE}
TEMP_DIR=/tmp/$APP_NAME_GIT
rm -rf $TEMP_DIR
mkdir $TEMP_DIR
cp -a * $TEMP_DIR/ # do not copy hidden files like .git

cd $TEMP_DIR
echo $APP_VERSION > ./build/current-version
echo $GIT_REF >> ./build/current-version

if [ $# -lt 1 ]; then
        RPM_DIR=`rpm --eval '%{_topdir}'`
else
        RPM_DIR=$1
fi
APP_SPEC=$RPM_DIR/SPECS/$APP_NAME-$APP_VERSION.spec
sed -e s/__GIT_REF__/$GIT_REF/g -e s/__VERSION__/$VERSION/g -e s/__RELEASE__/$RELEASE/g ./build/$APP_NAME.spec.in > $APP_SPEC

sed -i -e s/__VERSION__/$VERSION/g -e s/__RELEASE__/$RELEASE/g -e s/__APP_NAME__/$APP_NAME/g -e "s/__DATE__/$DATE/g" -e "s/__MAINTAINER__/$MAINTAINER/g" debian/* 2> /dev/null || true

tar -zcf $RPM_DIR/SOURCES/$APP_NAME_GIT.tar.gz --exclude .git  -C .. $APP_NAME_GIT
rpmbuild -bs --define 'dist %{nil}' --define '_source_filedigest_algorithm md5' --define '_binary_filedigest_algorithm md5' --rmsource --rmspec --define "_topdir $RPM_DIR" $APP_SPEC
cd $BASE_DIR
rm -rf $TEMP_DIR
