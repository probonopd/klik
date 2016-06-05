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
import sys
import glob
import tempfile
import urllib
import shutil
import re
from klik.utils.freedesktop import DesktopParser
from gettext import ngettext

from klik.utils.xdg import XdgUtils
from klik.utils.notify import Notify


class KlikIntegration(object):

	### Auxiliar functions that deal with our CMG registered package identifiers
	### "cmg-" + absolute_path_to_cmg + "-" + menu_entry_name

	def __get_package_id(self, cmg_path, do=None):
		# If given a desktop object, returns the complete pakage id
		# Otherwise, returns only the package id prefix
		prefix = "cmg-" + urllib.quote_plus( cmg_path )
		if do:
			return prefix + '-' + urllib.quote_plus( do.get( "Name" ) )
		return prefix

	def __get_app_name(self, filename):
		# Given a .desktop or icon filename, extracts the menu entry name
		app_name = os.path.splitext( filename )[0]
		return urllib.unquote_plus( app_name[app_name.rfind(".cmg-")+5:] )

	def __get_cmg_path(self, filename):
		return urllib.unquote_plus( filename[4:filename.find(".cmg")+4] )
	
	def __is_valid_menu_entry(self, filename):
		return filename.startswith( "cmg-" )

	def __get_menu_entry_mask(self, cmg_path, check_version=False, check_path=False):
		if check_path:
			# `path_to_.desktop_files/cmg-cmg_name_w/_path...
			cmg_path = urllib.quote_plus( cmg_path )
		else:
			# `path_to_.desktop_files/cmg-*cmg_name_w/out_path...
			cmg_path = "*" + urllib.quote_plus( os.path.split(cmg_path)[1] )
		if check_version:
			#                         ...cmg_name_w/_version.cmg-*`
			cmg_path = os.path.splitext(cmg_path)[0]
		else:
			#                         ...cmg_name_w/out_version*.cmg-*`
			cmg_path = cmg_path[:cmg_path.rfind("_")] + "*"
		return os.path.join( self.base.xdg.get_xdg_apps_path(), "cmg-" + cmg_path + ".cmg-*" )


	### Auxiliar functions to dela with colliding packages


	def __check_already_registered(self, cmg_path):
		# Checks if the CMG pointed by `cmg_path` is already registered.
		# Returns: ( len( [list of colliding paths] ),
		#            len( [list of colliding versions] ),
		#            [list of colliding .desktop files] )
		
		paths = []
		versions = []
		
		collisions = glob.glob( self.__get_menu_entry_mask( cmg_path ) )
		if collisions:
			paths = set(map( self.__get_path, collisions ))       # list of paths w/out reps
			versions = set(map( self.__get_version, collisions )) # list of versions w/out reps
		return (len(paths)-1, len(versions)-1, collisions)
		
	def __get_collision_status(self, menu_entry):
		# Extracts the collision status from a .desktop file
		do = DesktopParser( menu_entry )
		version = "(" + self.__get_version( menu_entry ) + ")"
		path = "(CMG is in " + self.__get_path( menu_entry ) + ")"
		name = do.get("Name") or ""
		comment = do.get("Comment") or ""
		return (comment.endswith(path), name.endswith(version))

	def __get_path(self, menu_entry):
		# Extracts the path of a CMG from the .desktop file name
		menu_entry = os.path.split( menu_entry )[1]
		menu_entry = urllib.unquote_plus( menu_entry )
		return menu_entry[4:menu_entry.rfind("/")]
	
	def __get_version(self, cmg_path):
		# Extracts the version of a CMG from the .desktop file name
		return urllib.unquote_plus( cmg_path[ cmg_path.rfind("_")+1 :
						      cmg_path.rfind(".cmg") ] )

	def __modify_menu_entry(self, menu_entry, code, remove_data=False):
		# Adds CMG data to a menu entry file, in order to distinguish
		# menu entries created by colliding CMGs
		
		# Get .desktop file
		
		do = DesktopParser( menu_entry )
		
		# Modify .desktop file
			
		if code == "version":
			data_to_add = "(" + self.__get_version( menu_entry ) + ")"
			field = "Name"
		elif code == "path":
			data_to_add = "(CMG is in " + self.__get_path( menu_entry ) + ")"
			field = "Comment"
			
		field_data = do.get( field )
			
		if not remove_data:
			if field_data:
				# Avoid adding information twice if registering twice
				if field_data.endswith( data_to_add ): return
				field_data += " " + data_to_add
			else:
				field_data = data_to_add
		else: # remove data
			if field_data and field_data.endswith( data_to_add ):
				field_data = field_data[:field_data.rfind( data_to_add )]
				if field_data and field_data[-1] == " ":
					field_data = field_data[:-1]

		do.set( field, field_data )

		# Write new .desktop file

		desktop_file = open( menu_entry, "w" )
		do.write( desktop_file )
		desktop_file.close()
		
	def __add_version(self, menu_entry):
		return self.__modify_menu_entry( menu_entry, "version" )
	def __del_version(self, menu_entry):
		return self.__modify_menu_entry( menu_entry, "version", remove_data=True )
	def __add_path(self,menu_entry):
		return self.__modify_menu_entry( menu_entry, "path" )
	def __del_path(self, menu_entry):
		return self.__modify_menu_entry( menu_entry, "path", remove_data=True )



	def __init__(self, base):
		self.base = base
		self.notify = Notify()
		self.max_items_to_notify = 20
		self.allow_same_version_collisions = False
			# TODO: Make this configurable by the user?
			#       "[] Allow register the same CMG more than once"
			#       (disabled by default)
		

	def register(self, cmg_list, command_options=set()):
		
		regexp_icon = re.compile("icons/([a-z]+)/([0-9]+)x")
		
		def register_icon(icon_path):
			# Tries to register the given icon, considering the different possible natures of icons:
			#   1.- Themed
			#   2.- Inside CMG
			#   3.- Already in the filesystem
			
			# If path is not in a filesystem, icon is themed: skip processing
			if icon_path[0] != "/":
				return True

			# icon_path[2:] to avoid `extension = .DirIcon`
			extension = os.path.splitext( icon_path[2:] )[1]
			temp_icon_path = os.path.join( build_path, package_id + extension)
			
			# Try to extract icon from CMG
			if self.base.extract_file( cmg_path, icon_path, temp_icon_path ):
				if not extension:
					# Guess image type
					import imghdr # TODO: svg? ---> add entry to 'tests' list
					extension = "." + ( imghdr.what( temp_icon_path ) or "xpm" )
					shutil.move( temp_icon_path, temp_icon_path + extension )
					temp_icon_path += extension

			# If image was not in CMG, try to get it from filesystem
			else:
				try:
					shutil.copy( icon_path, temp_icon_path )
				except IOError:
					return False

			# Try to guess icon size and theme
			regexp_result = regexp_icon.search( icon_path )
			if not temp_icon_path.endswith(".svg"): # FIXME: xdg-utils do not support SVG icons
				if regexp_result:
					theme, size = regexp_result.groups()
					self.base.xdg.icon_install( temp_icon_path, theme, size, noupdate=True )
				else:
					self.base.xdg.icon_install( temp_icon_path, noupdate=True )

			os.remove( temp_icon_path )
			
			return True

		def is_already_registered( cmg_path ):
			if "f" in command_options: return False
			return len( glob.glob( self.__get_menu_entry_mask(cmg_path,
					       check_version=True,
					       check_path=self.allow_same_version_collisions) ) ) > 0

	
		# some distros do not have .local/share/applications by default
		#os.mkdir( self.base.xdg.get_xdg_apps_path() )

		apps_to_notify = set()
		notify_icon = None

		cmg_list = filter( self.base.is_valid_cmg, cmg_list )

		for cmg_path in cmg_list:

			# Make sure CMG path is absolute
			if cmg_path[0] != "/":
				cmg_path = os.path.abspath(cmg_path)

			print _("Registering %s") % cmg_path
			
			if is_already_registered( cmg_path ):
				print "\t", _("Package already registered")
				continue

			build_path = tempfile.mkdtemp( '.register.' + os.environ["USER"],
						       self.base.settings.temp_directory_path )
			self.base.temp_paths.append( build_path )
			
			# Registering one menu entry per desktop object found/created

			desktop_objects = self.base.get_desktop_objects( cmg_path )
			for do in desktop_objects:
				# Package ID is the CMG name escaped with the specific .desktop name...
				package_id = self.__get_package_id( cmg_path, do )

				# Icons: each menu entry can have many icons, in different sizes and themes
				icon_list = self.base.scan_cmg_icons( cmg_path, do )
				icon_list = filter( register_icon, icon_list )
				if icon_list:
					# At least one valid icon could be installed
					if (icon_list[0][0] == "/"): # Icon is not themed
						do.set( "Icon", package_id )
				else:
					# No valid icon for this entry, fallback to klik default icon
					do.set( "Icon", os.path.join(sys.path[0], os.pardir, "share", "klik", "klik.png") )
					print "\t", _("Icon does not exist")

				# Write .desktop file
				temp_desktop_path = os.path.join( build_path, package_id + ".desktop" )
				temp_desktop_file = open( temp_desktop_path, "w" )
				do.write( temp_desktop_file )
				temp_desktop_file.close()

				# Register .desktop file and remove temporal file
				self.base.xdg.desktop_install( temp_desktop_path, noupdate=True )
				os.remove( temp_desktop_path )

				# Checking if the CMG is already registered, with the
				# same version or with a different one.
				(paths, versions, colliding_entries) = self.__check_already_registered( cmg_path )
				if paths:
					map( self.__add_path, colliding_entries )
				if versions:
					map( self.__add_version, colliding_entries )


				apps_to_notify.add( " - " + do.get("Name") )
				notify_icon = do.get( "Icon" )
	
			os.rmdir( build_path )



		self.base.xdg.icon_update()
		self.base.xdg.desktop_update()

		# Notification
		self.notify_event( _("The following menu items have been added") + ":\n\n%s\n\n" +
				   ngettext("To uninstall remove the file from your application folder",
					    "To uninstall remove the files from your application folder",
					    len(apps_to_notify)) + ".\n",
				   apps_to_notify, notify_icon )



	def notify_event(self, message, applications, notify_icon="application-x-extension-cmg"):
		if applications and self.base.settings.application_folder_notify:
			n_apps = len(applications)
			applications = list(applications)
			# If more than one package need notification, force default
			# icon instead of any package-specific icon
			if n_apps > 1:
				notify_icon = "application-x-extension-cmg"
			# Limit number of displaied items
			if n_apps > self.max_items_to_notify:
				applications = applications[0:self.max_items_to_notify]
				applications.append( _(" ... and %d more")
						     % (n_apps - self.max_items_to_notify) )
			applications.sort()
			self.notify.show( _("CMG Application Folder") ,
					  message % "\n".join(applications),
					  notify_icon )
	
		
	def __is_valid_cmg_name(self, path):
		# If the file exists, returns true if it's a valid CMG
		# Otherwise, returns true if the name is valid
		return self.base.is_valid_cmg( path ) or path.lower().endswith( ".cmg" )
	
	def unregister(self, cmg_list):

		apps_to_notify = set()

		cmg_list = filter( self.__is_valid_cmg_name, cmg_list )
	
		for cmg_path in cmg_list:

			# Make sure CMG path is absolute
			if cmg_path[0] != "/":
				cmg_path = os.path.abspath( cmg_path )
			
			print _("Unregistering %s") % cmg_path
			
			package_id_prefix = self.__get_package_id( cmg_path )
				
			# Look for .desktop files owned by the CMG, and xdg-uninstall them
			menu_entries = glob.glob( os.path.join(self.base.xdg.get_xdg_apps_path(),
							       package_id_prefix + "*") )
			
			if menu_entries:
				(old_paths, old_versions) = self.__get_collision_status( menu_entries[0] )
			
				for file in menu_entries:
					self.base.xdg.desktop_uninstall( file, noupdate=True )
					apps_to_notify.add( " - " + self.__get_app_name(file) )
				
				# If unregistering this package ends with a collision situation,
				# remove extra info from the menu entry that is left.
				
				(paths, versions, colliding_entries) = self.__check_already_registered( cmg_path )
				if old_paths and not paths:
					map( self.__del_path, colliding_entries )
				if old_versions and not versions:
					map( self.__del_version, colliding_entries )

			# Look for icons owned by the CMG, and xdg-uninstall them

			self.base.xdg.icon_uninstall( package_id_prefix, uninstall_all=True )

		self.base.xdg.desktop_update()

		# Notification
		self.notify_event( _("The following menu items have been removed:") + "\n\n%s", apps_to_notify )
		
	
	def sync(self, apps_dirs):
		# Perform some cleanup, registering and unregistering packages as needed,
		# trying to leave the system as consistent as possible.
		#
		# apps_dirs: List of Application directories. The goal of the function is to
		#            register every .cmg in that directory and unregister any previous
		#            registered .cmg that is currently not in that directory.

		# Make sure paths are absolute
		apps_dirs = map( os.path.abspath, apps_dirs )

		# List of valid CMGs in all Applications directories.
		registered_cmgs = map( lambda x : glob.glob(x+"/*"), apps_dirs )    # list of lists
		registered_cmgs = reduce( list.__add__, registered_cmgs )           # flat list
		registered_cmgs = filter( self.base.is_valid_cmg, registered_cmgs ) # list of valid CMGs
		registered_cmgs = set( registered_cmgs )                            # for faster lookups


		### Register non-registered packages existent in any of the Application directories

		def cmg_is_not_registered(cmg_path):
			# Returns: True if there are NO .desktop files registered
			#          that correspond with `cmg_path`
			return not glob.glob( os.path.join(self.base.xdg.get_xdg_apps_path(),
							   self.__get_package_id( cmg_path )) + "*" )
		
		cmgs_to_register = filter( cmg_is_not_registered, registered_cmgs )
		
		if cmgs_to_register:
			self.register( cmgs_to_register )

	
		### Unregister packages that left litter in .desktop or icon directories
					

		def purge_dir(data_dir):
			# For each file in `data_dir`, look if the associated CMG is registered.
			# If not, unregister the associated CMG to remove all the litter it left
			#
			# data_dir: Path of directory with registered files (.desktop's, icons, etc)
			# Returns: Set of packages that need unregistration.
			
			if not os.path.isdir( data_dir ):
				return set()

			packages_to_unregister = set()

			for filename in os.listdir( data_dir ):
				if self.__is_valid_menu_entry( filename ):
					cmg_path = self.__get_cmg_path( filename )
					if cmg_path not in registered_cmgs:
						packages_to_unregister.add( cmg_path )
			
			return packages_to_unregister


		packages_to_unregister = set()

		# Orphan .desktop files
		packages_to_unregister.update( purge_dir( self.base.xdg.get_xdg_apps_path() ) )

		# Orphan icon files
		for icon_path in self.base.xdg.get_xdg_icon_paths():
			packages_to_unregister.update( purge_dir( icon_path ) )

		if packages_to_unregister:
			self.unregister( packages_to_unregister )
