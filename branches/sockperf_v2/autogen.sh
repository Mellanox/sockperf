#!/bin/sh
set -ex
rm -rf autom4te.cache
mkdir -p config/m4
autoreconf -v --install || exit 1
rm -rf autom4te.cache
 
exit 0
