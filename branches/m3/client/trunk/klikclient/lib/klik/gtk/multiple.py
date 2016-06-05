#!/usr/bin/env python
# -*- coding: UTF-8 -*-

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
	Simple dialogue that returns a selected desktop object
"""


# Python libs
import sys
import os
import tempfile
import traceback
import threading
import thread
import time
import string
import xml.sax.saxutils

# gtk libs

import pygtk
pygtk.require("2.0")

import gtk
import gtk.glade
import gobject
import utils



class MultipleApplications( object ):

	def __init__(self, klik):

		self.klik = klik

		# Load glade file
		self.gladefile = os.path.join(sys.path[0] , os.pardir, "share", "klik", "klik.glade")

		self.wtree = gtk.glade.XML(self.gladefile)
		self.window = self.wtree.get_widget("multiple_applications")
		self.window.set_icon_from_file( os.path.join(sys.path[0] , os.pardir, "share", "klik", "klik-window.png") )
		dic = {
			"on_button_ok_clicked" : self.on_button_ok_clicked,
			"on_button_close_clicked"   : self.on_button_close_clicked
		}
		self.wtree.signal_autoconnect(dic)

		self.setup_item_tree()

		self.result = None

	def quit(self):
		self.window.destroy()
		gtk.main_quit()

	def on_button_ok_clicked(self, widget):
		items = self.wtree.get_widget('item_tree')

		model, iter = items.get_selection().get_selected()
		if not iter: return
		desktop = model.get_value(iter, 2)
		self.result = desktop
		self.quit()

	def on_button_close_clicked(self, widget):
		gobject.idle_add(self.quit)

	def load_items(self, desktop_files):
		self.load_items(desktop_files);

	def setup_item_tree(self):
		items = self.wtree.get_widget('item_tree')

		# Check box
		#column = gtk.TreeViewColumn('Select')
		#cell = gtk.CellRendererToggle()
		#cell.connect('toggled', self.on_item_tree_show_toggled)
		#column.pack_start(cell, True)
		#column.set_attributes(cell, active=0)
		#items.append_column(column)

		# Item Col
		column = gtk.TreeViewColumn(('Item'))
		column.set_spacing(4)
		cell = gtk.CellRendererPixbuf()
		#cell.connect('click', self.on_item_tree_show_toggled)
		column.pack_start(cell, False)
		column.set_attributes(cell, pixbuf=0)
		cell = gtk.CellRendererText()
		#
		#cell.set_fixed_size(-1, 25)
		column.pack_start(cell, True)
		column.set_attributes(cell, markup=1)
		items.append_column(column)

		self.item_store = gtk.ListStore(gtk.gdk.Pixbuf, str, object)

		items.set_model(self.item_store)

	def load_items(self, desktop_files):
		self.item_store.clear()
		for item in desktop_files:

			name = item.get("Name").strip()
			if item.get("Comment") != None:
				name = xml.sax.saxutils.escape(name) + '\n<small><i>' + xml.sax.saxutils.escape(item.get("Comment").strip()) + '</i></small>'
			else:
				name = xml.sax.saxutils.escape(name) + '\n<small><i></i></small>'

			icon = self.klik.scan_cmg_icons(item.get("X-CMG"), item)[0]
			
			if icon[0] == "/":
				temp_path = tempfile.mktemp()
				self.klik.extract_file( item.get("X-CMG"), icon, temp_path)
				icon = temp_path
				
			pixbuf = utils.getIcon(icon)

			if pixbuf == None:
				pixbuf = utils.getIcon("application-default-icon")
			
			if name != '':
				self.item_store.append((pixbuf, name, item))

		self.window.show()
		
		gtk.main()

		return self.result

