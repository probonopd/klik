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
	This is fairly basic it just reads in the string and parses using regular expressions
	http://olympus.het.brown.edu/cgi-bin/dwww?type=file&location=/usr/share/doc/menu/html/ch3.html
"""

import os
import sys
import re
# /usr/share/menu/<package-name>
class MenuParser(object):
	def __init__(self, filename):
		self.menu_objects = []
	
		f = open(filename, "r")
		content =  f.read()
		f.close()
		
		sections = content.split("\n?")
		
		for section in sections:
			self.menu_objects.append( MenuObject(section) )

class MenuObject(object):
	def __init__(self, content):

		self.icon = ""
		self.command = ""
		self.title = ""
		self.section = ""
		self.freedesktop_category = ""
		
		p = re.compile(r'icon="?(?P<value>[^"\\\n]+)')
		m = p.search(content)
		if m != None:
			self.icon = m.group('value').strip()
		
		p = re.compile(r'command="?(?P<value>[^"\\\n]+)')
		m = p.search(content)
		if m != None:
			self.command = m.group('value').strip()
		
		p = re.compile(r'title="?(?P<value>[^"\\\n]+)')
		m = p.search(content)
		if m != None:
			self.title = m.group('value').strip()

		p = re.compile(r'section="?(?P<value>[^"\\\n]+)')
		m = p.search(content)
		if m != None:
			self.section = m.group('value').strip()
			
		self.freedesktop_category =  self.freedesktop_category_from_section( self.section )
		
		
	def freedesktop_category_from_section(self, section):
		
		# Utility, System, Science, Education, 	AudioVideo, Game, Network, Settings, Graphics, Office, Development	
		# Games
		if section.find("Games/") > -1:
			return "Game"
		
		if section.find("Apps/Databases")  > -1 or section.find("Apps/Editors")  > -1:
			return "Office"
			
		if section.find("Apps/Education")  > -1:
			return "Education"
			
		if section.find("Apps/Graphics")  > -1:
			return "Graphics"
			
		if section.find("Apps/Science")  > -1 or section.find("Apps/Math")  > -1 or section.find("Apps/Technical")  > -1:
			return "Science"
			
		if section.find("Apps/Net")  > -1:
			return "Network"
			
		if section.find("Apps/Programming")  > -1:
			return "Development"
			
		if section.find("Apps/Sound")  > -1:
			return "AudioVideo"
			
		if section.find("Apps/Viewers")  > -1:
			return "Graphics"
			
		if section.find("Apps/Tools")  > -1 or section.find("Apps/Emulators")  > -1 or section.find("Apps/XShells")  > -1 or section.find("Apps/Shells")  > -1:
			return "Utility"
			
		if section.find("Apps/System")  > -1:
			return "System"
		
		return ""

"""
Apps            - normal apps
      Emulators     - dosemu, etc.
     
      Hamradio      - anything relating to ham radio.

      Text          - text oriented tools other than editors.

   
    Help            - programs that provide user documentation
    Screen          - programs that affect the whole screen 
      Lock          - xlock, etc.
      Save          - screen savers
      Root-window   - things that fill the root window
    WindowManagers  - X window managers 
      Modules       - window manager modules
"""




if __name__ == "__main__":
	test = MenuParser(sys.argv[1])
	print "icon : " + test.icon
	print "title : " + test.title
	print "command : " + test.command
	print "section : " + test.section
