#!/usr/bin/env python
import os
import sys
import shutil
import stat
import glob
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

xdg = XdgUtils( os.path.join(sys.path[0], "klikclient", "lib", "klik", "utils", "xdg-utils") )

# Check for root
if xdg.is_root() == False:
	print ("You must be root in order to run "+sys.argv[0]+", exiting")
	sys.exit(1)

def remove_tree( path ):
	try:
		shutil.rmtree( path )
		os.removedirs( os.path.dirname( path ) ) # Remove directory if we leave it empty
		return True
	except OSError:
		return False
	except Exception, e:
		print "remove_tree():", e
		return False
def remove_file( path ):
	try:
		os.remove( path )
		os.removedirs( os.path.dirname( path ) ) # Remove directory if we leave it empty
		return True
	except OSError:
		return False
	except Exception, e:
		print "remove_file():", e
		return False

# Remove sub folders
print "Removing klik sub folders ..."
for path in ["share/klik", "lib/klik", "lib/nautilus/extensions-1.0"]:
	remove_tree( os.path.join( klik_install_path, path ) )
for path in ["share/klik", "lib/klik"]:
	remove_tree( os.path.join( klik_install_path, path ) )

# Remove Executables
print "Removing executables ..."
binaries_path = os.path.join(os.pardir, "binaryparts/bin")
for bin in os.listdir( binaries_path ) + ["klik"]:
	remove_file( os.path.join(klik_install_path, "bin", bin) )

# Remove Libraries
remove_file( os.path.join(klik_install_path, "lib", "libfakechroot.so") )

# If bash_completion is installed, uninstall completion file
remove_file( os.path.join( "/etc", "bash_completion.d", "klik" ) )

# LANGUAGE FILES
print "Removing language files ..."
locale_path = os.path.join("/usr", "share", "locale")
locales = glob.glob(locale_path + "/*")
for lang in locales:
	remove_file( os.path.join(lang, "LC_MESSAGES", "klikclient.mo") )

# CMG MIMETYPE
print "Removing file type (.cmg) from system ..."
cmg_mimetype = os.path.join(klik_files_path , "install", "cmg-mimetype.xml")
xdg.mime_uninstall( cmg_mimetype )

#Default icon for mimetype
print "Removing file type icon (.cmg) from system ..."
cmg_icon = os.path.join(klik_files_path , "install", "application-x-application-cmg.png")
xdg.mime_icon_uninstall( "48", cmg_icon, "application-x-application-cmg")

# RECIPIE MIMETYPE
#recipe_mimetype = os.path.join(sys.path[0] , "install", "gnome", "cmg-mimetype.xml")

# APPLICATION MENU / FILE ASSOCIATION
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


