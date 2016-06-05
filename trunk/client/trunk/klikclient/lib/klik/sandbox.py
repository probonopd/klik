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

"""
	Work out what should be sandboxed
		data - all writes
		home - all users home folder
		portable - all users dot files
"""
import sys
import os

class KlikSandBox (object):

	def __init__(self, cmg_path):
	
		self.cmg_path = cmg_path
		self.jail_type = "none"		#Types are none, data, home, portable
		self.jail_path = ""
		self.__check_for_sandbox(cmg_path)
	
	def __check_for_sandbox(self, cmg_path):
	
		basename  = os.path.basename(cmg_path)
		basepath = os.path.dirname(cmg_path)
		
		# .app.cmg, app.cmg.data, .app.cmg.data
		paths = [os.path.join(basepath, "." + basename), os.path.join(basepath, "." + basename + ".data"), os.path.join(basepath, basename + ".data")]
		if self.__check_path(paths, "data"):
			return 
		
		# app.cmg.home, .app.cmg.home
		paths = [os.path.join(basepath, "." + basename + ".home"), os.path.join(basepath, basename + ".home")]
		if self.__check_path(paths, "home"):
			return 	
				
		# app.cmg.portable, .app.cmg.portable
		paths = [os.path.join(basepath, "." + basename + ".portable"), os.path.join(basepath, basename + ".portable")]
		if self.__check_path(paths, "portable"):
			return 
		
		self.jail_type = "none"
		self.jail_path = ""	
		
		return 
		
	def __check_path(self, jail_paths, jail_type):
		for path in jail_paths:
			if os.path.exists( path ) and os.access(path, os.X_OK) and os.access(path, os.W_OK):
				self.jail_type = jail_type
				self.jail_path = path
				return True
		return False
		
	def create_jail(self, jail_type="data"):
		basename  = os.path.basename(self.cmg_path)
		basepath = os.path.dirname(self.cmg_path)
		path = os.path.join(basepath, basename + "." + jail_type)
		if not os.path.exists(path):
			os.makedirs(path)
		return 
