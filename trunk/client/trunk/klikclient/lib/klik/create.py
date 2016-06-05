#!/usr/bin/env python
# -*- coding: utf-8 -*-

#   klik2 
#   Copyright (C) 2008  Jason Taylor - killerkiwi2005@gmail.com
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

import urllib
import urllib2
import shutil
import os
from subprocess import *
import tempfile
import sys
import stat 
import hashlib

# klik libs
from klik.utils.events import Events
from klik.utils.freedesktop import DesktopParser
import klik.utils.scriptutil as su
from klik.recipe import *
from klik.exception import ExecuteException
from klik.cmg import KlikCmg

class KlikCreateEvents(Events):
	__events__ = ('on_download_progress', 'print_to_stdout',  )

class KlikCreate (object):

	def __init__(self, klik):
		self.klik = klik
		self.events = KlikCreateEvents()

		self.current_progress_text = ""
		self.silent_current_progress_text = ""
		self.current_fileno = 0
		self.total_files = 0
		self.recipe = None
		self.total_download_size = 0
		
	def __download_progress(self, count, blockSize, totalSize):

		# FIX :: some times downloads more than total (dropped bytes or whatever) which makes progress bar jump backwards !!
		total = blockSize * count
		if total > totalSize:
			total = totalSize
			blockSize = 0

		current_progress_percentage = (float(self.current_fileno - 1) / self.total_files) + (float(total) / totalSize / self.total_files) * 0.9
		
		
		if self.recipe.size != None and self.recipe.size != "":
		
			self.total_download_size = self.total_download_size + blockSize

			t = "%skB" % (self.total_download_size / 1000)
			if self.total_download_size > 1000000:
				t = "%.1fMB" % (float(self.total_download_size) / 1000000)

			print (len(self.current_progress_text)+2) * "\b",

			self.current_progress_text = "%s of %s bytes" % (t, self.recipe.size)
			print self.current_progress_text, 
			sys.stdout.flush()
		else:
			print (len(self.silent_current_progress_text)+2) * "\b",
			self.silent_current_progress_text = "%s of %s" % (total, totalSize)
			print self.silent_current_progress_text,
			sys.stdout.flush()
			
		self.events.on_download_progress(current_progress_percentage, self.current_progress_text)
	
	def create(self, recipe, location):
		
		self.recipe = recipe
		
		# Create the temp folder
		build_path = tempfile.mkdtemp('.build.' + urllib.quote(recipe.name) , self.klik.settings.temp_directory_path)
		# Take user out privacy issue with error post .. + '.' + os.environ["USER"] 
		
		cache_path = os.path.join( self.klik.settings.temp_directory_path, "cache" )

		self.klik.temp_paths.append( build_path )

		# Creation Paths
		root_directory = "%s/root" % (build_path)

		packages_directory = "%s/packages" % (build_path)
		
		os.mkdir(root_directory)
		os.mkdir(packages_directory)

		if recipe.version != "":
			separator = "_"
		else:
			separator =""

		app_path = os.path.expanduser(u"%s/%s%s%s.cmg" % (location, urllib.quote(recipe.name), separator, recipe.version))
		
		# Download all packages
		self.events.print_to_stdout( "" )
		self.total_files = len(recipe.packages)
		self.total_download_size = 0
		for package in recipe.packages:

			try:		
				# Check for url redirect
				self.events.print_to_stdout( "" )
				self.events.print_to_stdout( _("Requesting : %s") % package.href )
				try: 
					p = urllib2.urlopen( package.href )
					package.href = p.geturl()
					p.close()
					self.events.print_to_stdout( _("Downloading : %s") % package.href )
					
				except urllib2.HTTPError, e:
    					raise ExecuteException( _("Error code %s %s") % (e.code, package.href) )
				
				
				current_package = urllib.url2pathname( package.href )
				self.current_fileno += 1

				package_basename = os.path.basename(current_package)
				package_path = os.path.join(packages_directory, package_basename)
				package_cache_path = os.path.join(cache_path, package_basename)

				if (self.klik.settings.temp_cache_downloads or os.getenv("KLIK_USE_CACHE") == "1") and os.path.isfile(package_cache_path):
					shutil.copy(package_cache_path, package_path)
				else:
					#self.events.print_to_stdout( _("Downloading : %s") % current_package )
					urllib.urlretrieve(package.href, package_path, self.__download_progress)

					if self.klik.settings.temp_cache_downloads or os.getenv("KLIK_USE_CACHE") == "1":
						if os.path.isdir(cache_path) == False:
							os.mkdir(cache_path)
						shutil.copy(package_path, package_cache_path)
				
				# Check md5 hash if present
				if package.md5:
					self.events.print_to_stdout( _("\nChecking MD5 Hash [%s] - ") % package.md5, True)
					md5_file = file(package_path ,'rb')
					if package.md5.startswith("SHA256:"):
						file_hash = "SHA256:" + hashlib.sha256(md5_file.read()).hexdigest()
					else:
						file_hash = hashlib.md5(md5_file.read()).hexdigest()
					md5_file.close()
					
					if package.md5 != file_hash:
						self.events.print_to_stdout( _("Failed") )
						raise ExecuteException( _("Error occured while attempting to download, MD5 hash does NOT match file %s") % package.href )
					self.events.print_to_stdout( _("Success") )
			except Exception, e:
				raise ExecuteException( _("Error occured while attempting to download %s (Error details: %s)") % (package.href, e) )
				
				# Cleanup 
				self.klik.clean_up()
				
				# Exit now
				return None
		
		self.events.on_download_progress( -1, _("Extracting Ingredients") )
			
		# preflight
		self.__execute_accepted_commands(packages_directory, recipe.preflight)
		
		# Extract Files		
		self.__extract_packages( packages_directory, root_directory)

		# Delete archives (disk space could be an issue!)
		shutil.rmtree(packages_directory, True)
				
		# Add the recipe file to the CMG
		f = open(root_directory + "/recipe.xml","w+")
		f.write(recipe.original_xml)
		f.close()

		# NOTE: execute will test desktop files no need to do that in create

		# Get icon if recipe has not set one
		# its ALWAYS better to set one
		self.events.print_to_stdout( "" )
		icon_path = os.path.join(root_directory, ".DirIcon") 
		if recipe.icon == None:
			self.events.print_to_stdout( _("No image set scanning for icon......") )
			icon_list = su.ffind(root_directory, shellglobs=(
				# Check name
				'%s.svg'% recipe.name, \
				'%s.png'% recipe.name, \
				'%s.gif' % recipe.name, \
				'%s.jpg' % recipe.name, \
				'%s.xpm' % recipe.name, \
				'*%s.svg'% recipe.name, \
				'*%s.png'% recipe.name, \
				'*%s.gif' % recipe.name, \
				'*%s.jpg' % recipe.name, \
				'*%s.xpm' % recipe.name, \
				# Now check command
				'%s.svg'% recipe.command, \
				'%s.png'% recipe.command, \
				'%s.gif' % recipe.command, \
				'%s.jpg' % recipe.command, \
				'%s.xpm' % recipe.command, \
				'*%s.svg'% recipe.command, \
				'*%s.png'% recipe.command, \
				'*%s.gif' % recipe.command, \
				'*%s.jpg' % recipe.command, \
				'*%s.xpm' % recipe.command, \
				
				'icon.xpm', \
				'*pixmaps*.*', ))
				#TODO: could also use command instead of name if this is failing a lot
				# ICONFIND=`find -name $APPNAME.png -or -name *$APPNAME.xpm -or -name *$APPNAME.gif -or -name *$APPNAME.jpg -or -ipath *pixmaps*.* 2>/dev/null | head -n 1` && \
			if len(icon_list) > 0:
				self.events.print_to_stdout( _("Icon Found : %s") % icon_list[0] )
				shutil.copy(icon_list[0], icon_path )
		else:
			
			if recipe.icon[0] == "/" or recipe.icon.startswith("file:///"):
				
				relative_path = os.path.join(root_directory, recipe.icon[1:].replace("file://", "")) 
				self.events.print_to_stdout( relative_path )
				if os.path.isfile( relative_path):
					shutil.copy(relative_path, icon_path)
			else:
				# maybe a url then ?
				try:
					urllib.urlretrieve(recipe.icon, icon_path)
				except ExecuteException, text:
					pass

		# postflight
		self.__execute_accepted_commands(root_directory, recipe.postflight)

		return KlikCmg(self.klik, self.pack(root_directory, app_path, build_path=build_path))

	
	def pack(self, root_directory, app_path, build_path=None):
		
		recipe_path = os.path.join(root_directory, "recipe.xml")
		recipe = None
		cmg_path = None
		
		# Load the recipe file	
		if  os.path.isfile( recipe_path ):
			recipe = KlikRecipe( recipe_path )
		else:
			raise ExecuteException( _("No recipe file found") )
			return

		# create a new build path if we dont have one yet
		if build_path == None:
			build_path = tempfile.mkdtemp('.build.' + urllib.quote(recipe.name) + '.' + os.environ["USER"] , self.klik.settings.temp_directory_path)

		cmg_path = os.path.join(build_path, urllib.quote(recipe.name) + "_" + recipe.version + ".cmg")

		self.events.on_download_progress( -1, _("Compressing Application Image...") )

		# Compressing
		# TODO: use same syntax as below
		zf_directory = os.path.join( build_path, "zftree")
		self.events.print_to_stdout( "" )
		self.events.print_to_stdout( _("Compressing...") )
		try:
			p = Popen(["mkzftree", "-V", "2", root_directory, zf_directory], stdout=PIPE, stderr=STDOUT)

			for line in p.stdout:
				self.events.print_to_stdout( line, True )
			p.stdout.close()
			p.wait()
		except:
			sys.exit(1)

		self.events.on_download_progress( -1, _("Generating Application Image") )
		
		# Build CMG / ISO
		self.events.print_to_stdout( "" )
		self.events.print_to_stdout( _("Generating ISO... ") )
		try:
			command = ["genisoimage", "-zvUR",
					"-input-charset", "utf-8",
					"-l",
					"-D",
					"-p", "http://klik.atekon.de/",
					"-V", recipe.name,
					"-volset" ,"Compressed Application Image",
					"-publisher", "Klik2 Client",
					"-no-pad",
					"-o", cmg_path, zf_directory]
					
			p = Popen(command, stdout=PIPE, stderr=STDOUT)
			for line in p.stdout:
				self.events.print_to_stdout( line, True )
			p.stdout.close()
			p.wait()
			
			result = p.returncode
			
		except:
			ExecuteException( _("genisoimage failed. Is it installed?") ) 

		# Embed makeself-like minirun code into leading 000's of the ISO
		self.events.print_to_stdout( "" )
		self.events.print_to_stdout( _("Embedding minirun...") )
		minirun = \
"""#!/usr/bin/env klik
# Compressed Application Image 0.1
$(which klik) run "$0" "$@" && exit 
echo "Please install klik. Runtime-less execution is not implemented yet. (TODO)"
exit
## END OF MINIRUN
"""
		fff = file(cmg_path, 'r+')
		fff.write(minirun)
		fff.close()
		
		# Set executable flag
		p = Popen(["chmod", "755", cmg_path], stdout=PIPE, stderr=STDOUT)
		for line in p.stdout:
			self.events.print_to_stdout( line, True )
		p.stdout.close()
		p.wait()
		
		# Copy to desktop
		if os.path.isfile(app_path):
			# Files exists create unique name instead
			basename  = os.path.basename(app_path)
			basepath = os.path.dirname(app_path)
			if basepath=="":
				basepath = os.getcwd()
			
			# find number of files already present
			number = 1 #len(su.ffind(basepath, shellglobs=("*%s", basename ))) # over kill this will count every file below ~/Desktop
			unique_path = app_path
			
			while os.path.isfile(unique_path):
				unique_path = "%s/copy (%d) of %s" % (basepath, number, basename)
				number = number + 1
			
			app_path = unique_path
		
		self.events.print_to_stdout( "" )
		self.events.print_to_stdout( _("Move File : %s ") % app_path )
			
		shutil.move(cmg_path, app_path)
		
		# Cleanup 
		self.klik.clean_up()
		
		self.events.print_to_stdout( "" )
		self.events.print_to_stdout( _("CMG creation complete") )
		return app_path
	
	
	def __extract_packages(self, package_path, root_path):
		
		self.events.print_to_stdout( "" )
		self.events.print_to_stdout( _("Unpack Packages : ") )
		# Tools required - tar, bunzip2, unzip, ar, zcat, rpm2cpio
		# Command calls should be safe only our generated path names are passed in

		# change the working folder
		os.chdir( root_path )
	
		# Zip archives
		self.__execute_extract_command("UNP=$(find %s/*.tgz 2>/dev/null) && for UN in $UNP; do tar zxvf $UN ; done" % (package_path))
		self.__execute_extract_command("UNP=$(find %s/*.tar.gz 2>/dev/null) && for UN in $UNP; do tar zxvf $UN ; done" % (package_path))
		self.__execute_extract_command("UNP=$(find %s/*.tbz 2>/dev/null) && for UN in $UNP; do bunzip2 -c $UN | tar -xvf - ; done" % (package_path))
		self.__execute_extract_command("UNP=$(find %s/*.tar.bz2 2>/dev/null) && for UN in $UNP; do bunzip2 -c $UN | tar -xvf - ; done" % (package_path))
		self.__execute_extract_command("UNP=$(find %s/*.tbz2 2>/dev/null) && for UN in $UNP; do bunzip2 -c $UN | tar -xvf - ; done" % (package_path))
		self.__execute_extract_command("UNP=$(find %s/*.zip 2>/dev/null) && for UN in $UNP; do unzip $UN ; done" % (package_path))
		
		# Debain packages
		self.__execute_extract_command("UNP=$(find %s/*.deb 2>/dev/null) && for UN in $UNP; do ar p $UN data.tar.gz | zcat | tar xv ; done" % (package_path))	
		
		# RPM packages
		# TODO: we need a better rpm extraction..
		# http://www.rpm.org/max-rpm/s1-rpm-file-format-rpm-file-format.html
		self.__execute_extract_command("UNP=$(find %s/*.rpm 2>/dev/null) && for UN in $UNP; do rpm2cpio < $UN | cpio -i -d --verbose ; done" % (package_path))
		
	def __execute_extract_command(self, command):
		p = Popen(command, stdout=PIPE, shell=True, stderr=STDOUT)
		for line in p.stdout:
			self.events.print_to_stdout( line, True )
		p.stdout.close()
		p.wait()

	def __execute_accepted_commands(self, basepath, action_collection):

		for action in action_collection:
			os.chdir( basepath )
			
			if isinstance(action, KlikTustredActionMove):	
				self.events.print_to_stdout( _("\nMoving File :") )
				self.events.print_to_stdout( _("from - %s") % action.source )
				self.events.print_to_stdout( _("to   - %s") % action.destination )
				mv = action.get_command(basepath)
				if mv == None:
					raise ExecuteException("Move command attempted an illegal path\n\nsource : %s\ndestination : %s" % (action.source, action.destination))
				call( mv )
			
			if isinstance(action, KlikTustredActionDelete):
				self.events.print_to_stdout( _("\nDeleting File :") )
				self.events.print_to_stdout( _("file - %s") % action.path )
				rm = action.get_command(basepath)
				if rm == None:
					raise ExecuteException("Delete command attempted an illegal path\n\npath : %s" % (action.path))
				call(rm)	
				
			if isinstance(action, KlikTustredActionSymLink):
				self.events.print_to_stdout( _("\nCreating Symlink :") )
				#self.events.print_to_stdout( _("from - %s") % action.source ) - this is causing a segfault ?
				#self.events.print_to_stdout( _("to   - %s") % action.destination ) - this is causing a segfault ?
				sl = action.get_command(basepath)
				if sl == None:
					raise ExecuteException("Symlink command attempted an illegal path\n\nsource : %s\ndestination : %s" % (action.source, action.destination))
				call(sl)
			
			if isinstance(action, KlikTustredActionUnzip):	
				self.events.print_to_stdout( _("\nUnzipping Archive :") )
				self.events.print_to_stdout( _("file - %s") % action.fileName )
				uz = action.get_command( basepath )
				if uz == None:
					raise ExecuteException("Unzip command attempted an illegal path\n\nsource : %s\ndestination : %s", (action.fileName))
				call(uz)	
				os.remove(action.fileName)
				
			if isinstance(action, KlikScript):	
				script = action.script
				if script == None:
					raise ExecuteException( _("Error with script : %s"), (script) )
				self.events.print_to_stdout(   "" )
				self.events.print_to_stdout(   "==================================================="  )
				self.events.print_to_stdout( _("WARNING :: THE FOLLOWING SCRIPT WILL BE EXECUTED : ") )
				self.events.print_to_stdout(   "==================================================="  )
				self.events.print_to_stdout(   script )
				self.events.print_to_stdout(   "==================================================="  )
				self.events.print_to_stdout( _("Executing...") )
				os.system(script)
			
