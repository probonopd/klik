#!/usr/bin/env python

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

# Python libs
import sys
import os
import tempfile
import traceback
import threading
import thread
import time
import string

# gtk libs
try:
	import pygtk
	import gtk
	import gtk.glade
	import pango
	import gobject
except:
	pass
# klick libs
from klik.base import KlikBase
from klik.utils.xdg import XdgUtils
from klik.exception import SafeException
from klik.exception import ExecuteException
from about import KlikAbout

#http://aruiz.typepad.com/siliconisland/2006/04/index.html
def threaded(f):
	def wrapper(*args):
		t = threading.Thread(target=f, args=args)
		t.setDaemon(True) # wont keep app alive
		t.start()
	return wrapper


class DownloadPrompt (object):

	def __init__(self, create):
		self.create = create

		self.window = self.create.wtree.get_widget("download_prompt")
		#self.window.set_title("")
		self.window.set_icon_from_file( os.path.join(self.create.klik.sys_path , os.pardir, "share", "klik", "klik-window.png") )
		
		self.label_run = self.create.wtree.get_widget("label_run")
		self.label_primary = self.create.wtree.get_widget("label_primary")
		self.box_trusted = self.create.wtree.get_widget("box_trusted")
		self.box_untrusted = self.create.wtree.get_widget("box_untrusted")
		self.textview_description = self.create.wtree.get_widget("textview_description")
		self.textview_sources = self.create.wtree.get_widget("textview_sources")
		self.button_view_recipe = self.create.wtree.get_widget("button_view_recipe")
		self.image_application_icon = self.create.wtree.get_widget("image_application_icon")
		self.textview_recipe = self.create.wtree.get_widget("textview_recipe")
		self.label_size = self.create.wtree.get_widget("label_size")
		self.label_summary = self.create.wtree.get_widget("label_summary")
		self.label_version = self.create.wtree.get_widget("label_version")
		self.label_secondary_trusted = self.create.wtree.get_widget("label_secondary_trusted")
		self.label_secondary_untrusted = self.create.wtree.get_widget("label_secondary_untrusted")
		self.label_signature = self.create.wtree.get_widget("label_signature")
		self.button_run = self.create.wtree.get_widget("button_run")
		self.combobox_jail_options = self.create.wtree.get_widget("combobox_jail_options")
		
		dic = {
			"on_download_prompt_destroy"   : self.on_cancel,
			"on_button_cancel_clicked"     : self.on_cancel,
			"on_button_run_clicked"        : self.on_button_run_clicked,
			"on_expander1_activate"        : self.on_expander1_activate,
			"on_button_about_clicked"      : self.on_button_about_clicked
		}
		self.create.wtree.signal_autoconnect(dic)
		
	def on_button_about_clicked(self, widget):
		ka = KlikAbout(self.create.klik)
		ka.show()
		
	def on_expander1_activate(self, widget, evt=None):
		# The expander effectively enables window resizable mode
		self.window.set_resizable(not widget.get_expanded())
		
	def setup(self):
		self.image_application_icon.set_from_file( os.path.join(self.create.klik.sys_path , os.pardir, "share", "klik", "klik.png") )

		self.label_primary.set_label( "<span size='larger' weight='bold'>%s</span>"
					      % _("Download and run \"%s\"?")
					      % self.create.recipe.name )

		self.textview_description.get_buffer().set_text(self.create.recipe.description)
		self.label_summary.set_text( self.create.recipe.summary )
		self.label_version.set_text( self.create.recipe.version )

		self.textview_recipe.get_buffer().set_text(self.create.recipe.original_xml)

		if self.create.recipe.size != None:
			self.label_size.set_text( self.create.recipe.size )

		self.window.set_title("klik - " + self.create.recipe.name)

		buf = self.textview_sources.get_buffer()
		buf.create_tag("bold", weight = pango.WEIGHT_BOLD)
		buf.create_tag("indent", indent = 12)
		buf.set_text("")

		buf.insert_with_tags_by_name(buf.get_start_iter(), _("Packages:\n"), "bold")
		for package in self.create.recipe.packages:
			buf.insert_with_tags_by_name(buf.get_end_iter(), "- %s\n" % package.href, "indent")

		(trusted, text) = self.create.recipe.is_trusted()
		if trusted:
			self.box_untrusted.hide()
			self.label_signature.set_markup( text.replace("<", "&lt;").replace(">", "&gt;"))
			self.button_run.grab_focus ()
		else:
			self.box_trusted.hide()
			self.label_secondary_untrusted.set_markup( "<b>%s</b>\n\n<i>%s</i>\n\n%s"
								   % (_("This application is NOT trusted"),
								      text,
								      _("You should be sure that applications are "
									"from a trustworthy source before running.")) )

		self.combobox_jail_options.set_active(self.create.klik.settings.default_jail_type)

	def on_cancel(self, widget):
		self.create.klik.clean_up()

	def on_button_run_clicked(self, widget):
		self.create.create_application_and_execute()


class FeedbackDialog (object): # Dialog that is shown after application runs first time

	def __init__(self, create):
		self.create = create

		self.window = self.create.wtree.get_widget("dialog_feedback")
		self.window.set_title("klik")
		self.window.set_icon_from_file( os.path.join(self.create.klik.sys_path , os.pardir, "share", "klik", "klik-window.png") )

		self.radiobutton_yes =  self.create.wtree.get_widget("radiobutton_yes")
		self.radiobutton_no =  self.create.wtree.get_widget("radiobutton_no")
		self.label_title =  self.create.wtree.get_widget("label_title")

		self.button_send = self.create.wtree.get_widget("button_send")
		self.button_cancel = self.create.wtree.get_widget("button_cancel")

		self.textview_system_information = self.create.wtree.get_widget("textview_system_information")
		self.textview_dependencies = self.create.wtree.get_widget("textview_dependencies")
		self.textview_error_report = self.create.wtree.get_widget("textview_error_report")
		self.textview_user_comment = self.create.wtree.get_widget("textview_user_comment")
		
		dic = {
			"on_dialog_feedback_destroy"		: self.on_cancel,
			"on_button_send_clicked"		: self.on_button_send_clicked,
			"on_button_cancel_feedback_clicked"	: self.on_cancel,
			"on_button_about_feedback_clicked"	: self.on_button_about_clicked
		}
		self.create.wtree.signal_autoconnect(dic)
		
		self.recipe = None
		self.result = False
		self.error_text = ""
		self.lib_report = ""

	def on_button_about_clicked(self, widget):
		ka = KlikAbout(self.create.klik)
		ka.show()

	def update_display(self, recipe, result, error_text, lib_report):		
		self.label_title.set_label("<span size='larger' weight='bold'>%s</span>" % recipe.name)
		self.radiobutton_yes.set_active(result == 0)
		self.radiobutton_no.set_active(result != 0)

		buf = self.textview_system_information.get_buffer()
		buf.set_text( self.create.klik.system_info() )

		buf = self.textview_dependencies.get_buffer()
		buf.set_text( lib_report )

		buf = self.textview_error_report.get_buffer()
		buf.set_text( error_text )
		
		self.error_text = error_text
		self.lib_report = lib_report
		
		self.window.set_title("klik - " + recipe.name)

	def on_button_send_clicked(self, widget):
		self.start_postback()

	@threaded
	def start_postback(self):
		gtk.gdk.threads_enter()
		self.window.hide()
		self.result = self.radiobutton_yes.get_active()
		#self.create.progress_dialog.label_progress_title
		self.create.progress_dialog.setup_for_feedback_progress()
		self.create.progress_dialog.window.show()
		gtk.gdk.threads_leave()
		buffer = self.textview_user_comment.get_buffer()
		self.create.klik.post_feedback(	self.create.recipe, \
						self.result, \
						self.error_text, \
						self.lib_report, \
						"", \
						buffer.get_text( buffer.get_start_iter(), buffer.get_end_iter() ) \
						)

		gtk.main_quit()

	def on_cancel(self, widget):
		self.create.quit()

class ProgressDialog (object): # Dialog that is shown while recipe is being downloaded

	def __init__(self, create):
		self.create = create
		self.create.klik.events.on_download_progress += self.__on_progress_update
		self.__pulse_flag = True

		self.window = self.create.wtree.get_widget("progress_dialog")
		self.window.set_title("klik")
		self.window.set_icon_from_file( os.path.join(self.create.klik.sys_path , os.pardir, "share", "klik", "klik-window.png") )
		self.label_progress_title = self.create.wtree.get_widget("label_progress_title")
		self.progressbar = self.create.wtree.get_widget("progressbar")
		self.button_cancel = self.create.wtree.get_widget("button_cancel")
		self.image_application_icon = self.create.wtree.get_widget("image_application_icon")
		self.image_application_icon.set_from_file( os.path.join(self.create.klik.sys_path , os.pardir, "share", "klik", "klik.png") )
		self.expander_detail = self.create.wtree.get_widget("expander_detail")
		self.textview_detail = self.create.wtree.get_widget("textview_detail")

		self.label_progress_title.set_label( "<span size='larger' weight='bold'>%s</span>"
						     % _("Getting information...") )
		self.progressbar.set_pulse_step(0.05)

		dic = {
			"on_button_cancel_progress_clicked"     : self.on_cancel,
			"on_progress_expander_detail_activate"	: self.on_progress_expander_detail_activate
		}
		self.create.wtree.signal_autoconnect(dic)

		# Change the color of the progress window details pane
		self.textview_detail.modify_base(gtk.STATE_NORMAL, gtk.gdk.Color(0,0,0));
		self.textview_detail.modify_text(gtk.STATE_NORMAL, gtk.gdk.Color(65535,65535,65535));

	def on_progress_expander_detail_activate(self, widget):
		return

	def set_window_title(self, title):
		self.window.set_title(title)

	def add_details_line(self, text):
		gtk.gdk.threads_enter()
		buffer = self.textview_detail.get_buffer()
		buffer.insert(buffer.get_end_iter(), text)
		self.textview_detail.scroll_to_iter(buffer.get_end_iter(), 0.0, True, 0.0, 0.5)
		gtk.gdk.threads_leave()

	def on_cancel(self, widget):
		gtk.main_quit()

	def start_pulse(self):
		self.__pulse_flag = True
		self.__pulse()

	def end_pulse(self):
		self.__pulse_flag = False

	@threaded
	def __pulse(self):
		while self.__pulse_flag == True:
			gtk.gdk.threads_enter()
			self.progressbar.pulse()
			gtk.gdk.threads_leave()
			time.sleep(0.1)


	def setup_for_download_progress(self):
		self.window.set_skip_taskbar_hint(False)
		self.window.set_title("klik")
		self.label_progress_title.set_label( "<span size='larger' weight='bold'>%s</span>"
						     % _("Downloading %s")
						     % self.create.recipe.name )
		self.progressbar.set_fraction(0.0)
		self.progressbar.set_text(" ")
		self.expander_detail.show()

	def setup_for_feedback_progress(self):
		self.window.set_skip_taskbar_hint(False)
		self.window.set_title("klik")
		self.label_progress_title.set_label( "<span size='larger' weight='bold'>%s</span>"
						     % _("Sending Feedback") )
		self.progressbar.set_fraction(0.0)
		self.progressbar.set_text(" ")
		self.expander_detail.hide()

	def __on_progress_update(self, percentage, text):

		if percentage == -1:
			self.progressbar.set_text(text)
			self.start_pulse();
		else:
			self.end_pulse()
			if percentage > 1:
				percentage = 1

			gtk.gdk.threads_enter()
			self.progressbar.set_fraction(percentage)
			self.progressbar.set_text(text)
			gtk.gdk.flush()
			gtk.gdk.threads_leave()


class KlikGtkCreate (object):

	def __init__(self, klikbase):

		# Objects we will use
		self.klik = klikbase
		self.xdg = XdgUtils()

		self.args = None
		self.recipe = None

		# Current install status
		# 0 - not started, 1 - downloading, 2 - compiling cmg, 3 -complete
		self.install_status = 0

		# Load glade file
		self.gladefile = os.path.join(self.klik.sys_path , os.pardir, "share", "klik", "klik.glade")

		self.wtree = gtk.glade.XML(self.gladefile)
		self.window = self.wtree.get_widget("download_prompt")
		self.window.set_icon_from_file( os.path.join(self.klik.sys_path , os.pardir, "share", "klik", "klik-window.png") )
		dic = {
			"on_button_cancel_clicked"   : self.on_button_cancel_clicked,
			"on_download_prompt_destroy" : self.on_download_prompt_destroy
		}
		self.wtree.signal_autoconnect(dic)


		# Initialize download prompt
		self.download_prompt = DownloadPrompt(self)
		self.progress_dialog = ProgressDialog(self)
		self.feedback_dialog = FeedbackDialog(self)

	def on_button_cancel_clicked(self, widget):
		gtk.main_quit()

	def on_download_prompt_destroy(self, widget):
		gtk.main_quit()

	def quit(self):
		gobject.idle_add(self.progress_dialog.window.destroy)
		gobject.idle_add(self.feedback_dialog.window.destroy)
		gobject.idle_add(self.download_prompt.window.destroy)
		self.klik.clean_up()

	def download_and_execute_recipe(self, recipe, args):
		self.app_name = recipe.name
		self.args = args
		self.recipe = recipe

		gtk.gdk.threads_init()
		self.download_and_execute()
		try:
			gtk.main()
			self.quit()
		except:
			# catch exits
			pass
		


	def download_and_execute_application(self, app_name, args):
		self.app_name = app_name
		self.args = args

		gtk.gdk.threads_init()
		try:
			self.download_recipe()
			gtk.main()
			self.quit()
		except Exception, inst:
			self.error_alert(inst, self.progress_dialog.window)

	def download_and_execute(self):
		gtk.gdk.threads_enter()
		self.download_prompt.setup()
		self.download_prompt.window.show()
		gtk.gdk.threads_leave()


	@threaded
	def create_application_and_execute(self):
	
		result = False
		error = ""
	
		gtk.gdk.threads_enter()
		self.progress_dialog.setup_for_download_progress()
		self.download_prompt.window.hide()
		self.progress_dialog.set_window_title("klik - " + self.app_name)
		self.progress_dialog.window.show()
		gtk.gdk.flush()
		gtk.gdk.threads_leave()

		# Create the CMG
		try:
			self.klik.events.print_to_stdout += self.progress_dialog.add_details_line
			path = self.klik.create_cmg( self.recipe )

		except Exception, inst:
			self.error_alert(inst, self.progress_dialog.window)
			self.quit()
			return
		
		try:
			# Hide the install window
			gtk.gdk.threads_enter()
			self.progress_dialog.window.hide()
			gtk.gdk.flush() # force it to happen now!
			gtk.gdk.threads_leave()

			# Create jail
			jail_option = self.download_prompt.combobox_jail_options.get_active()
			
			if jail_option == 2:
				self.klik.create_jailed_cmg(path, "portable")
			elif jail_option == 1:
				self.klik.create_jailed_cmg(path, "home")
			#elif jail_option == 1:
			#	self.klik.create_jailed_cmg(path, "data")

			# Execute the application
			if path != None:
				print ""
				print "Starting Application..."
				
				gtk.gdk.threads_enter()
				# this needs to happen on main thread in case multi dialogue is needed
				result, error = self.klik.execute_cmg(None, path, self.args, True)
				gtk.gdk.threads_leave()
				
				# Get the libraries
				lib_report = self.klik.check_for_missing_library(None, path)
				print lib_report
				
				# Show the feedback form
				gtk.gdk.threads_enter()
				self.feedback_dialog.update_display(self.recipe, result, error, lib_report)
				self.feedback_dialog.window.show()
				gtk.gdk.threads_leave()


		except Exception, inst:
			self.error_alert(inst, self.progress_dialog.window)
			self.quit()

	def error_alert(self, error_inst, window=None):

		gtk.gdk.threads_enter()
		
		text = "<b>%s</b>\n\n%s" % (_("Error Occurred"), str(error_inst))
		text = string.replace(text, "\\n","\n")
		print ""

		if isinstance(error_inst, SafeException) == False and not isinstance(error_inst ,ExecuteException):
			# dump trace to screen
			traceback.print_exc()
			gtk.gdk.threads_leave()
		else:
			print >> sys.stderr, error_inst

			# GTK error window
			error_dlg = gtk.MessageDialog(type=gtk.MESSAGE_ERROR
				, message_format=("")
				, buttons=gtk.BUTTONS_CANCEL)
			error_dlg.set_modal(True)
			error_dlg.set_transient_for(window)
			error_dlg.set_markup(str(text))
			error_dlg.run()
			error_dlg.destroy()
			gtk.gdk.threads_leave()


		if isinstance(error_inst ,ExecuteException):
			# error occured while executing trigger feedback
			# Show the feedback
			gtk.gdk.threads_enter()
			self.feedback_dialog.update_display(self.recipe, 1, str(error_inst), "")
			self.feedback_dialog.window.show()
			gtk.gdk.threads_leave()
		else:
			gobject.idle_add(self.quit)

	@threaded
	def download_recipe(self):
		gtk.gdk.threads_enter()
		self.progress_dialog.set_window_title("klik - " + self.app_name)
		self.progress_dialog.window.show_all()
		self.progress_dialog.start_pulse()
		gtk.gdk.threads_leave()

		try:
			# Get recipe
			self.recipe = self.klik.get_recipe( self.app_name )

			gtk.gdk.threads_enter()
			self.progress_dialog.end_pulse()
			self.progress_dialog.window.hide()
			gtk.gdk.threads_leave()

			self.download_and_execute()

		except Exception, inst:
			self.error_alert(inst, self.progress_dialog.window)
			return

