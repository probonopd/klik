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
import subprocess
import sys
import tempfile
import mimetypes
import urllib
import shutil
import re

import klik.utils.images
import klik.utils.freedesktop
from klik.fuse import KlikExecute
from klik.utils.freedesktop import DesktopParser
from klik.utils.debian import MenuParser
from klik.sandbox import KlikSandBox
from klik.recipe import KlikRecipe

class KlikCmg(object):


	def __init__(self, klik, path):

		# Make sure path is absolute
		path = os.path.expanduser( path )
		path = os.path.expandvars( path )
		path = os.path.abspath( path )

		# private variables
		self.__recipe = None
		self.__cmg_version = -1

		# Public variables
		self.desktop_files = []
		self.desktop_files_fixed = []
		self.icon = None
		self.path = path
		self.sandbox = KlikSandBox(path)
		self.mount_point_path = None
		self.last_command = None
		self.klik = klik

	def exists(self):
		return os.path.exists(self.path)

	def find_sub_directories(self, path):
		directories = []
		p = subprocess.Popen(["cmginfo", "-f", self.path, "-l", path, "-v"], stdout=subprocess.PIPE)
		for line in p.stdout:
			line = line.strip()
			if line[0] == "d":
				directories.append( line[2:] )
		p.stdout.close()
		return directories

	def __get_command(self, args, force_prompt):

		command = None

		if self.cmg_version == 2:

			desktop_object = None

			# Need to check recipe command
			# Parse out command and parameters
			parse_count = 0
			parse_args = len(args) == 0
			p = re.compile("(('[^']+')|(\"[^\"]+\")|([^\s]+))+")
			for r in re.finditer(p, self.get_recipe().command):
				if parse_count == 0:
					command = r.group(0)
				elif parse_args:
					args.append(r.group(0))
				parse_count = parse_count + 1

			# look at desktop files
			if force_prompt or not command or self.klik.settings.always_show_app_chooser:

				desktop_objects = self.get_desktop_objects(False)
				if force_prompt or self.klik.xdg.get_is_x_display() and len(desktop_objects) > 1:

					desktop_object = None

					# if self.klik.xdg.get_desktop_enviroment() == "GNOME" and :
					if self.klik.xdg.get_is_gtk_installed():

						# this dialouge will return the selected desktop object
						from klik.gtk.multiple import MultipleApplications
						ma = MultipleApplications(self.klik)

						desktop_object = ma.load_items( desktop_objects )

					# QT ?
					if desktop_object != None:
						exec_array = desktop_object.get_exec_array()
						command = exec_array[0]
					else:
						return None

				else:
					if len(desktop_objects) > 0:
						desktop_object  = desktop_objects[0]
						exec_array = desktop_object.get_exec_array()
						command = exec_array[0]
		else:
			# klik 1
			command = "/wrapper"

		return command

	def execute(self, args, command=None, capture_error=False, force_prompt=False ):

		# CLI should add commands to line
		if force_prompt or not command:
			command = self.__get_command(args, force_prompt)
		if command:
			self.last_command = command
			ex = KlikExecute(self.klik)
			return ex.execute( self, command, args, capture_error )
		return 0, "Cancelled"

	def execute_shell(self):
		ex = KlikExecute(self.klik)
		command = self.__get_command([None], False)
		if command:
			return ex.execute_shell(self, command)
		return 0, "Cancelled"

	def get_recipe(self):
                if self.__cmg_version == 1:
			print "cmg.py:get_recipe() FIXME: Avoid getting the recipe of a klik1 package"
                        sys.exit(1)
		if not self.__recipe:
			temp_path = self.extract_file("/recipe.xml")
			try:
				self.__recipe = KlikRecipe( temp_path )
				os.remove(temp_path)
			except Exception, e:
				print "cmg.get_recipe() - Error:", e
		return self.__recipe

	def open_pipe(self, file_path_from):

		if self.cmg_version == 1:
			return open( self.extract_file(file_path_from), "r")
		else:
			# Klik2 packages: if file_path_to != None, extract file; otherwise write file to a pipe
			p = subprocess.Popen(["cmginfo", "-f", self.path, "-e", file_path_from], stdout=subprocess.PIPE)
			return p.stdout

	def extract_file(self, file_path_from, file_path_to=None):
		# If you do not pass in a file path to a temp file will be created that you should delete

		if not file_path_to:
			file_path_to = tempfile.mktemp()

		if self.cmg_version == 1:
			# ALWAYS use KlikExecute!!!!
			ex = KlikExecute(self.klik)
			ex.extract_file( self, file_path_from, file_path_to)
		else:
			# Klik2 packages: if file_path_to != None, extract file; otherwise write file to a pipe
			if file_path_to:
				subprocess.Popen(["cmginfo", "-f", self.path, "-e", file_path_from, "-o", file_path_to]).wait()

		if os.path.exists( file_path_to ):
			return file_path_to

		return None


	def is_valid_cmg(self):

		if self.__cmg_version == -1:
			if os.path.isfile( self.path ):
				# Try system mimetype first
				mtype, entype = mimetypes.guess_type(self.path)
				if mtype in ["application/x-application-cmg"]:
					return 2
				else:
					# Check size of file
					size = os.stat( self.path )[6];
					if size < 1:
						self.__cmg_version = 0
					f = open(self.path, "r")
					if size > 32775:
						f.seek(32769)
						magic = str(f.read(5)).strip()
						if magic.startswith( "CD001" ):
							# iso
							f.seek(22)
							magic = str(f.read(28)).strip()
							# Ima is NOT a typo!  only picking 28 chars up
							if (magic.startswith("Compressed Application Image") or
							    magic.startswith("KLIK2 ISO") or
							    magic.startswith("# Compressed Application Ima") or
							    magic.startswith("# KLIK2 ISO")):
								f.close()
								self.__cmg_version = 2
								return self.__cmg_version
							# this should really mount and check for recipe not seek(22)
					f.seek(0)
					magic = str(f.read(4)).strip()
					if (len(magic)==4 and
					    ord(magic[0])==69 and
					    ord(magic[1])==61 and
					    ord(magic[2])==205 and
					    ord(magic[3])==40):
						# cramfs
						f.close()
						self.__cmg_version = 1
						return self.__cmg_version
						# this should really mount and check it is a klik1 cmg
					#if self.is_klik2_cmg( path ):
						#return 2
					#if self.is_klik1_cmg( path ):
						#return 1
					f.close()
			self.__cmg_version = 0
		return self.__cmg_version

	# Return all the files in a given CMG that match pre and post text
	def find_files(self, pre_text=None, post_text=None, omit_extension=False):
		if omit_extension:
			# If post text contains a file extension, remove it
			if post_text:
				post_text = os.path.splitext( post_text )[0]

		files = []
		p = subprocess.Popen(["isoinfo", "-R", "-f", "-i", self.path], stdout=subprocess.PIPE)
		#p = subprocess.Popen(["cmginfo", self.path], stdout=subprocess.PIPE) - this is WAY to slow
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

	def scan_cmg_icons(self, do):
		# Returns a list of icons for a .desktop entry

		icon = do.get( "Icon" )
		if icon and icon != "/.DirIcon":
			# Check if the icon belongs to the default theme
			theme_icon = None
			if icon[0] != "/" and self.klik.xdg.get_is_gtk_installed():
				try:
					# never rely on gtk
					import gtk.utils
					theme_icon = gtk.utils.find_icon_from_theme( icon )
				except Exception, e:
					print "cmg.scan_cmg_icons() - Error:", e

				if theme_icon:
					return [theme_icon]

			if not theme_icon:
				# If icon doesn't belong to a theme, search it in the CMG
				icon_files = self.find_files( "/usr/share/icons/", icon, omit_extension=True )
				if icon_files:
					return icon_files
		# Icon doesn't exist, fallback to CMG icon
		return ["/.DirIcon"]


	# Get desktop objects set to execute cmg
	def get_desktop_objects(self, fix_exec=True):

		# Search application .desktop files
		desktop_objects = []
		for file in self.find_files("/usr/share/applications/", ".desktop"):
			# Extract and parse the .desktop file
			temp_path = self.extract_file( file )

			if temp_path:
				do = DesktopParser( temp_path )
				os.remove( temp_path )

				if do.get("Exec") and (do.get("Type") == "Application"):
					do.set("X-CMG", self.path)
					desktop_objects.append( do )
			else:
				print "\t!! Couldn't extract", file

		# Attempt to generate desktop files from debian menu
		if len(desktop_objects) == 0:

			menu_files = self.find_files("/usr/share/menu/")
			for menu_file in menu_files:
				# Extract and parse the .desktop file
				temp_path = self.extract_file( menu_file )
				for mo in MenuParser( temp_path ).menu_objects:
					do = DesktopParser()

					do.set("Type", "Application")
					do.set("Exec", mo.command)
					do.set("Name", mo.title.capitalize())
					#do.set("Terminal", recipe.require_terminal)
					do.set("Icon", mo.icon)
					do.set("Categories", mo.freedesktop_category)
					do.set("X-CMG", self.path)

					desktop_objects.append( do )

				os.remove( temp_path )

		# Couldn't find any desktop file, build a basic one
		if len(desktop_objects) == 0:

			# With klik2 packages, extract information from recipe
			# With klik1 packages, make up information

			cmg_version = self.is_valid_cmg()

			if cmg_version == 2:
				recipe = self.get_recipe()
			else:
				recipe = None

			if recipe:
				if recipe.command:
					command = recipe.command
				else:
					command = recipe.name
				name = recipe.name.capitalize()
				terminal = recipe.require_terminal
			else:
				command = ""
				name = os.path.splitext( os.path.basename( self.path ) )[0].capitalize()
				terminal = False

			do = DesktopParser()

			do.set("Name", name)
			do.set("Terminal", terminal)
			do.set("Exec", command)
			do.set("Type", "Application")
			do.set("Icon", "/.DirIcon")
			do.set("X-CMG", self.path)

			# Debtags / categories
			if recipe and recipe.debtags.has_key("use"):
				category = klik.utils.freedesktop.get_freedesktop_category_from_debtag( recipe.debtags["use"] )
				if category != None:
					do.set("Categories", category)

			desktop_objects.append( do )

		return desktop_objects

	def extract_icon(self, file_path_to, resize=None):
		if self.extract_file("/.DirIcon", file_path_to):
			# Resize image if needed
			if resize:
				if resize < 0:
					resize = self.klik.settings.desktop_icon_size # TODO: review this behaviour
				path = klik.utils.images.resize(file_path_to, resize)
				if path != file_path_to:
					shutil.copy(  path, file_path_to )
					os.remove( path )
			return True
		return False

	def check_for_missing_library(self, command=None):
		ex = KlikExecute(self.klik)
		if not command:
			command = self.__get_command([None], False)
		if command:
			return ex.check_for_missing_library(self, command)
		return "Cancelled"

	def get_unique_id(self, name=None):
		# If given a desktop entry name, returns the complete pakage id
		# Otherwise, returns only the package id prefix
		prefix = "cmg-" + urllib.quote_plus( self.path )
		if name:
			return prefix + '-' + urllib.quote_plus( name )
		return prefix

	# Properties
	cmg_version = property(fget=is_valid_cmg)
	recipe = property(fget=get_recipe)
