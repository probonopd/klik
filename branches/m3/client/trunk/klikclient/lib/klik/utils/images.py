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

class Images (object):

	def check_image_size(self, file_path, default_size):
		try:
			import gtk
			pixbuf = gtk.gdk.pixbuf_new_from_file_at_size(file_path, default_size, default_size)
			pixbuf.save(file_path, "png")

		except Exception, text:
			print "Image Resize Failed : %s" % text
