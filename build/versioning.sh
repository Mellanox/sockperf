#!/bin/bash -e

VERSION=`cat version | tr -d '\n'`
GIT_REF=`git rev-parse HEAD 2> /dev/null || echo ""`
x=`git describe --long --abbrev=12 --dirty 2> /dev/null || echo ""`
if [ -n "$x" ]; then x=`echo $x | sed -e 's/-g/.git/' -e 's/-dirty/.dirty/' | sed s/.*-//`; else  x="no.git"; fi
VER_GIT=$x
