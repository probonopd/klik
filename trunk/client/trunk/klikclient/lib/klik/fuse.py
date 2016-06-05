#!/usr/bin/env python

#   klik2 - allows the mounting of and iso or crramfs with a union overlay
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
import commands
import sys
import os, os.path
import shutil
import tempfile
from subprocess import *
import time
import klik.execute
from klik.exception import ExecuteException

class KlikExecute (klik.execute.KlikExecute):

	def __init__(self, klik):
		self.klik = klik

	def mount_cmg(self, cmg, union=True, reusable_mount_point=False):
		os.unsetenv("LD_PRELOAD");
                version = cmg.is_valid_cmg()
		if version == 2:
			cmg.mount_point_path = self.__mount_iso(cmg, union, reusable_mount_point)
		if version == 1:
			cmg.mount_point_path = self.__mount_cramfs(cmg, union, reusable_mount_point)
		return cmg.mount_point_path

	def __get_mount_point(self, cmg, reusable_mount_point):
		# lock mountpoint by cmg path, this will allow us to reuse mount
		#if reusable_mount_point:
		#	# path needs to be under tmp, else nautilus shows a window
		#	path = os.path.expanduser(os.path.join( self.klik.settings.temp_directory_path,  "execute", os.environ["USER"], cmg.get_unique_id()))
		#	if not os.path.exists(path):
		#		os.makedirs(path)
		#	return os.path.join(path, "mnt")
		#else:

		# Insure temp path exists
		if not os.path.exists(self.klik.settings.temp_directory_path):
			os.makedirs(self.klik.settings.temp_directory_path)
		path = tempfile.mkdtemp('.execute.' + cmg.get_unique_id(), self.klik.settings.temp_directory_path)

		return path



	def __mount_cramfs(self, cmg, union=True, reusable_mount_point=True):

		# create mount point
		mount_point_path = self.__get_mount_point(cmg, reusable_mount_point)

		# only mount if not already mounted
		if not self.is_mount_in_use(mount_point_path):

			# Mount the application image (CMG)
			fusecram = os.path.abspath(os.path.join(self.klik.sys_path, os.path.pardir, "bin", "fusecram"))

			command = [fusecram, "-n"]

			# Advanced glibc loader
			if self.klik.settings.glibc_loader:
				command.append("-g")

			# Add union
			if union:
				command.append("-u")

			# Add sandbox options
			command = self.__get_sandbox_options(command, cmg.sandbox)

			# Append paths
			command = command + ["-p", cmg.path, mount_point_path, "-s"]

			# Attempt mount, suppress error text but check return code
			try:
				p = Popen(command) # , stderr=open(os.devnull, "w")
				p.wait()
				if p.returncode != 0:
					raise ExecuteException("Could not mount cmg using fusecram\n\nCommand:\n" + " ".join(command))
			except:
				raise ExecuteException("Could not mount cmg using fusecram\n\nCommand:\n" + " ".join(command))

		self.__create_pid_file(mount_point_path)
		return mount_point_path


	def __mount_iso(self, cmg, union=True, reusable_mount_point=True):

		# create mount point
		mount_point_path = self.__get_mount_point(cmg, reusable_mount_point)

		# only mount if not already mounted
		#if not reusable_mount_point or not os.path.exists(mount_point_path):

		# Mount the application image (CMG)
		fusioniso = os.path.abspath(os.path.join(self.klik.sys_path, os.path.pardir, "bin",  "fusioniso"))
		command = [fusioniso, "-n"]

		# Advanced glibc loader
		if self.klik.settings.glibc_loader:
			command.append("-g")

		# Add union
		if union:
			command.append("-u")

		# Add sandbox options
		#command = self.__get_sandbox_options(command, cmg.sandbox)

		# Append paths
		command = command + ["-p", cmg.path, mount_point_path, "-s"]

		# Attempt mount, suppress error text but check return code
		try:
			p = Popen(command) # , stderr=open(os.devnull, "w")
			p.wait()
			if p.returncode != 0:
				raise ExecuteException("Could not mount cmg using fusioniso\n\nCommand:\n" + " ".join(command))
		except:
			raise ExecuteException("Could not mount cmg using fusioniso\n\nCommand:\n" + " ".join(command))

		#if reusable_mount_point:
		#	self.__create_pid_file(mount_point_path)

		return mount_point_path

	def umnount_cmg (self, mount_point_path):

		# only unmount if last client
		#if self.__client_count(mount_point_path) < 2:

			# always wait until mount is not being used before attempting to unmount
			self.__wait_for_processes_using_mount(mount_point_path)

			# ask fusioniso to unmount properly its mount point
                        command = ["fusermount", "-u", mount_point_path]
                        p = Popen(command) #, stderr=open(os.devnull, "w")
                        p.wait()

                        # Unmount the cmg, attempt this at least three times
                        #       This aggressive approach is much better than leaving still mounted cmgs
                        #       polluting the system...
                        #try:
                        attempt = 0
                        while attempt < 6:

                                # Wait 1 second before attempting again
                                time.sleep(1)

                                # TODO : Check __is_last_client here ?
                                if not os.path.exists(mount_point_path):
                                        break

                                # try an aggressive approach is the previous one fails
                                if attempt == 3:
                                        command = ["fusermount", "-z", "-u", mount_point_path]
                                        p = Popen(command) #, stderr=open(os.devnull, "w")
                                        p.wait()

                                # try to remove mount point (only if empty)
                                ret = os.rmdir(os.path.abspath(os.path.join(mount_point_path, os.path.pardir)))
                                if ret == 0:
                                        break
                                else:
                                        attempt = attempt + 1

			if attempt == 6:
				raise ExecuteException("3rd attempt failed - could not unmount cmg using fusermount\n\nPath:\n" + mount_point_path)
			#except:
			#	raise ExecuteException("! Could not unmount cmg using fusermount\n\nPath:\n" + mount_point_path)

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
		if os.path.exists(mount_point_path):
			p = Popen(["fuser", "-m", mount_point_path], stdout=PIPE, stdin=PIPE, stderr=PIPE)
			p.wait()
			return p.returncode == 0
		return False



	## Curently Unused Methods ############################################################
	def __get_sandbox_options(self, command, sandbox):
		# Add sandbox options
		if sandbox != None and sandbox.jail_type != "none":
			if sandbox.jail_type == "home":
				command = command + ["-b", sandbox.jail_path]
			elif sandbox.jail_type == "portable":
				command = command + ["-d", sandbox.jail_path]
			#else:
			#	command = command + ["-B", sandbox.jail_path]
		return command

	def __create_pid_file(self, mount_point_path):
		path = os.path.join(mount_point_path, os.path.pardir, "pid")
		if not os.path.exists(path):
			os.makedirs(path)
		p = open( os.path.join(path, str( os.getpid() )), "w")
		p.close()

	def __delete_pid_file(self, mount_point_path):
		path = os.path.abspath(os.path.join(mount_point_path, os.path.pardir, "pid",  str( os.getpid() )))
		if os.path.exists(path):
			os.remove(path)

	def __client_count(self, mount_point_path):

		# check other pids
		client_count = 0
		path = os.path.join(mount_point_path, os.path.pardir, "pid")
		if os.path.exists( os.path.join(mount_point_path, os.path.pardir, "pid") ):
			for root, dirs, files in os.walk(path):
				for pid in files:
					# check pid is still alive
					if not commands.getoutput('ls /proc | grep %s' % pid):
						# pid is dead kill the pid file
						os.remove( os.path.abspath(os.path.join(mount_point_path, os.path.pardir, "pid", pid)))
					else:
						client_count = client_count + 1
		return client_count
