#!/usr/bin/env python

#   klik2 - allows the mounting of and iso with a union overlay
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
	All commands that are fuse specific
"""

import sys
import os, os.path
import shutil
import tempfile
from subprocess import *
import time
import urllib
import klik.execute
from klik.exception import ExecuteException

class KlikExecute (klik.execute.KlikExecute):

	def __init__(self, klik):
		self.klik = klik

	def mount_cmg(self, cmg_path, union=True, sandbox=None, cmg_version=2):
		if cmg_version == 2:
			return self.__mount_iso(cmg_path, union, sandbox)
		if cmg_version == 1:
			return self.__mount_cramfs(cmg_path, union, sandbox)

	def __get_mount_point(self, cmg_path):
		# lock mountpoint by cmg path
		#reutrn os.path.join(self.klik.settings.temp_directory_path, 'execute.' + urllib.quote_plus(cmg_path))
		return tempfile.mkdtemp('.execute.' + urllib.quote(os.path.basename(cmg_path)) , self.klik.settings.temp_directory_path)

	def __get_sandbox_options(self, command, sandbox):
		# Add sandbox options
		if sandbox != None and sandbox.jail_type != "none":
			if sandbox.jail_type == "home":
				command = command + ["-b", sandbox.jail_path]
			elif sandbox.jail_type == "portable":
				command = command + ["-d", sandbox.jail_path]
			else: 
				command = command + ["-B", sandbox.jail_path]
		return command


	def __mount_cramfs(self, cmg_path, union=True, sandbox=None):
	
		# create mount point
		mount_point_path = self.__get_mount_point(cmg_path)
		
		# Mount the application image (CMG)
		fusecram = os.path.abspath(os.path.join(self.klik.sys_path, os.path.pardir, "bin", "fusecram"))

		command = [fusecram, "-n"]

		# Add union
		if union:
			command.append("-u")
		
		# Add sandbox options
		command = self.__get_sandbox_options(command, sandbox)

		# Append paths
		command = command + ["-p", cmg_path, mount_point_path, "-s"]

		# Attempt mount, suppress error text but check return code
		try:
			p = Popen(command) # , stderr=open(os.devnull, "w")
			p.wait()
			if p.returncode != 0:
				raise ExecuteException("Could not mount cmg using fusecram\n\nCommand:\n" + " ".join(command))
		except:
			raise ExecuteException("Could not mount cmg using fusecram\n\nCommand:\n" + " ".join(command))
		return mount_point_path


	def __mount_iso(self, cmg_path, union=True, sandbox=None):

		# create mount point
		mount_point_path = self.__get_mount_point(cmg_path)

		# Mount the application image (CMG)
		fusioniso = os.path.abspath(os.path.join(self.klik.sys_path, os.path.pardir, "bin",  "fusioniso"))

		command = [fusioniso, "-n"]

		# Add union
		if union:
			command.append("-u")
		
		# Add sandbox options
		command = self.__get_sandbox_options(command, sandbox)

		# Append paths
		command = command + ["-p", cmg_path, mount_point_path, "-s"]
		
		# Attempt mount, suppress error text but check return code
		try:
			p = Popen(command) # , stderr=open(os.devnull, "w")
			p.wait()
			if p.returncode != 0:
				raise ExecuteException("Could not mount cmg using fusioniso\n\nCommand:\n" + " ".join(command))
		except:
			raise ExecuteException("Could not mount cmg using fusioniso\n\nCommand:\n" + " ".join(command))
		return mount_point_path

	def umnount_cmg (self, mount_point_path):
	
		# always wait until mount is not being used before attempting to unmount
		self.__wait_for_processes_using_mount(mount_point_path)
	
		# Unmount the cmg, attempt this at least three times
		#	This aggressive approach is much better than leaving still mounted cmgs
		#	polluting the system...
		try:
			attempt = 0
			while attempt < 3:
				attempt = attempt + 1
				p = Popen(["fusermount", "-u", mount_point_path], stderr=open(os.devnull, "w"))
				p.wait()

				# Wait 1 second before attempting again
				if p.returncode != 0:
					time.sleep(1)
				else:
					break;

			if attempt == 3:
				raise ExecuteException("Could not unmount cmg using fusermount\n\nPath:\n" + mount_point_path)
		except:
			raise ExecuteException("Could not unmount cmg using fusermount\n\nPath:\n" + mount_point_path)
	
	def __wait_for_processes_using_mount(self, mount_point_path):
		
		# New method waits on pids to avoid a timer
		fuse_in_use = True
		while fuse_in_use > 0:
			# List all pids active for this mount point
			fuser = Popen(["fuser", "-m", "-v", mount_point_path], stdout=PIPE, stdin=PIPE, stderr=PIPE)
			for line in fuser.stdout.readlines():
				words = line.strip().split(" ")
				for word in words:
					try:
						pid = int(word)
						os.waitpid(pid, 0) # wait on pid
					except:
						pass
			fuser.stdout.close()
			fuser.wait()
			fuse_in_use = fuser.returncode == 0

	def is_mount_in_use(self, mount_point_path):
		p = Popen(["fuser", "-m", mount_point_path], stdout=PIPE, stdin=PIPE, stderr=PIPE)
		p.wait()
		return p.returncode != 0
