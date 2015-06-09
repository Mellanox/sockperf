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

if [ "x$1" = "xc" ]; then
	shift
	$topdir/configure CXXFLAGS='-g -O0 -Wall -Werror' $@
	make clean
else
	printf "\nNow run '$topdir/configure' and 'make'.\n\n"
fi

exit 0
