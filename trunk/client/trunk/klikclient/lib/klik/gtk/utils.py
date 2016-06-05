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


import gtk

def find_icon_from_theme( iconName ):
	if iconName and not '/' in iconName and iconName[-3:] in ('png', 'svg', 'xpm'):
		iconName = iconName[:-4]
	icon_theme = gtk.icon_theme_get_default()
	icon = icon_theme.lookup_icon(iconName, 24, 0)
	if icon != None:
		return iconName
	return None
	
def getIcon(iconName, for_properties=False):
	pixbuf, path = None, None

	if iconName and not '/' in iconName and iconName[-3:] in ('png', 'svg', 'xpm'):
		iconName = iconName[:-4]
	icon_theme = gtk.icon_theme_get_default()
	try:
		pixbuf = icon_theme.load_icon(iconName, 24, 0)
		path = icon_theme.lookup_icon(iconName, 24, 0).get_filename()
	except:
		if iconName and '/' in iconName:
			try:
				pixbuf = gtk.gdk.pixbuf_new_from_file_at_size(iconName, 24, 24)
				path = iconName
			except:
				pass
		if pixbuf == None:
			if for_properties:
				return None, None

			iconName = 'application-default-icon'
			try:
				pixbuf = icon_theme.load_icon(iconName, 24, 0)
				path = icon_theme.lookup_icon(iconName, 24, 0).get_filename()
			except:
				return None
	if pixbuf == None:
		return None
	if pixbuf.get_width() != 24 or pixbuf.get_height() != 24:
		pixbuf = pixbuf.scale_simple(24, 24, gtk.gdk.INTERP_HYPER)
	if for_properties:
		return pixbuf, path
	return pixbuf
