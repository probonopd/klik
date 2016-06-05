#!/bin/bash

if [ -x automake-1.9 ]; then
	automake_version="-1.9"
elif [ -x automake-1.10 ]; then
	automake_version="-1.10"
fi

rm -f aclocal.m4 configure

aclocal${automake_version} -I m4
autoheader
libtoolize --force --copy
automake${automake_version} --add-missing --copy
autoconf
rm -rf autom4te.cache

