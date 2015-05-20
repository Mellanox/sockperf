#!/bin/sh
set -ex

oldpwd=$(pwd)
topdir=$(dirname "$0")
cd "$topdir"

rm -rf autom4te.cache
mkdir -p config/m4 config/aux
autoreconf -v --install || exit 1
rm -rf autom4te.cache

cd "$oldpwd"

exit 0
