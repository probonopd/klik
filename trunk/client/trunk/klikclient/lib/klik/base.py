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
import hashlib
import stat

# klick libs
from klik.cmg import KlikCmg
from klik.fuse import KlikExecute
from klik.recipe import KlikRecipe
from klik.ticket import KlikTicket
from klik.create import KlikCreate
from klik.settings import KlikSettings
from klik.sandbox import KlikSandBox
from klik.exception import SafeException
from klik.exception import ExecuteException
from klik.utils.events import Events
from klik.utils.xdg import XdgUtils
from klik.utils.notify import Notify
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

		# Cleanup any junk in execute
		self.clean_up()

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
		# hashlib.md5(f1.read()).digest()
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
			h.getreply()
			print h.file.read()
			print "Feedback Sent"
		except Exception, e:
			print "base.post_feedback() - Error:", e

	def check_for_updated_recipe(self, recipe):
		try:
			new_recipe = self.get_recipe(recipe.source_uri)

			# Has the recipe changed
			old_md5_hash = hashlib.md5(recipe.original_xml).hexdigest()
			new_md5_hash = hashlib.md5(new_recipe.original_xml).hexdigest()

			if not old_md5_hash == new_md5_hash:
				# Should we also check application version numbers?
				# A recipe might change just to fix a run time error etc...
				return True, new_recipe
		except Exception, e:
			print "base.check_for_updated_recipe() - Error", e

		# If we cant get a recipe always return false
		return False, None

	def get_recipe(self, application_name):

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
			for header in ["klik2://", "klik2:", "klik://", "klik:"]:
				if application_name.startswith(header):
					application_name = application_name[len(header):]
					break
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
		recipe.print_details()

		return recipe

	def create_cmg(self, recipe_object ):
		return self.create.create( recipe_object, self.settings.destination )

	def create_jailed_cmg(self, cmg_path, jail_type="data"):
		sandbox = KlikSandBox(cmg_path)
		sandbox.create_jail( jail_type )
		return


	def open_application_folder(self, index=0):
		self.xdg.open(self.settings.application_folders[index])

	def is_valid_cmg(self, filename):
		return KlikCmg( self, filename ).is_valid_cmg()

	def is_valid_recipe(self, filename):
		mtype, entype = mimetypes.guess_type(filename)
		if mtype in [ "application/x-extension-cmg-recipe", "text/xml", "application/xml" ]:
			return True
		return filename.endswith(".recipe") or filename.endswith(".xml")

	def is_valid_ticket(self, filename):
		mtype, entype = mimetypes.guess_type(filename)
		if mtype in [ "text/x-application-ticket" ]:
			return True
		return filename.endswith(".ticket")

	def unpack_cmg(self, cmg_path, location):
		try:
			cmg_path = os.path.expanduser( cmg_path )
			cmg_path = os.path.expandvars( cmg_path )
			cmg_path = os.path.abspath( cmg_path )

			if not location:
				location = os.path.splitext(cmg_path)[0]
			print location

			location = os.path.expanduser( location )
			location = os.path.expandvars( location )
			location = os.path.abspath( location )

			if not os.path.exists(location):
				print "Extract to : " + location
				ex = KlikExecute(self)
				ex.extract_all(KlikCmg(self, cmg_path), location)
			else:
				print "Sorry %s already exists" % location
		except Exception, inst:
			if isinstance(inst, ExecuteException):
				print "Error occured : " + str(inst)
			else:
				traceback.print_exc()

	def pack_cmg(self, location, cmg_path):
		try:
			location = os.path.expanduser( location )
			location = os.path.expandvars( location )
			location = os.path.abspath( location )

			self.create.pack( location, cmg_path )
		except Exception, inst:
			if isinstance(inst, ExecuteException):
				print "Error occured : " + str(inst)
			else:
				traceback.print_exc()

	# Delete all temp files registered in self.temp_paths
	def clean_up(self):
		if not self.settings.debug:
			# delete all tmp files that exist
			for path in self.temp_paths:
				if os.path.isdir(path):
					shutil.rmtree(path.encode('ascii'), True)
				if os.path.isfile(path):
					os.remove(path)

			# delete all old mounts
			# is this a good idea... it can slow down startup speeds..
			#ex = KlikExecute(self)
			#mount_path = os.path.expanduser( os.path.join(self.settings.config_path, "execute"))
			#for root, dirs, files in os.walk( mount_path ):
			#	for dir in dirs:
			#		try:
			#			# Check if mnt exists and if it is in use
			#			mount_point = os.path.join(mount_path, dir, "mnt")
			#			if os.path.exists(mount_point):
			#				# is it in use
			#				if not ex.is_mount_in_use(mount_point):
			#					ex.umnount_cmg(mount_point)
			#					shutil.rmtree(os.path.join(mount_path, dir))
			#			else:
			#				shutil.rmtree(os.path.join(mount_path, dir))
			#		except:
			#			pass
			#	break # only do first level of folders

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
		rv += "\nDesktop environment: " + self.xdg.get_desktop_enviroment()
		rv += "\nTerminal detected: " + str(self.xdg.get_is_terminal())
		#rv += "\nPyGTK installed: " + str(self.xdg.get_is_gtk_installed())
		rv += "\nBase System Includes: \n\t- " + "\n\t- ".join(self.xdg.get_installed_libs())
		return rv

	def execute_cmg_shell(self, cmg_path):
		cmg = KlikCmg(self, cmg_path)
		return cmg.execute_shell()


	def download(self, apps):

		print ""
		print "System Information"
		print "=================="
		print  self.system_info()

		for app in set(apps):
			recipe = None
			if os.path.isfile(app):
				if self.is_valid_ticket( app ):
					ticket = KlikTicket( app )
					app = ticket.name
				elif self.is_valid_recipe( app ):
					recipe = KlikRecipe( app )
			if not recipe:
				recipe = self.get_recipe( app )
			self.create_cmg( recipe )
		return 1, ""

	def download_and_execute(self, app_name, args):

		recipe = None
		ticket = None

		# Make sure gpg key has been loaded
		self.insure_gpg_key_registered()

		if os.path.isfile(app_name):
			if self.is_valid_ticket( app_name ):
				ticket = KlikTicket( app_name )
				app_name = ticket.name
				recipe = self.get_recipe( app_name )
			elif self.is_valid_recipe( app_name ):
				recipe = KlikRecipe( app_name )
			else:
				print "Error: Invalid file type"
				return 0, ""



		print ""
		print "System Information"
		print "=================="
		print  self.system_info()

		app_name = app_name.lower()
		de = self.xdg.get_desktop_enviroment()

		# Insure destination path exists, or make it
		if not os.path.exists( self.settings.destination ):
			os.mkdir(self.settings.destination)

		# TODO: This (kde|gnome|term) thing should probably be implemented with subclassing
		if self.xdg.get_is_x_display() and self.xdg.get_is_gtk_installed(): #and (de in ["KDE", "GNOME", "XFCE"]):

			from klik.gtk.create import KlikGtkCore
			gtk = KlikGtkCore(self)
			if recipe:
				gtk.download_and_execute_recipe( recipe, args )
			else:
				gtk.download_and_execute_application( app_name, args )
			return 0, ""

		else:
			if not recipe:
				recipe = self.get_recipe( app_name )
			print ""
			result = raw_input("Are you sure you want to download this application (y/N)? ")

			if len(result) > 0 and result[0].lower() == "y":
				cmg = self.create_cmg( recipe )
				return cmg.execute(path, args) # FIXME
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
		recipe = KlikRecipe( recipe_path )
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


	def is_system_sane(self):
		valid_fuse_module = False
		valid_fuse_group = False
		valid_fuse_mode = False
		if os.path.exists("/dev/fuse"):

			# Module fuse is loaded
			pipe = subprocess.Popen(["lsmod"], stdout=subprocess.PIPE)
			for line in pipe.stdout:
				if line.startswith("fuse"):
					valid_fuse_module = True

			stbuf = os.stat("/dev/fuse")

			# If fuse device belongs to group fuse, the user is in the fuse group
			group =stbuf[stat.ST_GID]
			if group == 0:
				valid_fuse_group = True
			else:
				valid_fuse_group = True # TODO

			# All users can access the fuse device
			mode = stbuf[stat.ST_MODE]
			if (mode & stat.S_IROTH) and (mode & stat.S_IWOTH):
				valid_fuse_mode = True

		return valid_fuse_module, valid_fuse_group, valid_fuse_mode


	# Private methods
	def __download_progress(self, percentage, text):
		self.events.on_download_progress( percentage, text )
