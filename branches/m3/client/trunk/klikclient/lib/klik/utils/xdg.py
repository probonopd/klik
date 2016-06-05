#!/usr/bin/env python

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

"""
	Python wrapper for project portland utils and other distro neutral commands
	All commands should work regardless of distro
	
	- http://www.freedesktop.org/wiki/Software/xdg-user-dirs
	- http://portland.freedesktop.org/wiki/
"""

import os
import sys
import re
import glob
from subprocess import *

class XdgUtils (object):

	def __init__(self, xdg_path = None):
		self.path = xdg_path
		self.__get_is_gtk_installed = None
		self.__desktop_dir = None
				
		# default location of custom xdg
		if self.path == None or self.path == "":
			self.path = os.path.abspath(os.path.join(sys.path[0], os.path.pardir, "lib" , "klik", "klik", "utils", "xdg-utils"))

		self.xdg_data_home = os.getenv( "XDG_DATA_HOME", os.path.join( os.getenv("HOME"), ".local", "share") ) # Default
				
	def __get_command(self, command):
		# detect system xdg command
		path = Popen(["which", command], stdout=PIPE, stderr=open(os.devnull, "w")).communicate()[0].strip()
		if path == "":
			# Command not installed use our custom version
			path = os.path.join(self.path, command)
		
		return path
	
	def get_desktop_enviroment(self):
		# Taken from orignal xdg scripts
		if os.getenv("GNOME_DESKTOP_SESSION_ID") != None and os.getenv("GNOME_DESKTOP_SESSION_ID") != "":
			return "GNOME"
		if os.getenv("KDE_FULL_SESSION") != None and os.getenv("KDE_FULL_SESSION") == "true":
			return "KDE"

		try:
			if Popen(["xprop", "-root", "_DT_SAVE_MODE"], stdout=PIPE, stderr=open(os.devnull, "w")).communicate()[0].strip().lower().count("xfce") > 0:
				return "XFCE"
		except:
			pass		

		return "CLI"

	def get_distribution(self):
	
		#  Try lsb first
		lsb_release = None

		lsb_release = Popen(["which", "lsb_release"], stdout=PIPE, stderr=open(os.devnull, "w")).communicate()[0].strip()
		if lsb_release != "":
			lsb_release = Popen([lsb_release, "-d"], stdout=PIPE, stderr=open(os.devnull, "w")).communicate()[0].strip()

		try:
			# try /etc/issue file
			if lsb_release == "":
				fp = open("/etc/issue")
				lsb_release = lsb_release + fp.readline().strip().strip(";")
				lsb_release = lsb_release + fp.readline().strip().strip(";")
				fp.close()
				lsb_release = re.sub("\\\\.*", "",  lsb_release).strip()
			else:
				lsb_release = lsb_release[13:].strip()

			workaround = ""
			# Kubuntu shows as Ubuntu (same code base)
			if self.get_desktop_enviroment() == "KDE" and os.path.exists( "/usr/share/kubuntu-default-settings" ):
				workaround = "KUBUNTU : "
			
			distro = workaround + lsb_release
			
			# Clean out switchs	
			return distro
		except:
			return  "unknown"
			
	def is_root(self):
		return Popen("whoami", stdout=PIPE).communicate()[0].strip() == "root" 
		
	def get_chipset_info(self):
		return Popen(["uname", "-m"], stdout=PIPE, stderr=open(os.devnull, "w")).communicate()[0].strip()
		
	def open(self, file_path):
		command =  self.__get_command("xdg-open")
		path = '"' + path + '"'
		call(["sh", command, file_path])

	def get_terminal_command(self, title="klik application shell"):
	
		# XDG wont do what we need... most of the terminals use factories
		de = self.get_desktop_enviroment()
		
		if de == "GNOME":
			return ["gnome-terminal", "--disable-factory", "--title", title, "-e"]
		if de == "KDE":
			return ["konsole", "-T", title, "-e"] # FIXME For some reason, this don't work (at least for me) - RazZziel
		return [self.__get_command("xdg-terminal")]
		
	def terminal(self, cmd):
		command = self.get_terminal_command()
		call(["sh"] + command + cmd)	
			
	def mime_install(self, path):
		command = self.__get_command("xdg-mime") 
		call(["sh", command, "install", path])
		
	def mime_uninstall(self, path):
		command = self.__get_command("xdg-mime") 
		call(["sh", command, "uninstall", path])

	def mime_icon_install(self, size, path, mime):
		command = self.__get_command("xdg-icon-resource")
		call(["sh", command, "install", "--context", "mimetypes", "--size", str(size), path, mime])
	
	def mime_icon_uninstall(self, size, path, mime):
		print ">",path
		command = self.__get_command("xdg-icon-resource")
		call(["sh", command, "uninstall", "--context", "mimetypes", "--size", str(size), path, mime])
		
	def desktop_install(self, path, noupdate=False):
		command = self.__get_command("xdg-desktop-menu")
		if noupdate: call(["sh", command, "install", "--noupdate", "--novendor", path])
		else:        call(["sh", command, "install", "--novendor", path])
			
	def desktop_uninstall(self, path, noupdate=False):
		command = self.__get_command("xdg-desktop-menu")
		if noupdate: call(["sh", command, "uninstall", "--noupdate", path])
		else:        call(["sh", command, "uninstall", path])

	def desktop_update(self):
		command = self.__get_command("xdg-desktop-menu")
		call(["sh", command, "forceupdate"])

	def icon_install(self, path, theme=None, size=48, noupdate=False):
		command = self.__get_command("xdg-icon-resource")
		command = ["sh", command, "install"]
		if noupdate: command += ["--noupdate"]
		if theme:    command += ["--theme", theme]
		call( command + ["--size", str(size), path] )
		
	def icon_uninstall(self, path, theme=None, size=48, noupdate=False, uninstall_all=False):
		command = self.__get_command("xdg-icon-resource")
		command = ["sh", command, "uninstall"]
		if not uninstall_all:
			if noupdate: command += ["--noupdate"]
			if theme: command += ["--theme", theme]
			call( command + ["--size", str(size), path] )
		else:
			command += ["--noupdate"]
			
			# Uninstall icons for all themes and sizes
			# `path` should come in the form `"cmg-" + urllib.quote_plus( cmg_path )`
			
			icons_to_uninstall = []
			for dir, theme, size in self.get_xdg_icon_paths(extended=True):
				for icon in glob.glob( os.path.join(dir, path) + "*" ):
					icons_to_uninstall += [(icon, theme, size)]
			
			for path, theme, size in icons_to_uninstall:
				call( command + ["--theme", theme, "--size", str(size), path] )
			
			self.icon_update()
				
	
	def icon_update(self):
		command = self.__get_command("xdg-icon-resource")
		call(["sh", command, "forceupdate"])


	def get_xdg_data_home(self):
		return self.xdg_data_home
	
	def get_xdg_apps_path(self):
		# Returns the directory where .desktop files are placed
		return os.path.join( self.get_xdg_data_home(), "applications" )
		
	def get_xdg_icon_paths(self, extended=False):
		# Returns the list of directories there icons are placed,
		# considering all the installed themes and sizes
		icon_path_pre = os.path.join( self.get_xdg_data_home(), "icons" )

		icon_paths = []
		
		if os.path.isdir( icon_path_pre ):

			for dir in glob.glob(icon_path_pre + "/*/*"):
				[theme, size] = dir.rsplit("/", 2)[-2:]
				if extended:
					icon_paths.append( (os.path.join( dir, "apps" ),
							    theme,
							    size[:len(size)/2]) )
				else:
					icon_paths.append( os.path.join( dir, "apps" ) )
		return icon_paths
	
	def get_is_terminal(self):
		#return os.getenv("TERM") != None # FAILS on suse
		return sys.stdout.isatty() # if stdout is connected then most likly a termnial

	def get_is_x_display(self):
		return os.getenv("DISPLAY") != None
	
	
	def get_is_gtk_installed(self):
		if self.__get_is_gtk_installed == None:
			try:
			 	import pygtk
			 	pygtk.require("2.0")
			 	
			 	import gtk
				import gtk.glade
				# any others to check for ?
				
				self.__get_is_gtk_installed = True
			except:
			  	self.__get_is_gtk_installed = False
		return self.__get_is_gtk_installed
	
	def get_is_qt_installed(self):
		return False
		
	def get_lang(self):
		lang = "UNKNOWN"
		if os.getenv("LANG") != None:
			lang = os.environ['LANG']
		return lang

	def __get_user_dir(self, key, default):
		"""
		http://www.freedesktop.org/wiki/Software/xdg-user-dirs
			XDG_DESKTOP_DIR
			XDG_DOWNLOAD_DIR
			XDG_TEMPLATES_DIR
			XDG_PUBLICSHARE_DIR
			XDG_DOCUMENTS_DIR
			XDG_MUSIC_DIR
			XDG_PICTURES_DIR
			XDG_VIDEOS_DIR
		"""
		user_dirs_dirs = os.path.expanduser("~/.config/user-dirs.dirs")
		if os.path.exists(user_dirs_dirs):
			f = open(user_dirs_dirs, "r")
			for line in f.readlines():
				if line.startswith(key):
					return os.path.expandvars(line[len(key)+2:-2])
		return default
	
	def get_desktop_dir(self):
		if self.__desktop_dir == None:
			self.__desktop_dir =  self.__get_user_dir("XDG_DESKTOP_DIR", os.path.expanduser("~/Desktop"))
		return self.__desktop_dir
	
	def get_free_space(self, directory):
		# return free space available for a given directory
		size = 0
		p = Popen(["df", directory], stdout=PIPE, stderr=open(os.devnull, "w"))
		p.stdout.readline() # sktip first line as it has headings
		for line in p.stdout.readlines():
			size = size + float(line[40:50].strip())
		return size
		
	def get_installed_libs(self):
	
		result = []
		
		result.append("PYTHON")
		
		if len(Popen(["which", "gnome-about"], stdout=PIPE, stderr=open(os.devnull, "w")).communicate()[0].strip()) > 0:
			result.append("GNOME")
			
		if self.get_is_gtk_installed():
			result.append("GTK")
		
		if len(Popen(["which", "kde-config"], stdout=PIPE, stderr=open(os.devnull, "w")).communicate()[0].strip()) > 0:
			try:
				if Popen(["kde-config" ,"--version"], stdout=PIPE, stderr=open(os.devnull, "w")).communicate()[0].strip().count("KDE") > 0:
					result.append("KDE")
		
				if Popen(["kde-config" ,"--version"], stdout=PIPE, stderr=open(os.devnull, "w")).communicate()[0].strip().count("Qt: 3.") > 0:
					result.append("QT3")
		
				if Popen(["kde-config" ,"--version"], stdout=PIPE, stderr=open(os.devnull, "w")).communicate()[0].strip().count("Qt: 4.") > 0:
					result.append("QT4")
			except:
				pass
		# Every thing after here is being collected out of intrest.. we are not filtering based on these
		# Although we may do in the future
		
		if len(Popen(["which", "mono"], stdout=PIPE, stderr=open(os.devnull, "w")).communicate()[0].strip()) > 0:
			result.append("MONO")
		
		if len(Popen(["which", "java"], stdout=PIPE, stderr=open(os.devnull, "w")).communicate()[0].strip()) > 0:
			result.append("JAVA")
			
		if len(Popen(["which", "wine"], stdout=PIPE, stderr=open(os.devnull, "w")).communicate()[0].strip()) > 0:
			result.append("WINE")
			
		return (result)
# TEST
if __name__ == "__main__":
	xdg = XdgUtils()
	print "X Display : " + str(xdg.get_is_x_display())
	print "Desktop Environment : " + xdg.get_desktop_enviroment()
	print "GTK Installed : " + str(xdg.get_is_gtk_installed())
	print "Lang : " + xdg.get_lang()
	print "Distro : " + xdg.get_distribution()
	print "Currently Root or Sudo : " + str(xdg.is_root())
	print "Chipset : " + xdg.get_chipset_info()
	print "Desktop Dir : " + xdg.get_desktop_dir()
	print "Freespace : " + str(xdg.get_free_space("/"))
	print "Terminal : " + str(xdg.get_is_terminal())
