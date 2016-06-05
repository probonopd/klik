"""
    This takes a cmg and injects the miniruntime into it. It expects 
    fusioniso.dyn to be present in the same directory as itself. 
    It should be stripped using "strip -v *" for additional space savings.
    Everything in this directory will be put into a tbz2 and appended 
    to the end of the dmg
    from where the files will be extracted and run when the cmg is executed.
"""

import os, sys, bz2
from subprocess import *

PATH_TO_EXECUTABLES = ""
MINI_RUN_SCRIPT = """#!/bin/bash -xe

OFFSET="%s"
FILE="$0"
#PWD=$(pwd)
TMP="/tmp/${HOME}/._run/_$(basename ${FILE})"
MOUNTPOINT="${TMP}/mnt" # Nautilus shows this if not in /tmp
echo "${MOUNTPOINT}"
mkdir -p "${MOUNTPOINT}"
tail -c +$OFFSET "${FILE}" | bunzip2 | tar -C ${TMP} -xvf - >/dev/null
"${TMP}/fusioniso.dyn" -up "${FILE}" "${MOUNTPOINT}"
# the next line makes sure we unmount and clean up at the exit
trap 'fusermount -z -u "${MOUNTPOINT}" ; rm -r "${TMP}"' EXIT
APP=$(grep "group main" "${MOUNTPOINT}/recipe.xml" | cut -d "\\"" -f 2)
echo "${APP}"
# we don't want to call ourself if user has put us there
PATH=$(echo $PATH | sed -e 's;:\?/usr/local/bin;;' -e 's;/usr/local/bin:\?;;')
export FAKECHROOT_EXCLUDE_PATHS="/tmp:/proc:/dev:/var/run"
export LD_PRELOAD=${TMP}/libkfakechroot.so
export PATH=/usr/sbin:$PATH # otherwise some distros don't find chroot
chroot "${MOUNTPOINT}" "${APP}" "$@"
sleep 0.5
exit 0

"""

class KlikStandAlone():

    def __init__(self, run_time_path):
        self.run_time_path = run_time_path
        return

    def inject(self, cmg_path):

        offset = str(os.stat(cmg_path)[6]+1)
        
        # append binary
        f = file(cmg_path, 'a') # append mode
        s = file(self.run_time_path, 'r')
        f.write(s.read())
        s.close()        
        f.close()
        
        # prepend script
        f = file(sourcecmg, 'r+') # prepend mode
        f.write(MINI_RUN_SCRIPT % offset)
        f.close()
`       
if __name__ == '__main__':
    if len(sys.argv) < 2:
        print ("")
        print ("This embeds a minimal runtime into the cmg and makes it self-executable")
        print ("")
        print ("Usage: %s cmg" % (os.path.basename(sys.argv[0])))
        print ("")
        exit(1)

    ksa = KlikStandAlone(os.path.join(os.path.realpath(os.path.dirname(__file__)), "runtime.tbz2"))
    ksa.inject( os.path.realpath(sys.argv[1]) )


