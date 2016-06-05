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
import shutil
import popen2
from subprocess import *
import time
import tempfile
from klik.utils.xdg import XdgUtils
from klik.exception import ExecuteException
from klik.sandbox import KlikSandBox

class KlikExecute (object):

	def __init__(self, klik):
		self.klik = klik

	# == START :: FOR CHILD OBJECT TO DECLARE ============================
	def mount_cmg(self, cmg_path):
		return "PATH/TO/MOUNT"
	def umnount_cmg (self, mount_point_path):
		return True
	# == END :: FOR CHILD OBJECT TO DECLARE ============================

	def extract_file(self, cmg_path, file_path_from, file_path_to):
		# Extract a single file
		mount_point_path = self.mount_cmg(cmg_path)
		shutil.copy(os.path.join(mount_point_path, file_path_from), file_path_to)
		self.umnount_cmg (mount_point_path)

	def extract_all(self, cmg_path, path_to):
		# Extract all files
		mount_point_path = self.mount_cmg(cmg_path, union=False)
		try:
			if not os.path.exists(path_to):
				os.makedirs(path_to)
		
			# Use cp its more verbose
			p = Popen(["cp",  mount_point_path, path_to, "-r", "-v", "-p"])
			p.wait()
			#shutil.copytree(mount_point_path, path_to)
		except:
			pass
	
		os.chmod(path_to, 0755)
		self.umnount_cmg (mount_point_path)

	def __prep(self, command, cmg_path, execute_args, cmg_version=2):
		recipe = None
		mount_point_path = None
		
		# Check for file
		if not os.path.exists(cmg_path):
			raise ExecuteException("CMG file does not exist! " + cmg_path)

		# Check for sandbox
		sandbox = KlikSandBox(cmg_path)
		if sandbox.jail_type != "none":
			self.klik.notify.show( "Application is in Jail" , sandbox.jail_path , "application-x-extension-cmg" )

		mount_point_path = self.mount_cmg(cmg_path, True, sandbox, cmg_version)

		# Load the recipe file
		recipe_path = os.path.join(mount_point_path, "recipe.xml")
		if  os.path.isfile( recipe_path ):
			recipe = self.klik.load_recipe( recipe_path )


		# Determine Command
		if command[0] == None or command[0] == "" :
			# Check for wrapper script
			wrapper_path = os.path.join(mount_point_path, "wrapper")
			if os.path.isfile( wrapper_path ) :
				command[0] = "sh"
				command.append( wrapper_path )
			elif recipe != None:
				command[0] = recipe.command
			else:
				# Need to do some searching...
				command[0] = recipe.name
		
		# Check if terminal is required
		if not self.klik.xdg.get_is_terminal() and recipe.require_terminal:
			command = self.klik.xdg.get_terminal_command(title=command[0]) + ["bash -c \"%s ; echo -ne '\\nApplication terminated, press enter to quit'; read\"" % " ".join(command)]

		# Set enviroment
		libfakechroot = os.path.abspath(os.path.join(self.klik.sys_path, os.path.pardir, "lib", "klik", "libfakechroot.so"))
		if os.getenv("LD_PRELOAD") != None:
			os.environ["LD_PRELOAD"] = libfakechroot + ":" + os.getenv("LD_PRELOAD")
		else:
			os.environ["LD_PRELOAD"] = libfakechroot
			
		# Exclude these paths from chroot
		os.environ["FAKECHROOT_EXCLUDE_PATHS"] = "/tmp:/proc:/dev:/var/run"
		if sandbox.jail_path:
			os.environ["FAKECHROOT_EXCLUDE_PATHS"] = os.environ["FAKECHROOT_EXCLUDE_PATHS"] + ":" + sandbox.jail_path

		# Update path to work across distros
		os.environ["PATH"] = "/usr/lib/sbin:/bin:/usr/sbin:/usr/bin:/usr/X11R6/bin:/usr/local/sbin:/usr/local/bin:/usr/games:" + os.environ["PATH"]
		
		os.environ["LD_LIBRARY_PATH"] = "/usr/lib:%s/lib:%s/lib:%s/usr/lib/:%s/opt/kde3/lib:%s/usr/X11R6/lib/" % (mount_point_path, mount_point_path, mount_point_path, mount_point_path, mount_point_path)

		# Change the working directory
		if len(command) > 0 and len(command[0]) > 0 and command[0][0] == "/":
			os.environ["FAKECHROOT_WORKING_DIRECTORY"] = os.path.dirname(command[0])

		# HACKS START HERE - we need to remove these if at all possible !!!
		
		# add all /usr/lib sub folders to LD_LIBRARY_PATH
		# This fix makes wireshark work, why dosnt wireshark see the union?
		if cmg_version == 2:
			directories = self.klik.find_sub_directories("/usr/lib", cmg_path)
			for dir in directories:
				os.environ["LD_LIBRARY_PATH"] = os.environ["LD_LIBRARY_PATH"] + ":" + os.path.join(mount_point_path, dir[1:])

		# HACKS END HERE - we need to remove these if at all possible !!!
		
		# Recipe enviroment varibles
		if recipe != None:
			for env in recipe.enviroment:
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

		return command, mount_point_path

	def execute_shell(self, cmg_path):
	
		command, mount_point_path = self.__prep([None], cmg_path, None)

		args =  ["chroot", mount_point_path, "bash", "--rcfile", os.path.abspath(os.path.join(sys.path[0], os.path.pardir, "share", "klik", "bashrc")), "-i"]
		print """=====================================================
You are now in a klik2 virtual application shell

Your command was """ + " ".join(command) +"""

Type "exit" to return
====================================================="""
		try:
			p = Popen(args, env=os.environ.copy(), cwd=mount_point_path)
			p.wait()

		except Exception, text:
			self.umnount_cmg (mount_point_path)
			raise ExecuteException("Error occurred while starting application")

		self.umnount_cmg (mount_point_path)

		return True

	# Execute should always be the same, regardless of mount type (hopefully)
	def execute(self, command, cmg_path, execute_args, catch_errors=False, cmg_version=2):

		command, mount_point_path = self.__prep([command], cmg_path, execute_args, cmg_version)

		p = None
		error_log_path = None
		error_log = None
		error_text = ""
		application_returncode = 0

		args = ["chroot", mount_point_path] + command

		try:
			if catch_errors:
				
				# TODO: Make this path under /tmp/klik
				error_log_path = tempfile.mktemp()
				error_log = open(error_log_path, "w")
				self.klik.temp_paths.append( error_log_path )

				p = Popen(args, close_fds=True, env=os.environ.copy(), cwd=mount_point_path, stderr=PIPE)
				for line in p.stderr:
					sys.stderr.write(line)
					error_log.write(line)
			else:
				p = Popen(args, close_fds=True, env=os.environ, cwd=mount_point_path)
				
			p.wait()

		except Exception, text:
			self.umnount_cmg (mount_point_path)
			raise ExecuteException("Error occurred while starting application - " + str(text))

		self.umnount_cmg (mount_point_path)
		
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
	def check_for_missing_library(self, command, cmg_path):

		command, mount_point_path = self.__prep([command], cmg_path, None)

		p = None
		text = ""
		application_returncode = 0
		
		args = ["chroot", mount_point_path, "which"] + command
		try:
			#os.environ["LD_TRACE_LOADED_OBJECTS"] = "1"
			command = Popen(args, env=os.environ, cwd=mount_point_path , stdout=PIPE, stderr=open(os.devnull, "w")).communicate()[0].strip()

			args = ["ldd", os.path.join(mount_point_path, command[1:])]
			text = Popen(args, env=os.environ, cwd=mount_point_path , stdout=PIPE, stderr=open(os.devnull, "w")).communicate()[0].strip()

			#os.environ["LD_TRACE_LOADED_OBJECTS"] = ""
		except Exception, text:
			#os.environ["LD_TRACE_LOADED_OBJECTS"] = ""
			self.umnount_cmg (mount_point_path)
			raise ExecuteException("Error occurred while starting application")

		
		self.umnount_cmg (mount_point_path)

		return text
