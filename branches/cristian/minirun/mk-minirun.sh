#!/bin/bash
# Klik2 Minirun - Proof of Concept
# Copyright (C) 2008 Cristian Greco
# GPLv2 only

if [ $# -ne 1 ]; then
	echo "usage: `basename $0` <imagefile.cmg>"
	exit 1
fi

RUNTIME="runtime.o"
RUNTIME_TAR="runtime.tgz"
RUNTIME_FILES="libcdio.so.7 libiso9660.so.5 libumlib.so.0
	       umbinwrap umfuseiso9660.so umfuse.so umview"

IMAGE_CMG=$1
IMAGE="image.o"
IMAGE_TAR="image.tgz"

mkdir -p klik
TMPDIR="klik"

cp $RUNTIME_FILES $TMPDIR/
cp $IMAGE_CMG $TMPDIR/image.cmg
cp minirun.c $TMPDIR


(
cd $TMPDIR
tar zcf $RUNTIME_TAR $RUNTIME_FILES
ld -r -b binary -o $RUNTIME $RUNTIME_TAR
objcopy --rename-section .data=.klik.runtime,alloc,load,readonly,data,cont $RUNTIME

tar zcf $IMAGE_TAR image.cmg
ld -r -b binary -o $IMAGE $IMAGE_TAR
objcopy --rename-section .data=.klik.image,alloc,load,readonly,data,cont $IMAGE

gcc -c minirun.c
)

gcc $TMPDIR/minirun.o $TMPDIR/$RUNTIME $TMPDIR/$IMAGE -o minirun
rm -rf $TMPDIR
