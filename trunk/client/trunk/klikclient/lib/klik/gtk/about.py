#!/usr/bin/env python
# -*- coding: UTF-8 -*-

import gtk
import subprocess
import os

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
class KlikAbout(object):

	def __init__(self, klik):
		self.klik = klik

	def on_url(self, d, link, data):
		subprocess.Popen(["xdg-open", self.klik.settings.website])

	def show(self):
		dlg = gtk.AboutDialog()
		gtk.about_dialog_set_url_hook(self.on_url, None)
		dlg.set_version(self.klik.settings.version) # fixme (get this from proper location)
		dlg.set_name("klik")
		#icon = gtk.icon_theme_get_default().load_icon("application-x-application-cmg", 48, 0)
		icon = gtk.gdk.pixbuf_new_from_file(os.path.join(self.klik.sys_path , os.pardir, "share", "klik", "klik-window.png")) # fixme, use  gtk.window_set_default_icon_list()
		dlg.set_logo(icon)
		dlg.set_authors(self.klik.settings.authors)
		dlg.set_documenters(self.klik.settings.documenters)
		dlg.set_website(self.klik.settings.website)
		dlg.set_comments('''Download and run software\n virtualized without installation\nusing compressed application images\n\nFor help, please visit\n#klik on irc.freenode.net''')
		def close(w, res):
			if res == gtk.RESPONSE_CANCEL:
				w.destroy()
		dlg.connect("response", close)
		dlg.show()
	 
if __name__ == "__main__":
	ka = KlikAbout(None)
	ka.show()
