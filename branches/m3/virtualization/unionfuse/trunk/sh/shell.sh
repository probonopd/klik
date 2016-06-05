#!/bin/bash
# Version 20070908
# Lionel Tricon <lionel.tricon at free dot fr>
DIRNAME=`cd \`dirname $0\` && cd .. && pwd`
export PATH=$DIRNAME/bin:/sbin:$PATH

# parameters management
args=`getopt db:B: "$@"`
set -- $args
for arg
do
  case "$arg" in
    -b)
      shift
      SANDBOX_PATH="-b $1"
      shift
      ;;
    -B)
      shift
      SANDBOX_PATH="-B $1"
      shift
      ;;
    -d)
      shift
      DEBUG_OUTPUT=`mktemp /tmp/klik2-output.XXXXX`
      ;;
  esac
done

# Help
shift
if [ "$1" == "" ]; then
  echo "Syntax: $0 [-d] [-b/B <path>] <cmg_image_file>"
  echo "-b <path>     -- sandbox feature for klik2 (redirect all modification from $HOME to <path>)"
  echo "-B <path>     -- sandbox feature for klik2 (redirect all modification to <path>)"
  echo "-d            -- debug mode (redirect all fuse* output to /tmp)"
  exit 1
else
  CMG_FILE="$@"
  shift
fi

derror() {
  echo "**"
  echo "** ERROR: $1"
  echo "**"
  exit 1
}

# mount directory
MNTNUM="1"
mkdir -p /tmp/app
chmod 777 /tmp/app
while true; do
  [ -e "/tmp/app/$MNTNUM" ] || break
  MNTNUM=$(($MNTNUM+1))
done

# fuse
mkdir -p /tmp/app/$MNTNUM
mkdir -p /tmp/app/$MNTNUM/mnt
mkdir -p /tmp/app/$MNTNUM/pid
ln -s "$CMG_FILE" /tmp/app/$MNTNUM/image

# fuse[iso|cram]
# Determine which filesystem is used as .cmg
(file -i "$CMG_FILE" | grep "x-iso9660") >/dev/null && FS=iso
(file "$CMG_FILE" | grep "Compressed ROM") >/dev/null && FS=cram

# We mount the fuse filesystem
export FAKECHROOT_EXCLUDE_PATHS=/tmp:/proc:/dev:/var/run
if [ -n "$FS" ]
then
  if [ "${DEBUG_OUTPUT}" == "" ]; then
    $DIRNAME/bin/fuse${FS} -n -u ${SANDBOX_PATH} /tmp/app/$MNTNUM/image /tmp/app/$MNTNUM/mnt/ -s
  else
    $DIRNAME/bin/fuse${FS}-d -n -u ${SANDBOX_PATH} /tmp/app/$MNTNUM/image /tmp/app/$MNTNUM/mnt/ -s -d 2>&1 > ${DEBUG_OUTPUT} &
    echo "Debug mode >> ${DEBUG_OUTPUT}"
    sleep 1
  fi
else
  derror "Filesystem unsupported"
fi

# Check if the filesystem is mounted
test -d /tmp/app/$MNTNUM/mnt/etc || {
  derror "Unable to mount $MOUNT"
  exit 1
}

# chroot
export KDE_FORK_SLAVES=1
export FAKECHROOT_BASE=/tmp/app/$MNTNUM/mnt
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$DIRNAME/lib:/tmp/app/$MNTNUM/mnt/lib:/tmp/app/$MNTNUM/mnt/usr/lib/
export LD_PRELOAD=$LD_PRELOAD:libfakechroot.so
/bin/bash
unset LD_PRELOAD
unset FAKECHROOT_BASE
unset LD_LIBRARY_PATH
unset KDE_FORK_SLAVES
unset FAKECHROOT_EXCLUDE_PATHS

# unmount and clean up
COUNT=1
grep /tmp/app/$MNTNUM/mnt /proc/mounts 2>&1 >/dev/null && {
  until $DIRNAME/bin/fusermount -u /tmp/app/$MNTNUM/mnt/; do
    sleep 1
    fuser -k /tmp/app/$MNTNUM/mnt 2>&1 >/dev/null
    COUNT=$(($COUNT+1))
    if [ $COUNT -gt 5 ]; then
      $DIRNAME/bin/fusermount -u -z /tmp/app/$MNTNUM/mnt/
      break
    fi
  done
}

# garbage collector
rm -f /tmp/app/$MNTNUM/image
rm -rf /tmp/app/$MNTNUM/pid
rmdir /tmp/app/$MNTNUM/mnt
rmdir /tmp/app/$MNTNUM

exit 0
