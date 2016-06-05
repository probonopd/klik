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
	This base object should be as neutral as possible about which mounting method is being used

	all mounting / unmounting code should be in its own sub class
"""
import sys
import os, os.path
import glob
import shutil
from subprocess import *
import time
import tempfile
from klik.utils.xdg import XdgUtils
from klik.exception import ExecuteException
from klik.sandbox import KlikSandBox

# Compatability scripts
from klik.compat.generic import KlikCompatGeneric
from klik.compat.kde import KlikCompatKDE
from klik.compat.python import KlikCompatPython
from klik.compat.mono import KlikCompatMono


class KlikExecute (object):

	def __init__(self, klik):
		self.klik = klik

	# == START :: FOR CHILD OBJECT TO DECLARE ============================
	def mount_cmg(self, path, union=True, sandbox=None, cmg_version=2):
		return "PATH/TO/MOUNT"
	def umnount_cmg (self, mount_point_path):
		return True
	# == END :: FOR CHILD OBJECT TO DECLARE ============================

	def extract_file(self, cmg, file_path_from, file_path_to):
		# Extract a single file
		mount_point_path = self.mount_cmg(cmg, True)
		shutil.copy(os.path.join(mount_point_path, file_path_from), file_path_to)
		self.umnount_cmg (mount_point_path)

	def extract_all(self, cmg, path_to):
		# Extract all files
		self.mount_cmg(cmg, False)

		if not os.path.exists(path_to):
			os.makedirs(path_to)

		# Use cp its more verbose
		# TODO: How does this hold up under large cmgs...
		files = glob.glob( os.path.join(cmg.mount_point_path, "*") )
		if len(files) > 0:
			call(["cp"] + files + [path_to, "-r", "-v", "-p"])

		# make sure we get dot files in root directory
		files = glob.glob( os.path.join(cmg.mount_point_path, ".*") )
		if len(files) > 0:
			call(["cp"] + files + [path_to, "-r", "-v", "-p"])

		os.chmod(path_to, 0755)

		self.umnount_cmg (cmg.mount_point_path)

	def __prep(self, cmg, command, reusable_mount_point=True):

		# Check for file
		if not cmg.exists():
			raise ExecuteException("CMG file does not exist! " + cmg.path)

		# Check for sandbox
		if cmg.sandbox.jail_type != "none":
			self.klik.notify.show( "Application is in Jail" , cmg.sandbox.jail_path , "application-x-application-cmg" )

		self.mount_cmg(cmg, True, reusable_mount_point)

		# Check if terminal is required
		if not self.klik.xdg.get_is_terminal() and cmg.get_recipe().require_terminal:
			command = self.klik.xdg.get_terminal_command(title=command[0]) + ["bash -c \"%s ; echo -ne '\\nApplication terminated, press enter to quit'; read\"" % " ".join(command)]

		# To start kde wrapper (if needed)
		KlikCompatKDE(self.klik).init_enviroment(cmg)

		# Setup fake chroot
		libfakechroot = os.path.abspath(os.path.join(self.klik.sys_path, os.path.pardir, "lib", "klik", "libkfakechroot.so"))
		if os.getenv("LD_PRELOAD") != None:
			os.environ["LD_PRELOAD"] = libfakechroot + ":" + os.getenv("LD_PRELOAD")
		else:
			os.environ["LD_PRELOAD"] = libfakechroot

		# Exclude these paths from chroot
		os.environ["FAKECHROOT_EXCLUDE_PATH"] = "/tmp:/proc:/dev:/var/run:" + os.path.expanduser(self.klik.settings.config_path)

		if cmg.sandbox.jail_path:
			os.environ["FAKECHROOT_EXCLUDE_PATH"] = os.environ["FAKECHROOT_EXCLUDE_PATH"] + ":" + cmg.sandbox.jail_path

		# Change the working directory, fixs some apps (looking at you tee wars)
		if len(command) > 0 and len(command[0]) > 0 and command[0][0] == "/":
			os.environ["FAKECHROOT_WORKING_DIRECTORY"] = os.path.dirname(command[0])

		# HACKS START HERE - we need to remove these if at all possible !!!
		KlikCompatGeneric(self.klik).init_enviroment(cmg)
		KlikCompatPython(self.klik).init_enviroment(cmg)
		KlikCompatMono(self.klik).init_enviroment(cmg)
		# HACKS END HERE - we need to remove these if at all possible !!!

		# Recipe enviroment varibles
		if cmg.recipe:
			for env in cmg.recipe.enviroment:
				default = ""

				# is there a default value?.... why do we even need this seems odd....
				if env.default != "":
					default = ":" + env.default
				if env.mode == "prepend":
					if os.getenv(env.name) != None:
						os.environ[env.name] = env.insert + ":" + os.getenv(env.name)
					else:
						os.environ[env.name] = env.insert + default
				if env.mode == "append":
					if os.getenv(env.name) != None:
						os.environ[env.name] = os.getenv(env.name) + ":" + env.insert
					else:
						os.environ[env.name] =  env.insert + default
				if env.mode == "replace":
					os.environ[env.name] = env.insert


		return command

	def execute_shell(self, cmg, command):

		command = self.__prep (cmg, [command])

		args =  ["chroot", cmg.mount_point_path, "bash", "--rcfile", os.path.abspath(os.path.join(sys.path[0], os.path.pardir, "share", "klik", "bashrc")), "-i"]
		print """=====================================================
You are now in a klik virtual application shell

Your command was """ + " ".join(command) +"""

Type "exit" to return
====================================================="""
		try:
			p = Popen(args, env=os.environ.copy(), cwd=cmg.mount_point_path)
			p.wait()

		except Exception, text:
			self.umnount_cmg (cmg.mount_point_path)
			raise ExecuteException("Error occurred while starting application")

		self.umnount_cmg (cmg.mount_point_path)

		return True

	# Execute should always be the same, regardless of mount type (hopefully)
	def execute(self, cmg, command, execute_args, catch_errors=False):

		command = self.__prep(cmg, [command] + execute_args)

		p = None
		error_log_path = None
		error_log = None
		error_text = ""
		application_returncode = 0
		args = ["chroot", cmg.mount_point_path] + command

		try:
			if catch_errors:

				# TODO: Make this path under /tmp/klik
				error_log_path = tempfile.mktemp()
				error_log = open(error_log_path, "w")
				self.klik.temp_paths.append( error_log_path )

				p = Popen(args, close_fds=True, env=os.environ, cwd=cmg.mount_point_path, stderr=PIPE)
				for line in p.stderr:
					sys.stderr.write(line)
					error_log.write(line)
			else:
				p = Popen(args, close_fds=True, env=os.environ, cwd=cmg.mount_point_path)

			p.wait()

		except Exception, text:

			self.umnount_cmg (cmg.mount_point_path)
			raise ExecuteException("Error occurred while starting application - " + str(text))

		self.umnount_cmg (cmg.mount_point_path)

		# Tidy up the error log tee
		if catch_errors:

			p.stderr.close()
			error_log.close()

			# Read error log
			error_log = open(error_log_path, "r")
			error_text = error_log.read()
			error_log.close()

			# Scrub error log of username
			error_text = error_text.replace(os.environ["USER"], "__$USER__")

		result = p.returncode

		# Patch for our feedback, we dont want X window kills reported as fail
		if result == 1 and error_text.strip().startswith("X connection to") > -1:
			result = 0

		return result, error_text

	# Execute should always be the same, regardless of mount type (hopefully)
	def check_for_missing_library(self, cmg, command):

		command = self.__prep(cmg, [command], False)

		p = None
		text = ""
		application_returncode = 0
		args = ["chroot", cmg.mount_point_path, "which"] + command
		try:
			command = Popen(args, env=os.environ, stdout=PIPE, stderr=open(os.devnull, "w")).communicate()[0].strip()
			os.environ["LD_TRACE_LOADED_OBJECTS"] = "1"
			text = Popen(os.path.join(cmg.mount_point_path, command[1:]), env=os.environ, stdout=PIPE, stderr=open(os.devnull, "w")).communicate()[0].strip()
			os.unsetenv("LD_TRACE_LOADED_OBJECTS");

		except Exception, text:
			os.unsetenv("LD_TRACE_LOADED_OBJECTS");
			print str(Exception)
			print str(text)
			self.umnount_cmg (cmg.mount_point_path)
			raise ExecuteException("Error occurred while starting application")


		self.umnount_cmg (cmg.mount_point_path)

		return text
