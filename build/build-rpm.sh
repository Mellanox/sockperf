#!/bin/bash -e

#################################################################
######## create *.src.rpm based on specfile template ############
#################################################################

BASE_DIR=`pwd`
script_dir=`dirname $(readlink -f $0)`
cd $script_dir/..

source ./build/versioning.sh
APP_NAME=sockperf
MAINTAINER="Mellanox Technologies Ltd. <support@mellanox.com>"
DATE=`date -R`

GITHUB_REF=$APP_NAME-$GIT_REF
FULL_VER=${VERSION}-${RELEASE}
echo $FULL_VER > ./build/current-version
echo $GIT_REF >> ./build/current-version

APP_NAME_VER=$APP_NAME-$FULL_VER

#DIRNAME=$GITHUB_REF  # match Fedora original style
DIRNAME=$APP_NAME_VER # better MOFED
DIRNAME=$APP_NAME-$VERSION

TEMP_DIR=/tmp/$DIRNAME
rm -rf $TEMP_DIR; mkdir $TEMP_DIR
cp -a * $TEMP_DIR/ # do not copy hidden files like .git
cd $TEMP_DIR


if [ $# -lt 1 ]; then
        RPM_DIR=`rpm --eval '%{_topdir}'`
else
        RPM_DIR=$1
fi

mkdir -p --verbose $RPM_DIR/{BUILD,RPMS,SOURCES,SPECS,SRPMS,tmp}

APP_SPEC=$RPM_DIR/SPECS/$APP_NAME_VER.spec
sed -e s/__GIT_REF__/$GIT_REF/g -e s/__VERSION__/$VERSION/g -e s/__RELEASE__/$RELEASE/g ./build/$APP_NAME.spec.in > $APP_SPEC
sed -i -e s/__VERSION__/$VERSION/g -e s/__RELEASE__/$RELEASE/g -e s/__APP_NAME__/$APP_NAME/g -e "s/__DATE__/$DATE/g" -e "s/__MAINTAINER__/$MAINTAINER/g" debian/* 2> /dev/null || true


./autogen.sh

tar -zcf $RPM_DIR/SOURCES/$DIRNAME.tar.gz --exclude .git  -C .. $DIRNAME
env RPM_BUILD_NCPUS=${NPROC} rpmbuild -bs --define 'dist %{nil}' --define '_source_filedigest_algorithm md5' --define '_binary_filedigest_algorithm md5' --rmsource --rmspec --define "_topdir $RPM_DIR" $APP_SPEC
cd $BASE_DIR
rm -rf $TEMP_DIR

# mv XXX.src.rpm from $RPM_DIR/SRPMS -> /.autodirect/mswg/release/sockperf/ and update ./latest.txt
