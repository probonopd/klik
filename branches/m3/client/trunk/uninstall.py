#!/usr/bin/env python
"""
	- for some reason call("cp","PATH/*","PATH") fails
	- Need an uninstall as well... note we will package for deb and rpm so just for the installer script
"""

import os
import sys
import shutil
import stat
import os.path
from subprocess import *
from optparse import OptionParser

opt = OptionParser()
opt.add_option("--prefix",
               metavar="<dir>",
               dest="prefix",
               default = "/opt/klik")

(options, args) = opt.parse_args()

klik_install_path = options.prefix
klik_files_path = sys.path[0]

sys.path.insert(1, klik_files_path + "/klikclient/lib")

# XDG UTILS
from klik.utils.xdg import XdgUtils
xdg = XdgUtils( os.path.join(sys.path[0], "klikclient", "lib", "klik", "utils", "xdg-utils"), "SYSTEM" )

# Check for root
if xdg.is_root() == False:
	print ("You must be root in order to run "+sys.argv[0]+", exiting")
	sys.exit(1)
else:
	print "Privileged mode detected ..."

# Remove sub folders
print ""
print "Remove klik sub folders ..."
path = os.path.join(klik_install_path, "share/klik")
if not os.path.exists(path):
	shutil.rmtree(path, True)
path = os.path.join(klik_install_path, "lib/klik")
if not os.path.exists(path):
	shutil.rmtree(path, True)

# If bash_completion is installed, uninstall completion file
bash_completion_file = os.path.join( "/etc", "bash_completion.d", "klik" )
if os.path.exists(bash_completion_file):
	os.remove( bash_completion_file )

# Remove Executables
os.path.join(klik_install_path, "bin", "fusecram")
os.path.join(klik_install_path, "bin", "fuseiso")
os.path.join(klik_install_path, "bin", "fusermount")
os.path.join(klik_install_path, "bin", "genisoimage")
os.path.join(klik_install_path, "bin", "isoinfo")
os.path.join(klik_install_path, "bin", "klik")
os.path.join(klik_install_path, "bin", "mkcramfs")
os.path.join(klik_install_path, "bin", "mkzftree")
"/usr/local/bin/klik"

# Remove Libraries
os.path.join(klik_install_path, "lib", "libfakechroot.so")

# CMG MIMETYPE
print ""
print "Removing file type (.cmg) from system ..."
cmg_mimetype = os.path.join(klik_files_path , "install", "cmg-mimetype.xml")
xdg.mime_uninstall( cmg_mimetype )

#Default icon for mimetype
print ""
print "Removing file type icon (.cmg) from system ..."
cmg_icon = os.path.join(klik_files_path , "install", "application-x-extension-cmg.png")
xdg.mime_icon_uninstall( "48", cmg_icon, "application-x-extension-cmg")

# RECIPIE MIMETYPE
#recipe_mimetype = os.path.join(sys.path[0] , "install", "gnome", "cmg-mimetype.xml")

# APPLICATION MENU / FILE ASSOCIATION
print ""
print "Removing application association for (.cmg) ..."
klik_desktop = os.path.join(klik_files_path , "install", "klik.desktop")
xdg.desktop_uninstall( klik_desktop )

# THUMBNAILER
if xdg.get_desktop_enviroment() == "GNOME":
	# Set destination to system defaults

	config_sources = []
	config_sources.append( "xml:readonly:/etc/gconf/gconf.xml.defaults" )		# System defaults
	config_sources.append( "xml:readwrite:" + os.environ["HOME"] + "/.gconf" )	# Current User (avoid needing login/out))

	print ""
	print "Settings GCONF keys"
	for config_source in config_sources:
		# Set source
		os.environ["GCONF_CONFIG_SOURCE"] = config_source
	
		# Register the thumbnailer 
		cmg_gnome_thumbnail = os.path.join(klik_files_path , "install", "gnome", "cmg-gnome-thumbnail.schema")
		Popen(["gconftool-2",  "--makefile-uninstall-rule", cmg_gnome_thumbnail], stdout=open(os.devnull, "w"), stderr=PIPE).communicate()
	
		klik_gnome_protocol = os.path.join(klik_files_path , "install", "gnome", "klik-protocol.schema")
		Popen(["gconftool-2",  "--makefile-uninstall-rule", klik_gnome_protocol], stdout=open(os.devnull, "w"), stderr=PIPE).communicate()

if xdg.get_desktop_enviroment() == "KDE":
	print "TODO: KDE thumbnailer"
	print "TODO: KDE klik protocol"
	
print ""
print "Uninstall Complete"


