#!/bin/bash -e
##############################################################################
# this script will extract content of a given *.src.rpm file (that its tarball 
# contains ./debian/ subfolder) and will compile it into *.deb binary
##############################################################################

APP_NAME=sockperf
SRCRPM_EXT=.src.rpm
TARBALL_EXT=.tar.gz
BASE_DIR=`pwd`
if [ $# -lt 1 ]; then
        echo -e "Usage is:\n\t $0 <path to $APP_NAME-*$SRCRPM_EXT file>" > /dev/stderr
	exit 1
fi

SRCRPM_FILE=`readlink -f $1`
APP_NAME_VER=`basename $SRCRPM_FILE $SRCRPM_EXT` # sockperf-2.7-13.gitd8618862f05d.dirty
FULL_VER=${APP_NAME_VER#$APP_NAME-} # 2.7-13.gitd8618862f05d.dirty
VERSION=${FULL_VER%%-*}       # 2.7

TEMP_DIR=/tmp/for-deb-$APP_NAME_VER
rm -rf $TEMP_DIR; mkdir $TEMP_DIR; cd $TEMP_DIR
rpm2cpio $SRCRPM_FILE | cpio -i 2> /dev/null # extract *.src.rpm
OLD_NAME=`basename $APP_NAME-*$TARBALL_EXT $TARBALL_EXT`

ln -s $OLD_NAME$TARBALL_EXT ${APP_NAME}_${VERSION}.orig$TARBALL_EXT
tar xf $OLD_NAME$TARBALL_EXT
cd $OLD_NAME

dpkg-buildpackage -us -uc # actual build of *.deb
cd $BASE_DIR
cp --verbose $TEMP_DIR/*.deb .
rm -rf $TEMP_DIR
