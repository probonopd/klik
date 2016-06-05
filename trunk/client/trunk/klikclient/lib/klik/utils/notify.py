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


class Notify( object ):
	
	def open_application_folder(self):
		pass
	
	def show(self, heading, text, icon):
		try:	
			# We can not rely on this working
			import pynotify
			if not pynotify.init("klik"):
				return False
		
			n = pynotify.Notification(heading, text, icon)
			return n.show()
		except ImportError:
			return False
		
if __name__ == '__main__':
	n = Notify()
	n.show("Application Registered", "test <b>test</b>", None)
