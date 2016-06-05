#!/usr/bin/env python
# -*- coding: utf-8 -*-

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
import re
from ConfigParser import ConfigParser

# From Alcatare.util.py
# see http://standards.freedesktop.org/desktop-entry-spec/desktop-entry-spec-0.9.4.html
class DesktopParser(ConfigParser):
	def __init__(self, filename=None, file_type='Application'):
		ConfigParser.__init__(self)
		self.filename = filename
		self.file_type = file_type
		if filename:
			if len(self.read(filename)) == 0:
				#file doesn't exist
				self.add_section('Desktop Entry')
		else:
			self.add_section('Desktop Entry')
		self._list_separator = ';'

	def optionxform(self, option):
		#makes keys not be lowercase
		return option

	def get(self, option, locale=None):
		locale_option = option + '[%s]' % locale
		try:
			value = ConfigParser.get(self, 'Desktop Entry', locale_option)
		except:
			try:
				value = ConfigParser.get(self, 'Desktop Entry', option)
			except:
				return None
		if self._list_separator in value:
			value = value.split(self._list_separator)
		if value == 'true':
			value = True
		if value == 'false':
			value = False
		return value

	def set(self, option, value, locale=None):
		if locale:
			option = option + '[%s]' % locale
		if value == True:
			value = 'true'
		if value == False:
			value = 'false'
		if isinstance(value, tuple) or isinstance(value, list):
			value = self._list_separator.join(value) + ';'
		ConfigParser.set(self, 'Desktop Entry', option, value)

	def write(self, file_object):
		file_object.write('[Desktop Entry]\n')
		items = []
		if not self.filename:
			file_object.write('Encoding=UTF-8\n')
			file_object.write('Type=' + str(self.file_type) + '\n')
		for item in self.items('Desktop Entry'):
			items.append(item)
		items.sort()
		for item in items:
			file_object.write(item[0] + '=' + item[1] + '\n')
	
	# turn command into an array
	def get_exec_array(self):
		command = []
		p = re.compile("('[^']+')|(\"[^\"]+\")|([^\s]+)")
		for r in re.finditer(p, self.get("Exec")):
			command.append( r.group(0) )
		return command
			
def get_freedesktop_category_from_debtag( debtag ):
	# http://packages.debian.org/about/debtags
	# Using the deb tag use define a freedesktop.org category
	# Utility, System, Science, Education, 	AudioVideo, Game, Network, Settings, Graphics, Office, Development	
	mapping = {}
	mapping["analysing"] = "Utility"
	mapping["browsing"] = "Network"
	mapping["chatting"] = "Network"
	mapping["checking"] = "Utility"
	mapping["comparing"] = "Utility"
	mapping["compressing"] = "Utility"
	mapping["configuring"] = "Settings"
	mapping["converting"] = "Utility"
	mapping["dialing"] = "Network"
	mapping["downloading"] = "Network"
	mapping["driver"] = "System"
	mapping["editing"] = "Office"
	mapping["entertaining"] = "AudioVideo"
	mapping["filtering"] = "Utility"
	mapping["gameplaying"] = "Game"
	mapping["learning"] = "Education"
	mapping["login"] = "Settings"
	mapping["monitor"] = "Utility"
	mapping["organizing"] = "Office"
	mapping["playing"] = "AudioVideo"
	mapping["printing"] = "Graphics"
	mapping["proxying"] = "Network"
	mapping["routing"] = "Network"
	mapping["scanning"] = "Graphics"
	mapping["searching"] = "Utility"
	mapping["storing"] = "Utility"
	mapping["synchronizing"] = "Utility"
	mapping["text-formatting"] = "Office"
	mapping["timekeeping"] = "Utility"
	mapping["transmission"] = "Network"
	mapping["typesetting"] = "Graphics"
	mapping["viewing"] = "Graphics"

	if mapping.has_key( debtag ):
		return mapping[ debtag ]
	
	return None


