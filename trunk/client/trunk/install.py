#!/usr/bin/env python
import os
import sys
import shutil
import stat
import pwd
import os.path
import glob
import re
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


def check_command_exists( command ):
	path = Popen(["which", command], stdout=PIPE, stderr=open(os.devnull, "w")).communicate()[0].strip()

	if path != "":
		print " - %s: found at %s " % (command, path)
	else:
		print " - %s: NOT FOUND" % (command)
		print ""
		print "Install FAILED"
		sys.exit(1)

def enable_private_copy_if_missing( command ):
	# enable private copy if generic is not found
	path = Popen(["which", command], stdout=PIPE, stderr=open(os.devnull, "w")).communicate()[0].strip()
	if path == '':
		print ""
		os.system(" ".join(["chmod", "755",  os.path.join(klik_install_path, "bin/" + command)]))
		print " - %s: found at %s " % (command, (klik_install_path, "bin/" + command))
	print " - %s: found at %s " % (command, path)


#Check for all required commands
print "Checking for required commands ..."
check_command_exists("gpg")
check_command_exists("isoinfo")
check_command_exists("rpm2cpio")
enable_private_copy_if_missing("ar")
enable_private_copy_if_missing("genisoimage")
enable_private_copy_if_missing("mkzftree")


# PYTHON CLIENT
print ""
print "Installing python client ..."
os.system(" ".join(["cp -r", os.path.join(klik_files_path, "klikclient/bin"), os.path.join(klik_install_path)]))
os.system(" ".join(["cp -r", os.path.join(klik_files_path, "klikclient/lib"), os.path.join(klik_install_path)]))
os.system(" ".join(["cp -r", os.path.join(klik_files_path, "klikclient/share"), os.path.join(klik_install_path)]))

print ""
print "Creating symlinks ..."
for bin in ["klik", "cmginfo", "klikd"]:
	print " - /usr/local/bin/"+bin
	os.system(" ".join(["ln -sf", klik_install_path+"/bin/"+bin, "/usr/local/bin/"+bin]))

# EXECUTABLES
print ""
print "Installing executables..."
path = os.path.join(klik_install_path, "bin/")
if not os.path.exists(path):
	os.makedirs(path)
os.system(" ".join(["cp", os.path.join(klik_files_path, os.path.pardir, "binaryparts/bin") + "/*", os.path.join(klik_install_path, "bin")]))
call(["chmod", "755",  os.path.join(klik_install_path, "bin")])
shutil.copy(os.path.join(klik_files_path, "klikclient/bin/klik"), os.path.join(klik_install_path, "bin"))

# required klik binaries
os.system(" ".join(["chmod", "755",  os.path.join(klik_install_path, "bin/klik")]))
os.system(" ".join(["chmod", "755",  os.path.join(klik_install_path, "bin/fusioniso")]))
os.system(" ".join(["chmod", "755",  os.path.join(klik_install_path, "bin/cmginfo")]))
os.system(" ".join(["chmod", "755",  os.path.join(klik_install_path, "bin/install_kde_user.sh")]))

# BINARY LIBS
print ""
print "Installing binary libraries ..."
os.system(" ".join(["cp", os.path.join(klik_files_path, os.path.pardir, "binaryparts/lib/klik") + "/*", os.path.join(klik_install_path, "lib/klik")]))

# Alternative glibc loader
print ""
print "Installing alternative glibc loader..."
glibc_version_text = Popen(["ldd", "--version"], stdout=PIPE, stderr=open(os.devnull, "w")).communicate()[0].strip()
glibc_version = "ld-" + re.search("\d+(\.\d+)+", glibc_version_text).group() + ".so"
ld = os.path.join(klik_install_path, "lib/klik",  glibc_version )
selected_so = os.path.join(klik_files_path, os.path.pardir, "binaryparts/lib/glibc", glibc_version)
if os.path.exists(selected_so):
	call(["cp", os.path.join(klik_files_path, os.path.pardir, "binaryparts/lib/glibc", glibc_version), ld])
	call(["ln", "-s",  ld, "/lib/ld-klik2.so.2"])
	call(["ln", "-s",  ld, "/lib/ld-lsk.so.1"])
	call(["ln", "-s",  ld, "/lib/ld-lsk.so.2"])
	call(["ln", "-s",  ld, "/lib/ld-lsk.so.3"])
else:
	print "ERROR: no binary for your ldd version "
	print glibc_version_text

# LANGUAGE FILES
print ""
print "Installing language files ..."
po_dir = os.path.join("klikclient", "po")
locale_dir = os.path.join(po_dir, "locale")
if os.path.isdir(locale_dir):
	os.system("cp -a %s /usr/share/" % locale_dir)
else:
	print " - Locales not generated, go to %s and `make`" % po_dir

# If bash_completion is installed, install completion file
bash_completion_dir = os.path.join( "/etc", "bash_completion.d" )
if os.path.isdir(bash_completion_dir):
	os.system(" ".join(["cp", os.path.join(klik_files_path , "install", "klik.completion"), os.path.join(bash_completion_dir, "klik") ]))

# Check whether we can use the system-provided fusermount
# Else, set permissions for our private fusermount
print ""
print "Checking for fusermount ..."
systemfusermountpath=Popen(["which", "fusermount"], stdout=PIPE, stderr=open(os.devnull, "w")).communicate()[0].strip()
privatefusermountpath = os.path.join(klik_install_path, "bin/fusermount")

if systemfusermountpath != '':
    print " - System fusermount exists"
    # We only care weather we can execute this
    st = os.stat(systemfusermountpath)
    mode = st[stat.ST_MODE]
    # http://docs.python.org/lib/os-file-dir.html
    if mode & stat.S_IXOTH:
        print " - SUCCESS : Using system fusermount"
        os.system("chmod a-x %s" %(privatefusermountpath,)) # python's chmod screws setuid bit
    else:
        print " - System fusermount cannot be used due to wrong permissions, attempting to use private copy"
        systemfusermountpath = ""

if systemfusermountpath == "":
    print " - System fusermount cannot be found, attempting to use private copy"
    print " - Setting private fusermount to be executable"
    try:
        os.chown(privatefusermountpath,0,0)
        os.system("chmod 4755 %s" %(privatefusermountpath,)) # python's chmod screws setuid bit
        print " - SUCCESS : Using private fusermount"
    except:
        print " - FAILED : Cannot find a fusermount that can be used"
        sys.exit(1)



# Hack so we can get svn number in error reporting
svnversion_path=Popen(["which", "svnversion"], stdout=PIPE, stderr=open(os.devnull, "w")).communicate()[0].strip()
if svnversion_path != "":
	svnversion=Popen(["svnversion", klik_files_path], stdout=PIPE, stderr=open(os.devnull, "w")).communicate()[0].strip()
	f = open( os.path.join(klik_install_path, "share/klik/svnversion"),"w")
	f.write( svnversion )
	print ""
	print "SVN version: " + svnversion
	f.close()

# ------------------------------------------------------------------
# AFTER THIS POINT YOU CAN NOT RELY ON BEING ROOT !!!!!!
# ------------------------------------------------------------------

# WORKAROUND : KDE live cds sometimes fail, the work around is to use 'user' mode instead of system
if xdg.get_desktop_enviroment() == "KDE":

	# Grab the last path which is the one XDG attempts to use
	path = Popen(["kde-config", "--path", "mime"], stdout=PIPE, stderr=open(os.devnull, "w")).communicate()[0].strip().split(":").pop()

	# check path is writable if it isn't switch to user mode
	if not os.access(path, os.W_OK):

		os.getenv("GNOME_DESKTOP_SESSION_ID")
		try:
			# Set uid back to user instead of root / sudo, this will make xdg work (hopefully)
			# will sudo_user work on other distros?
			if os.getenv['SUDO_USER'] != None: # kubuntu
				print ""
				print "KDE LIVE CD DETECTED : install path is not writeable switching to user mode"
				os.seteuid(pwd.getpwnam(os.environ['SUDO_USER'])[2])

			elif os.getenv['USER'] != None: # mandriva
				print ""
				print "KDE LIVE CD DETECTED : install path is not writeable switching to user mode"
				os.seteuid(pwd.getpwnam(os.environ['USER'])[2])
		except:
			# this failed ... we can live with it you just dont get desktop integration
			pass

# CMG MIMETYPE
print ""
print "Register file type (.cmg) with system ..."
cmg_mimetype = os.path.join(klik_files_path , "install", "cmg-mimetype.xml")
xdg.mime_install( cmg_mimetype )

#Default icon for mimetype
print ""
print "Register file type icon (.cmg) with system ..."
cmg_icon = os.path.join(klik_files_path , "install", "application-x-application-cmg.png")
xdg.mime_icon_install( "48", cmg_icon, "application-x-application-cmg")

# RECIPIE MIMETYPE
cmg_mimetype = os.path.join(klik_files_path , "install", "recipe-mimetype.xml")
xdg.mime_install( cmg_mimetype )

# APPLICATION MENU / FILE ASSOCIATION
print ""
print "Install application association for (.cmg) ..."
klik_desktop = os.path.join(klik_files_path , "install", "klik.desktop")
xdg.desktop_install( klik_desktop )

# THUMBNAILER
if xdg.get_desktop_enviroment() == "GNOME":

	# http://www.gnome.org/projects/gconf/
	# Register the thumbnailer and protocol
	print ""
	print "Settings GCONF keys"

	# Get gconf default target
	gconf_default_source=Popen(["gconftool-2", "--get-default-source"], stdout=PIPE, stderr=open(os.devnull, "w")).communicate()[0].strip()	

	config_sources = []
	config_sources.append( gconf_default_source )		# System defaults
	config_sources.append( "xml:readwrite:" + os.environ["HOME"] + "/.gconf" )	# Current User (avoid needing login/out))
	for config_source in config_sources:
		# Set source
		os.environ["GCONF_CONFIG_SOURCE"] = config_source

		# Register the thumbnailer
		cmg_gnome_thumbnail = os.path.join(klik_files_path , "install", "gnome", "cmg-gnome-thumbnail.schema")
		Popen(["gconftool-2",  "--makefile-install-rule", cmg_gnome_thumbnail], stdout=open(os.devnull, "w"), stderr=PIPE).communicate()
		# register klik protocol
		klik_gnome_protocol = os.path.join(klik_files_path , "install", "gnome", "klik-protocol.schema")
		Popen(["gconftool-2",  "--makefile-install-rule", klik_gnome_protocol], stdout=open(os.devnull, "w"), stderr=PIPE).communicate()



	# try install nautilus property page
	# requires python-nautilus
	# TODO: this path is good for ubuntu, what about fedora and suse?
	if os.path.exists("/usr/lib/nautilus/extensions-1.0/python"):
		print "[ optional ] nautilus property page installed"
		shutil.copy(os.path.join(klik_files_path, "klikclient/lib/nautilus/extensions-1.0/python/cmg-nautilus-property-page.py"), "/usr/lib/nautilus/extensions-1.0/python/cmg-nautilus-property-page.py")
	else:
		print "[ optional ] nautilus property page could not be installed, requires python-nautilus"
		
	# Force all gconf instances to restart
	os.system("killall gconfd-2")
	#os.system("nautilus -q && nautilus")
	
if xdg.get_desktop_enviroment() == "KDE":

	print "KDE klik protocol"
	try:
		# Attempt system wide install		
		kde_home = Popen(["kde-config", "--prefix"], stdout=PIPE, stderr=open(os.devnull, "w")).communicate()[0].strip()
		shutil.copy(os.path.join(klik_files_path, "install", "kde", "klik.protocol"), os.path.join(kde_home, "share/services/klik.protocol"))
		shutil.copy(os.path.join(klik_files_path, "install", "kde", "klik.desktop"), os.path.join(kde_home, "share/apps/konqueror/servicemenus/klik.desktop"))
	except:
		# This is needed for Kubuntu Live CD
		# Attempt user install
		kde_home = Popen(["kde-config", "--localprefix"], stdout=PIPE, stderr=open(os.devnull, "w")).communicate()[0].strip()
		shutil.copy(os.path.join(klik_files_path, "install", "kde", "klik.protocol"), os.path.join(kde_home, "share/services/klik.protocol"))
	
	print "KDE thumbnailer and kfile integration"
        os.system("./install_kde.sh %s" % klik_install_path);

# Cleanup
# If the current directory has a .svn we can assume that there is .svn garbage
# We remove it now because in uninstall.py, shutil.rmtree doesn't remove hidden directories
if klik_install_path and os.path.exists(".svn"):
	print "Removing clutter ..."
	os.system("find %s -depth -name .svn -exec echo -e ' - {}' \;" % klik_install_path )
	print
	print " - !! This is a temporal security measure, this command shouldn't break nothing, but just in case"
	response = raw_input (" - Remove directories? [y/N] ")
	if response.lower() == "y":
		os.system("find %s -depth -name .svn -exec rm -rf '{}' \;" % klik_install_path )
		print " - Clutter removed"

print ""
print "Install Complete"
print ""
print "Please report errors to http://code.google.com/p/klikclient/issues/list"

