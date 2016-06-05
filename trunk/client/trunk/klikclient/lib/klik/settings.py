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

from xml.dom.minidom import *
import os
import sys
import shutil
from klik.utils.xdg import XdgUtils

class KlikSettings(object):

	# dont let user change these ones
	temp_directory_path = "/tmp/klik/"
	default_download_url = "http://klik.atekon.de/klik2/recipe.gpg.php?package="
	default_feedback_url = "http://klik.atekon.de/feedback/"
	version = "1.9.2"
	config_path = "~/.klik"

	authors = ["Simon Peter <probono@myrealbox.com>",
			"Kurt Pfeifle <pfeifle@kde.org>",
			"Jason Taylor <killerkiwi2005@gmail.com>",
			"Lionel Tricon <lionel.tricon@free.fr>",
			"Niall Walsh <niallw@gmail.com>",
			"Ismael Barros <razielmine@gmail.com>",
			"Denis Washington <dwashington@gmx.net>"
			"",
			"Thanks to all testers and contributors",
			"in #klik on irc.freenode.net"]
	documenters = ["Kurt Pfeifle <pfeifle@kde.org>"]
	website = "http://klikclient.googlecode.com"

	def __init__(self):
		xdg = XdgUtils()

		# Can we use a custom glib loader?
		self.glibc_loader = os.path.exists("/lib/ld-klik2.so.2")

		# Attempt to load version from svnversion file
		svnversion_path = os.path.join(sys.path[0] , os.pardir, "share", "klik", "svnversion")
		if os.path.exists(svnversion_path):
			f = open(svnversion_path, "r")
			self.version =  self.version + "." + f.readline().strip() + "svn"
			f.close()

		# default destination
		self.destination = xdg.get_desktop_dir()

		# Hardcoded default values
		self.desktop_icon_size = 48
		self.debug = False
		self.temp_cache_downloads = False
		self.application_folders = [self.destination]
		self.application_folder_notify = True
		self.always_show_app_chooser = False
		self.default_jail_type = 0

		# Load global, override code
		system_settings_path = os.path.join(sys.path[0], os.pardir, "share", "klik", "settings.xml")
		self.__load_file( system_settings_path )

		# Load user, overides global
		user_settings_directory = os.path.expanduser("~/.klik")
		user_settings_path = os.path.join(user_settings_directory, "settings.xml")
		self.__load_file( user_settings_path )

		# Remove duplicates in application folders list
		set = {}
		self.application_folders = [set.setdefault(x,x) for x in self.application_folders if x not in set]

		# If user settings file doesn't exist, create it
		if not os.path.isdir(user_settings_directory):
			os.mkdir(user_settings_directory)
		if not os.path.exists(user_settings_path):
			shutil.copy(system_settings_path, user_settings_path)

		if os.getenv("KLIK_DEBUG") != None:
			self.debug = (os.getenv("KLIK_DEBUG") == "1")

	def __load_file(self, path):

		# private method to eval an arg to a bool value
		def mk_bool( arg ):
			return arg == "1" or arg.lower() == "true"

		if os.path.exists(path):

			# Load XML document
			try:
				self.document = parse( path )

				# Load settings

				self.__parse( "destination",               os.path.expanduser )
				self.__parse( "desktop_icon_size",         int )
				self.__parse( "debug",                     mk_bool )
				self.__parse( "temp_cache_downloads",      mk_bool )
				self.__parse( "application_folder_notify", mk_bool )
				self.__parse( "always_show_app_chooser",   mk_bool )
				self.__parse( "default_jail_type",         int )

				self.__parse_list( "application_folders", "folder", os.path.expanduser )

				# Cleanup
				self.document.unlink() # http://docs.python.org/lib/module-xml.dom.minidom.html

				#self.__print_values() # Debug, prints the value of all settings

			except xml.parsers.expat.ExpatError, err:
				print "!! %s contains syntax errors (%s)" % (path, err)

	def __parse(self, node_name, parse_func):
		# self.`node_name` = parse_func(value of `node_name` in XML file)
		nodes = self.__getNodes( self.document, node_name )
		if nodes:
			default_value = getattr( self, node_name )
			setattr( self, node_name, parse_func( self.__getNodeValue(nodes[0], default_value) ) )

	def __parse_list(self, node_name, child_name, parse_func):
		nodes = self.__getNodes( self.document, node_name )
		if nodes:
			lst = getattr( self, node_name )
			children = self.__getNodes( nodes[0], "folder" )
			# Each item is added to the begining of the list,
			# so that last added items have more priority
			for child in children:
				lst.insert( 0, parse_func( self.__getNodeValue(child) ) )
			setattr( self, node_name, lst )

	def __getNodes(self, element, tagname):
		return element.getElementsByTagName(tagname)

	def __getNodeValue(self, element, defaultValue=None):
		node =  element.firstChild
		if node:
			if node.nextSibling:
				return node.nextSibling.nodeValue.strip()
			else:
				return node.nodeValue.strip()
		return defaultValue

	def __print_values(self):
		for (key, value) in vars(self).iteritems():
			if key not in ["document"]:
				print "\t%s = %s" % (key, value)

	def print_settings(self, settings):

		try:
			for var_name in settings:
				var = getattr( self, var_name )
				if type( var ) is list:
					print "\n".join( var )
				elif type( var ) is bool:
					print int( var )
				else:
					print var
				print
			return 0

		except:
			return 1
