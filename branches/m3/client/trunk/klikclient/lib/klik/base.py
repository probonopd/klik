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

import os
import platform
import urllib
import urllib2
import subprocess
import sys
import tempfile
import mimetypes
import shutil
import httplib
import urlparse
import md5

# klick libs
from klik.fuse import KlikExecute
from klik.recipe import KlikRecipe
from klik.create import KlikCreate
from klik.settings import KlikSettings
from klik.sandbox import KlikSandBox
from klik.exception import SafeException
from klik.exception import ExecuteException
from klik.utils.events import Events
from klik.utils.xdg import XdgUtils
from klik.utils.freedesktop import DesktopParser
from klik.utils.debian import MenuParser
from klik.utils.notify import Notify
import klik.utils.freedesktop
from klik.zeroinstallgpg.gpg import *

# Declare available events
class KlikBaseEvents(Events):
	__events__ = ('on_download_progress', 'on_download_complete', 'on_pack_complete','print_to_stdout',  )

class KlikBase (object):

	def __init__(self, sys_path=sys.path[0]):

		self.sys_path = sys_path

		self.events = KlikBaseEvents()
		self.create = KlikCreate(self)
		self.settings = KlikSettings()
		self.xdg = XdgUtils()
		self.notify = Notify()

		self.create.events.on_download_progress += self.__download_progress
		self.create.events.print_to_stdout += self.print_to_stdout
		self.current_progress_percentage = 0
		self.current_progress_text = ""
		self.temp_paths = []

		# Insure temp path exists
		if not os.path.exists( self.settings.temp_directory_path ):
			os.mkdir(self.settings.temp_directory_path)

	# Custom print method so we can capture stdout
	def print_to_stdout(self, obj, no_new_line=False):
		if no_new_line:
			sys.stdout.write( obj )
			sys.stdout.flush()
			self.events.print_to_stdout( obj )
		else:
			print obj
			self.events.print_to_stdout( obj + "\n")

	def post_feedback(self, recipe, workingyesno, errorreport, librarytrace, md5sum, comment):

		content = []

		content.append("package=" + urllib.quote( recipe.name )) #
		content.append("version=" + urllib.quote( recipe.version ))

		content.append("workingyesno=" + urllib.quote( str(workingyesno) )) #
		content.append("systeminformation=" + urllib.quote( self.system_info() )) #
		content.append("comment=" + urllib.quote( comment )) #

		#  f1 = file(os.path.join(path_1, os.listdir(path_1)[0]) ,'rb')
		# md5.new(f1.read()).digest()
		# f1.close()

		# only post error info on a failure
		if not workingyesno:
			content.append("errorreport=" + urllib.quote( errorreport ))
			content.append("librarytrace=" + urllib.quote( librarytrace ))
			content.append("recipe=" + urllib.quote( recipe.original_xml ))
			#content.append("md5sum=" + urllib.quote( librarytrace ))

		url_parts = urlparse.urlparse(self.settings.default_feedback_url)
		body = "&".join(content)
		h = httplib.HTTP(url_parts[1])
		h.putrequest('POST', url_parts[2])
		h.putheader('content-type', "application/x-www-form-urlencoded")
		h.putheader('content-length', str(len(body)))
		h.endheaders()

		try:
			h.send(body)
			#h.getreply()
			#print h.file.read()
		except:
			pass

	def check_for_updated_recipe(self, recipe):
		try:
			new_recipe = self.get_recipe(recipe.source_uri)
			
			# Has the recipe changed
			old_md5_hash = md5.new(recipe.original_xml).hexdigest()
			new_md5_hash = md5.new(new_recipe.original_xml).hexdigest()
			
			if not old_md5_hash == new_md5_hash:
				# Should we also check application version numbers?
				# A recipe might change just to fix a run time error etc...
				return True, new_recipe
		except:
			pass
		
		# If we cant get a recipe always return false
		return False, None

	def get_recipe(self, application_name ):

		# Get information about users system
		plat = platform.system() + " " + platform.machine()
		pyvers = "Python/"+ platform.python_version()

		useragent = "klik/%s (%s; %s; %s; %s;) %s (%s)" % (self.settings.version, self.xdg.get_is_terminal(), plat, self.xdg.get_lang(),  self.xdg.get_desktop_enviroment()+"+"+ "+".join(self.xdg.get_installed_libs()), pyvers, self.xdg.get_distribution())

		# Convert to lower case for comparing
		application_name = application_name.lower()

		# Download Recipe
		if application_name.startswith("http://"):
			url = application_name
		else:
			# Strip klik protocol
			if application_name.startswith("klik2://"):
				application_name = application_name[8:]
			elif application_name.startswith("klik2:"):
				application_name = application_name[6:]
				
			if application_name.startswith("klik://"):
				application_name = application_name[7:]
			elif application_name.startswith("klik:"):
				application_name = application_name[5:]

			url = self.settings.default_download_url + application_name

		print "Recipe URL: " + url
		try:
			request = urllib2.Request(url)
			request.add_header('User-Agent',useragent)
			opener = urllib2.build_opener()
			data = opener.open(request).read()
		except:
			raise SafeException("Error occurred while attempting to download recipe")

		# Load Recipe
		print ""
		print "Parsing recipe..."
		recipe = KlikRecipe()
		recipe.load_from_string( data )
		self.print_recipe(recipe)

		return recipe

	def print_recipe(self, recipe):
		print ""
		print "Application details"
		print "==================="
		print u"Name: %s" % recipe.name
		print u"Version: %s" % recipe.version

		trusted, text = recipe.is_trusted()
		if trusted == True:
			print "Trusted Contact : " +  text
		else:
			print "Trust Level : UNTRUSTED - This recipe has not been signed by someone you trust"
			
		print u"Summary: %s" % recipe.summary
		print u"Description: %s" % recipe.description
		print ""

	def load_recipe(self, recipe_path):
		recipe = KlikRecipe()
		recipe.load_from_file( recipe_path )
		return recipe

	def create_cmg(self, recipe_object ):
		return self.create.create( recipe_object, self.settings.destination )

	def execute_cmg(self, command, cmg_path, args, capture_error=False, force_prompt=False ):
		ex = KlikExecute(self)
		desktop_object = None
		
		cmg_version = self.is_valid_cmg(cmg_path)
		
		if cmg_version == 2:
		
			# CLI should add commands to line
			if force_prompt or command == None or command.strip() == "":

				# Need to check recipe command
				# Parse out command and parameters
				recipe = self.extract_recipe(cmg_path)
			
				parse_count = 0
				parse_args = len(args) == 0
				p = re.compile("(('[^']+')|(\"[^\"]+\")|([^\s]+))+")
				for r in re.finditer(p, recipe.command):
					if parse_count == 0:
						command = r.group(0)
					elif parse_args:
						args.append(r.group(0))
					parse_count = parse_count + 1

				# look at desktop files
				if force_prompt or command == None or command.strip() == "" or self.settings.always_show_app_chooser:

					desktop_objects = self.get_desktop_objects(cmg_path, False)
					if force_prompt or self.xdg.get_is_x_display() and len(desktop_objects) > 1:

						desktop_object = None

						# if self.xdg.get_desktop_enviroment() == "GNOME" and :
						if self.xdg.get_is_gtk_installed():
							# this dialouge will return the selected desktop object
							from klik.gtk.multiple import MultipleApplications
							ma = MultipleApplications(self)
							desktop_object = ma.load_items( desktop_objects )

						# QT ?

						if desktop_object != None:
							exec_array = desktop_object.get_exec_array()
							command = exec_array[0]
						else:
							return 1, "User Cancelled"
					
				

					else:
						if len(desktop_objects) > 0:
							desktop_object  = desktop_objects[0]
							exec_array = desktop_object.get_exec_array()
							command = exec_array[0]
					
		return ex.execute( command, cmg_path, args, capture_error, cmg_version )
		


	def check_for_missing_library(self, command, cmg_path):
		ex = KlikExecute(self)
		return ex.check_for_missing_library( command, cmg_path )

	def create_jailed_cmg(self, cmg_path, jail_type="data"):
		sandbox = KlikSandBox(cmg_path)
		sandbox.create_jail( jail_type )
		return 

	def extract_recipe(self, cmg_path):
		temp_path = tempfile.mktemp()
		self.extract_file(cmg_path, "/recipe.xml", temp_path)
		try:
			recipe = self.load_recipe( temp_path )
			os.remove(temp_path)
			return recipe
		except:
			# no recipe
			pass
			
		return None
	
	def extract_file(self, cmg_path, file_path_from, file_path_to):
		if self.is_klik1_cmg( cmg_path ):
			if file_path_from[0] == "/": file_path_from = file_path_from[1:]
			temp_path = tempfile.mkdtemp( '.extract.' + os.environ["USER"], self.settings.temp_directory_path )
			try:
				try:
					subprocess.Popen(["/opt/klik/bin/fusecram", "-n", "-p", cmg_path, temp_path, "-s"]).wait()
					shutil.copy( os.path.join(temp_path, file_path_from), file_path_to )
				except IOError:
					return False
			finally:
				subprocess.Popen(["fusermount", "-u", temp_path]).wait()
		else:
			subprocess.Popen(["cmginfo", "-f", cmg_path, "-e", file_path_from, "-o", file_path_to]).wait()
		return os.path.exists( file_path_to )

	def open_application_folder(self, index=0):
		self.xdg.open(self.settings.application_folders[index])

	def is_valid_cmg(self, path):
		if os.path.isfile( path ):
			mtype, entype = mimetypes.guess_type(path)
			if mtype in ["application/x-extension-cmg"]:
				return 2
			else:
				if self.is_klik2_cmg( path ):
					return 2
				if self.is_klik1_cmg( path ):
					return 1
		return 1

	def is_klik2_cmg(self, path):
		# Klik2 Compatability....
		f = open(path, "r")
		f.seek(22)     # Go to the 22nd byte in the file
		magic = str(f.read(28)).strip()
		test = magic.startswith( "# Compressed Application Ima" ) or magic.startswith( "# KLIK2 ISO" ) or magic.startswith( "Compressed Application Image" ) or magic.startswith( "KLIK2 ISO")
		return test
		
	def is_klik1_cmg(self, path):
		# Klik1 Compatability....
		f = open(path, "r")
		f.seek(16)     # Go to the 16th byte in the file
		test = ( str(f.read(16)).strip() == "Compressed ROMFS" )
		f.close()
		return test
		
	def is_valid_recipe(self, filename):
		mtype, entype = mimetypes.guess_type(filename)
		if mtype in [ "text/xml", "application/xml", "application/x-extension-cmg-recipe" ]:
			return True
		if mtype == None:
			return filename.endswith(".recipe") or filename.endswith(".xml") 
		return False 

	def find_sub_directories(self, path, cmg_path):
		directories = []
		p = subprocess.Popen(["cmginfo", "-f", cmg_path, "-l", path, "-v"], stdout=subprocess.PIPE)
		for line in p.stdout:
			line = line.strip()
			if line[0] == "d":
				directories.append( line[2:] )
		p.stdout.close()
		return directories


	# Return all the files in a given CMG that match pre and post text
	def find_files(self, cmg_path, pre_text=None, post_text=None, omit_extension=False):
		if omit_extension:
			# If post text contains a file extension, remove it
			if post_text:
				post_text = os.path.splitext( post_text )[0]

		files = []
		p = subprocess.Popen(["isoinfo", "-R", "-f", "-i", cmg_path], stdout=subprocess.PIPE)
		#p = subprocess.Popen(["cmginfo", cmg_path], stdout=subprocess.PIPE) - this is WAY to slow
		for file in p.stdout:
			file = file.strip()
			if post_text != None:
				if omit_extension:
					file_ends = file.endswith(post_text, 0, -4)
				else:
					file_ends = file.endswith(post_text)
			if (pre_text == None or file.startswith(pre_text)) and (post_text == None or file_ends):
				files.append( file )

		p.stdout.close()

		return files

	def scan_cmg_icons(self, cmg_path, do):
		# Returns a list of icons for a .desktop entry

		icon = do.get( "Icon" )

		if not icon:
			# No icon specifyed, fallback to CMG icon
			return ["/.DirIcon"]

		else:
			# Check if the icon belongs to the default theme
			theme_icon = None
			if icon[0] != "/" and self.xdg.get_is_gtk_installed():
				try:
					# never rely on gtk
					import gtk.utils
					theme_icon = gtk.utils.find_icon_from_theme( icon )
				except:
					pass

				if theme_icon:
					return [theme_icon]

			if not theme_icon:
				# If icon doesn't belong to a theme, search it in the CMG
				icon_files = self.find_files( cmg_path, "/usr/share/icons/", icon, omit_extension=True )
				if icon_files:
					return icon_files
				else:
					# Icon doesn't exist, fallback to CMG icon
					return ["/.DirIcon"]


	# Get desktop objects set to execute cmg
	def get_desktop_objects(self, cmg_path, fix_exec=True):

		# Search application .desktop files
		desktop_files = self.find_files(cmg_path, "/usr/share/applications/", ".desktop")
		
		desktop_objects = []
		for file in desktop_files:

			# Extract and parse the .desktop file
			temp_path = tempfile.mktemp()

			if not self.extract_file( cmg_path, file, temp_path ):
				print "\t!! Couldn't extract", file

			else:
				do = DesktopParser( temp_path )

				command = do.get("Exec")
				if do.get("Type") == "Application" and command != None:

					# Parse command and change it to point to CMG
					if fix_exec:
						do.set( "Exec", "klik exec \"" + cmg_path + "\" " + command )
					else:
						do.set( "Exec", command )
					do.set( "X-CMG", cmg_path )

					desktop_objects.append( do )

				os.remove( temp_path )

		# Attempt to generate desktop files from debian menu
		if len(desktop_objects) == 0:
			menu_files = self.find_files(cmg_path, "/usr/share/menu/")
			
			for menu_file in menu_files:

				# Extract and parse the .desktop file
				temp_path = tempfile.mktemp()

				if not self.extract_file( cmg_path, menu_file, temp_path ):
					print "\t!! Couldn't extract", menu_file

				else:
					for mo in MenuParser( temp_path ).menu_objects:
						do = DesktopParser()

						do.set("Type", "Application")
						if fix_exec:
							do.set("Exec", "klik exec \"" + cmg_path + "\" " + mo.command)
						else:
							do.set("Exec", mo.command)

						do.set( "X-CMG", cmg_path )
						do.set("Name", mo.title.capitalize())
						#do.set("Terminal", recipe.require_terminal)
						do.set("Icon", mo.icon)
						do.set("Categories", mo.freedesktop_category)

						desktop_objects.append( do )

		# Couldn't find any desktop file, build a basic one
		if len(desktop_objects) == 0:
		
			# With klik2 packages, extract information from recipe
			# With klik1 packages, make up information
			cmg_version = self.is_valid_cmg(cmg_path)
			
			recipe = None
			if cmg_version == 2:
				recipe = self.extract_recipe( cmg_path )
		
			do = DesktopParser()
			
			if fix_exec or (cmg_version == 1):
				do.set("Exec", "klik \"" + cmg_path + "\"") # dont need to not fix this one..
			else:
				if recipe.command:
					do.set("Exec", recipe.command)
				else:
					do.set("Exec", recipe.name)
		
			if recipe:
				name = recipe.name.capitalize()
				terminal = recipe.require_terminal
			else:
				name = os.path.splitext( os.path.split( cmg_path )[1] )[0].capitalize()
				terminal = False
			
			do.set("Name", name)
			do.set("Terminal", terminal)
			
			do.set("Type", "Application")
			do.set("Icon", "/.DirIcon")
			do.set("X-CMG", cmg_path)

			# Debtags / categories
			if recipe and recipe.debtags.has_key("use"):
				category = klik.utils.freedesktop.get_freedesktop_category_from_debtag( recipe.debtags["use"] )
				if category != None:
					do.set("Categories", category)

			desktop_objects.append( do )
		
		return desktop_objects

	def extract_icon(self, cmg_path, file_path_to, image_size):

		if self.extract_file(cmg_path, "/.DirIcon", file_path_to):

			# Check image size
			if image_size > -1:
				try:
					from klik.utils.images import Images
					img = Images()
					# NOTE : image_size is the thumbnail size NOT the icon size....
					img.check_image_size(file_path_to,  self.settings.desktop_icon_size)

				except Exception, text:
					pass
			return True
		return False


	def unpack_cmg(self, cmg_path, location):
		try:
			path = os.path.join(location, os.path.basename(cmg_path))
			if not os.path.exists(path):
				ex = KlikExecute(self)
				ex.extract_all(cmg_path, path)
			else:
				print "Sorry %s already exists" % path
		except Exception, inst:
			if isinstance(inst, ExecuteException):
				print "Error occured : " + str(inst)
			else:
				traceback.print_exc()

	def pack_cmg(self, location, cmg_path):
		try:
			self.create.pack( location, cmg_path )
		except Exception, inst:
			if isinstance(inst, ExecuteException):
				print "Error occured : " + str(inst)
			else:
				traceback.print_exc()

	# Delete all temp files registered in self.temp_paths
	def clean_up(self):
		if not self.settings.debug and os.getenv("KLIK_DEBUG") != "1":
			# delete all tmp files that exist
			for path in self.temp_paths:
				if os.path.isdir(path):
					shutil.rmtree(path.encode('ascii'), True)
				if os.path.isfile(path):
					os.remove(path)

	def insure_gpg_key_registered(self):
		# Currently probono's key
		finger_print = "91206CA327C8F18D56E928B6BAA5EC577BCE0539"
		sig = load_key( finger_print )

		# if base key not loaded load it now
		if sig.get_short_name() != "probono":
			print ""
			print "NO GPG KEY REGISTERED : Attempting to register GPG key..."
			key_file_path = os.path.join(self.sys_path , os.pardir, "share", "klik", "BAA5EC577BCE0539.gpg")
			key_file = open( key_file_path, "r")
			sig = import_key( key_file )
			key_file.close()
			print sig


	def system_info(self):
		rv = "klik version: " + self.settings.version
		rv += "\nProcessor: " + self.xdg.get_chipset_info()
		rv += "\nDistribution: " + self.xdg.get_distribution()
		rv += "\nPlatform: " + str(platform.platform())
		rv += "\nX server: " + str(self.xdg.get_is_x_display())
		rv += "\nPython version: " + platform.python_version()
		rv += "\nDesktop environment: " + self.xdg.get_desktop_enviroment()
		rv += "\nTerminal detected: " + str(self.xdg.get_is_terminal())
		#rv += "\nPyGTK installed: " + str(self.xdg.get_is_gtk_installed())
		rv += "\nBase System Includes: " + ", ".join(self.xdg.get_installed_libs())
		return rv

	def execute_cmg_shell(self, cmg_path):
		ex = KlikExecute(self)
		ex.execute_shell( cmg_path )

	def download_and_execute(self, app_name, args):

		recipe = None

		# Make sure gpg key has been loaded
		self.insure_gpg_key_registered()

		if os.path.isfile(app_name):

			# Check the mimetpye of the file....
			# TODO: register cmg mimetype with system so we can detect here
			mtype,entype = mimetypes.guess_type(app_name)

			# If file is not a recipie
			if self.is_valid_recipe(app_name):
				print("\n\n" + self.system_info())
				recipe = self.load_recipe( app_name )
			else:
				return self.execute_cmg(None, app_name, args)
		else:
			print ""
			print "System Information"
			print "=================="
			print  self.system_info()

		app_name = app_name.lower()
		de = self.xdg.get_desktop_enviroment()

		# Insure destination path exists, or make it
		if not os.path.exists( self.settings.destination ):
			os.mkdir(self.settings.destination)

		if self.xdg.get_is_x_display() == True and self.xdg.get_is_gtk_installed() == True: #and (de == "KDE" or de == "GNOME" or de == "XFCE"):

			from klik.gtk.create import KlikGtkCreate
			gtk = KlikGtkCreate(self)
			if recipe != None:
				gtk.download_and_execute_recipe( recipe, args )
			else:
				gtk.download_and_execute_application( app_name, args )
			return 0, ""

		else:
			if recipe == None:
				recipe = self.get_recipe( app_name )

			print ""
			result = raw_input("Are you sure you want to download this application (y/N)? ")

			if len(result) > 0 and result[0].lower() == "y":
				path = self.create_cmg( recipe )
				return self.execute_cmg(None, path, args)
			return 0, ""


	def merge(self, cmg_one, cmg_two, cmg_out):

		# NOTE : cmg 2 can not overwrite files from cmg 1...
		#		 this is probably wrong ;)

		build_path = tempfile.mkdtemp('.merge.' + os.environ["USER"] , self.settings.temp_directory_path)
		self.temp_paths.append( build_path )

		build_path_two = os.path.join(build_path, "two")
		self.unpack_cmg(cmg_two, build_path_two)

		# Rename first recipe
		recipe_path = os.path.join(build_path_two, "recipe.xml")
		recipe = self.load_recipe( recipe_path )
		if recipe.version != "":
			separator = "_"
		else:
			separator = ""
		recipe_rename_path = os.path.expanduser(u"%s/merged-%s%s%s.xml" % (build_path_two, recipe.name, separator, recipe.version))
		os.path.rename(recipe_path, recipe_rename_path)

		# Unpack second cmg
		build_path_one = os.path.join(build_path, "one")
		self.unpack_cmg(cmg_one, build_path_one)

		# Pack into new
		self.pack_cmg(cmg_out, build_path)

		# Cleanup temp files
		self.clean_up()

	# Private methods
	def __download_progress(self, percentage, text):
		self.events.on_download_progress( percentage, text )
