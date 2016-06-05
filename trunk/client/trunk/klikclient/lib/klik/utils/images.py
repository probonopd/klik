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

import os

def resize(file_path, size):
	try:
		# TODO: What about using PIL here?
		# 	more portable, more poserful
		# 	http://www.pythonware.com/library/pil/handbook/index.htm
		# 	(RazZziel)
		# 	PIL is not always installed, remember minimise deps - Killerkiwi
		import gtk
		pixbuf = gtk.gdk.pixbuf_new_from_file_at_size(file_path, size, size)
		os.remove(file_path)
		path = os.path.splitext(file_path)[0] + ".png"
		pixbuf.save(path, "png")
		return path
	except Exception, text:
		print "Image resize failed:", text
	return None
"""
This method will overlay an image instead.......
def resize(file_path, size):
	import gtk
	pixbuf2 = gtk.gdk.pixbuf_new_from_file("/home/jason/code/klik/trunk/client/trunk/install/mimetype-icon-large.png")
	x = 41
	y = 41
	h = 45
	w = 45
	pixbuf = gtk.gdk.pixbuf_new_from_file_at_size(file_path, w, h)
	pixbuf.composite(pixbuf2, x, y, w, h, x, y, 1.0, 1.0, gtk.gdk.INTERP_TILES, 254)
	path = os.path.splitext(file_path)[0] + ".png"
	pixbuf2.save(path, "png")
	return path
"""
def guess_type(file_path):
	try:
		import imghdr # TODO: svg? ---> add entry to 'tests' list
		return imghdr.what( file_path ) or "xpm"
	except Exception, text:
		print "Image guess failed:", text
