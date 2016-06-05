#!/usr/bin/env python
# -*- coding: utf-8 -*-

#   klik2 
#   Copyright (C) 2008  XXXXXXXXXXXX
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

class SafeException(Exception):
	"""An exception that can be reported to the user without a stack trace.
	The command-line interface's C{--verbose} option will display the full stack trace."""
	def __init__(self, value):
		self.value = value
	def __str__(self):
		return self.value

class ExecuteException(Exception):
	"""An exception thathas occured while tring to execute"""
	def __init__(self, value):
		self.value = value
	def __str__(self):
		return self.value
