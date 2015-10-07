#!/bin/bash -e

APP_NAME=sockperf
SRCRPM_EXT=.src.rpm
SPEC_EXT=.spec
TARBALL_EXT=.tar.gz

BASE_DIR=`pwd`
DATE=`date -R`
MAINTAINER="Mellanox Technologies Ltd. <support@mellanox.com>"

if [ $# -lt 1 ]; then
        echo -e "Usage is:\n\t $0 <path to $APP_NAME-*$SRCRPM_EXT file> <arch>" > /dev/stderr
	exit 1
fi

SRCRPM_FILE=`readlink -f $1`
SPECFILE=`rpm2cpio $SRCRPM_FILE | cpio -t 2> /dev/null | grep "$SPEC_EXT\$"`
APP_NAME_VER=${SPECFILE%$SPEC_EXT}  # sockperf-2.7-13.gitd8618862f05d.dirty
FULL_VER=${APP_NAME_VER#$APP_NAME-} # 2.7-13.gitd8618862f05d.dirty
RELEASE=${FULL_VER#*-}              # 13.gitd8618862f05d.dirty
VERSION=${FULL_VER%-$RELEASE}       # 2.7
DEBIAN_APP_NAME_VER=${APP_NAME}_${VERSION}-${RELEASE} # sockperf_2.7-13.gitd8618862f05d.dirty
TEMP_DIR=/tmp/for-deb-$APP_NAME_VER
rm -rf $TEMP_DIR; mkdir $TEMP_DIR; cd $TEMP_DIR
rpm2cpio $SRCRPM_FILE | cpio -i 2> /dev/null
OLD_NAME=`basename --suffix $TARBALL_EXT $APP_NAME-*$TARBALL_EXT`
mv $OLD_NAME$TARBALL_EXT $DEBIAN_APP_NAME_VER$TARBALL_EXT
ln -s $DEBIAN_APP_NAME_VER$TARBALL_EXT ${APP_NAME}_${VERSION}.orig$TARBALL_EXT
tar xf $DEBIAN_APP_NAME_VER$TARBALL_EXT
mv $OLD_NAME $DEBIAN_APP_NAME_VER
cd $DEBIAN_APP_NAME_VER
sed -i -e s/__VERSION__/$VERSION/g -e s/__RELEASE__/$RELEASE/g -e s/__APP_NAME__/$APP_NAME/g -e "s/__DATE__/$DATE/g" -e "s/__MAINTAINER__/$MAINTAINER/g" debian/* 2> /dev/null || true
debuild -us -uc
cd $BASE_DIR
cp $TEMP_DIR/*deb .
rm -rf $TEMP_DIR
