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
from klik.exception import SafeException
from klik.exception import ExecuteException

from about import KlikAbout

# Turn on threading
gtk.gdk.threads_init()

#http://aruiz.typepad.com/siliconisland/2006/04/index.html
def threaded(f):
	def wrapper(*args):
		t = threading.Thread(target=f, args=args)
		t.setDaemon(True) # wont keep app alive
		t.start()
	return wrapper

class KlikGtkCore (object):

	def __init__(self, klikbase):

		# Objects we will use
		self.klik = klikbase

		# Glade file
		self.gladefile = os.path.join(self.klik.sys_path , os.pardir, "share", "klik", "klik.glade")

		self.app_name = None		# application to get recipe for
		self.recipe = None		# recipe to use
		self.error = None		# last error
		self.cmg = None			# created cmg

	def download_and_execute_recipe(self, recipe, args):
		self.app_name = recipe.name
		self.args = args
		self.recipe = recipe

		self.create_application_and_execute()

	def download_and_execute_application(self, app_name, args):
		self.app_name = app_name
		self.args = args

		progress = ProgressDialog(self)
		progress.set_window_title("klik - " + self.app_name)
		print gtk.main_level()
		# grab recipe on another thread
		@threaded
		def download_recipe_threaded(progress):

			# Get recipe
			try:
				self.recipe = self.klik.get_recipe( self.app_name )

				gtk.gdk.threads_enter()
				progress.destroy()
				gtk.gdk.threads_leave()

			except Exception, inst:
				self.error = inst
				gtk.gdk.threads_enter()
				progress.destroy()
				gtk.gdk.threads_leave()

		# Start recipe download
		download_recipe_threaded(progress)

		# Show progress window
		progress.main()

		if self.error:
			self.error_alert(self.error)
			return

		# Should now have a recipe, display prompt
		if self.recipe:
			self.create_application_and_execute()

	def create_application_and_execute(self):

		prompt = DownloadPrompt(self)
		result = prompt.main()

		if result:

			progress = ProgressDialog(self)
			progress.setup_for_download_progress()
			progress.set_window_title("klik - " + self.recipe.name)
			self.klik.events.print_to_stdout += progress.add_details_line

			# Create cmg on a seprate thread
			@threaded
			def create_cmg_threaded(progress):
				try:
					self.cmg = self.klik.create_cmg( self.recipe )

					gtk.gdk.threads_enter()
					progress.destroy()
					gtk.gdk.threads_leave()

				except Exception, inst:
					self.error = inst
					gtk.gdk.threads_enter()
					progress.destroy()
					gtk.gdk.threads_leave()

			# Start cmg creation
			create_cmg_threaded(progress)

			# Show progress window
			progress.main()

			# process error
			if self.error:
				self.error_alert(self.error)
				return

			# There should now be a cmg!
			if self.cmg:
				self.execute_cmg()

	def execute_cmg(self):

		# Execute the application
		print ""
		print "Starting Application..."

		# this needs to happen on main thread in case multi dialogue is needed
		result, error = self.cmg.execute(self.args, capture_error=True)
		lib_report = ""

		# Get the libraries
		lib_report = self.cmg.check_for_missing_library(self.cmg.last_command)
		print ""
		print "Tracing Application Libraries..."
		print lib_report

		# Prompt user for feedback
		feedback = FeedbackDialog(self)
		feedback.update_display(self.recipe, result, error, lib_report)
		result = feedback.main()

		if result:
			self.send_feedback(result[0], result[1], error, lib_report)

	def send_feedback(self, result, text, error, lib_report):

		progress = ProgressDialog(self)
		progress.setup_for_feedback_progress()
		progress.set_window_title("klik - " + self.cmg.recipe.name)

		# Create cmg on a seprate thread
		@threaded
		def send_feedback_threaded(progress):
			try:
				self.klik.post_feedback(self.recipe, \
						result, \
						error, \
						lib_report, \
						"", \
						text \
						)

				gtk.gdk.threads_enter()
				progress.destroy()
				gtk.gdk.threads_leave()

			except Exception, inst:
				self.error = inst
				gtk.gdk.threads_enter()
				progress.destroy()
				gtk.gdk.threads_leave()


		# Start cmg creation
		send_feedback_threaded(progress)

		# Show progress window
		progress.main()

		if self.error:
			self.error_alert(self.error)
			return

	def error_alert(self, error_inst, window=None):

		text = "<b>%s</b>\n\n%s" % (_("Error Occurred"), str(error_inst))
		text = string.replace(text, "\\n","\n")
		print ""

		if isinstance(error_inst, SafeException) == False and not isinstance(error_inst ,ExecuteException):
			# dump trace to screen
			traceback.print_exc()
		else:
			print >> sys.stderr, error_inst
			# GTK error window
			error_dlg = gtk.MessageDialog(type=gtk.MESSAGE_ERROR
				, message_format=("")
				, buttons=gtk.BUTTONS_CANCEL)
			error_dlg.set_modal(True)
			#error_dlg.set_transient_for(window)
			error_dlg.set_markup(str(text))
			error_dlg.run()
			error_dlg.destroy()

		if isinstance(error_inst, ExecuteException):
			# error occured while executing trigger feedback
			# Show the feedback
			# Prompt user for feedback
			feedback = FeedbackDialog(self)
			feedback.update_display(self.recipe, False, str(error_inst), "")
			result = feedback.main()

			if result:
				self.send_feedback(cmg, result[0], result[1], error, lib_report)

############################################################################################################
class ProgressDialog (object): # Dialog that is shown while recipe is being downloaded

	def __init__(self, core):
		self.core = core
		self.__pulse_flag = True

		wtree = gtk.glade.XML(core.gladefile)
		self.window = wtree.get_widget("progress_dialog")
		self.window.set_title("klik")
		self.window.set_icon_from_file( os.path.join(self.core.klik.sys_path , os.pardir, "share", "klik", "klik-window.png") )
		self.label_progress_title = wtree.get_widget("label_progress_title")
		self.progressbar = wtree.get_widget("progressbar")
		self.button_cancel = wtree.get_widget("button_cancel")
		self.image_application_icon = wtree.get_widget("image_application_icon")
		self.image_application_icon.set_from_file( os.path.join(self.core.klik.sys_path , os.pardir, "share", "klik", "klik.png") )
		self.expander_detail = wtree.get_widget("expander_detail")
		self.textview_detail = wtree.get_widget("textview_detail")

		self.label_progress_title.set_label("<span size='larger' weight='bold'>%s</span>"
						     % _("Getting information...") )
		self.progressbar.set_pulse_step(0.05)

		dic = {
			"on_button_cancel_clicked"     : self.on_cancel
		}
		wtree.signal_autoconnect(dic)
		self.window.connect("destroy", self.on_destroy)

		# Change the color of the progress window details pane
		self.textview_detail.modify_base(gtk.STATE_NORMAL, gtk.gdk.Color(0,0,0));
		self.textview_detail.modify_text(gtk.STATE_NORMAL, gtk.gdk.Color(65535,65535,65535));

		core.klik.events.on_download_progress += self.__on_progress_update

	def main(self):
		self.window.show()
		self.start_pulse()
		gtk.main()

	def destroy(self):
		self.window.destroy()
		return True

	def on_destroy(self, widget, data=None):
		gtk.main_quit()


	def set_window_title(self, title):
		self.window.set_title(title)

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

	def add_details_line(self, text):
		gtk.gdk.threads_enter()
		self.progressbar.set_sensitive(False) # Disable while updating else we segfault!
		buffer = self.textview_detail.get_buffer()
		buffer.insert(buffer.get_end_iter(), text)
		self.textview_detail.scroll_to_iter(buffer.get_end_iter(), 0.0, True, 0.0, 0.5)
		self.progressbar.set_sensitive(True)
		gtk.gdk.threads_leave()

	def __on_progress_update(self, percentage, text):
		gtk.gdk.threads_enter()
		if percentage == -1:
			self.progressbar.set_text(text)
			self.start_pulse();
		else:
			self.end_pulse()
			if percentage > 1:
				percentage = 1

			self.progressbar.set_fraction(percentage)
			self.progressbar.set_text(text)

		gtk.gdk.flush()
		gtk.gdk.threads_leave()

	#### SPECIAL CASES #####
	def setup_for_download_progress(self):
		self.window.set_skip_taskbar_hint(False)
		self.window.set_title("klik")
		self.label_progress_title.set_label( "<span size='larger' weight='bold'>%s</span>"
						     % _("Downloading %s")
						     % self.core.recipe.name )
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

############################################################################################################
class FeedbackDialog (object): # Dialog that is shown after application runs first time

	def __init__(self, core):

		self.core = core
		self.return_value = None

		wtree = gtk.glade.XML(core.gladefile)
		self.window = wtree.get_widget("dialog_feedback")
		self.window.set_title("klik")
		self.window.set_icon_from_file( os.path.join(core.klik.sys_path , os.pardir, "share", "klik", "klik-window.png") )

		self.radiobutton_yes =  wtree.get_widget("radiobutton_yes")
		self.radiobutton_no =  wtree.get_widget("radiobutton_no")
		self.label_title =  wtree.get_widget("label_title")

		self.button_send = wtree.get_widget("button_send")
		self.button_cancel = wtree.get_widget("button_cancel")

		self.textview_system_information = wtree.get_widget("textview_system_information")
		self.textview_dependencies = wtree.get_widget("textview_dependencies")
		self.textview_error_report = wtree.get_widget("textview_error_report")
		self.textview_user_comment = wtree.get_widget("textview_user_comment")

		# Events
		dic = {
			"on_button_send_clicked"		: self.on_button_send_clicked,
			"on_button_about_feedback_clicked"	: self.on_button_about_clicked,
			"on_button_cancel_feedback_clicked"	: self.on_cancel
		}
		wtree.signal_autoconnect(dic)
		self.window.connect("destroy", self.on_destroy)

	def update_display(self, recipe, result, error_text, lib_report):
		self.label_title.set_label("<span size='larger' weight='bold'>%s</span>" % recipe.name)
		self.radiobutton_yes.set_active(result == 0)
		self.radiobutton_no.set_active(result != 0)

		buf = self.textview_system_information.get_buffer()
		buf.set_text( self.core.klik.system_info() )

		buf = self.textview_dependencies.get_buffer()
		buf.set_text( lib_report )

		buf = self.textview_error_report.get_buffer()
		buf.set_text( error_text )

		self.error_text = error_text
		self.lib_report = lib_report

		self.window.set_title("klik - " + recipe.name)

	def on_button_about_clicked(self, widget):
		ka = KlikAbout(self.core.klik)
		ka.show()

	def on_button_send_clicked(self, widget):
		# Set result
		buffer = self.textview_user_comment.get_buffer()
		text = buffer.get_text( buffer.get_start_iter(), buffer.get_end_iter() )
		self.return_value = (self.radiobutton_yes.get_active(), text)

		# Close window
		self.window.destroy()

	def on_cancel(self, widget):
		# Set result
		self.return_value = None

		# Close window
		self.window.destroy()
		gtk.main_quit()

	def destroy(self):
		self.window.destroy()

	def on_destroy(self, widget, data=None):
		gtk.main_quit()

	def main(self):
		self.window.show()
		gtk.main()
		return self.return_value

############################################################################################################
class DownloadPrompt (object):

	def __init__(self, core):
		self.core = core
		self.return_value = False

		wtree = gtk.glade.XML(core.gladefile)
		self.window = wtree.get_widget("download_prompt")
		self.window.set_icon_from_file( os.path.join(self.core.klik.sys_path , os.pardir, "share", "klik", "klik-window.png") )

		self.label_run = wtree.get_widget("label_run")
		self.label_primary = wtree.get_widget("label_primary")
		self.box_trusted = wtree.get_widget("box_trusted")
		self.box_untrusted = wtree.get_widget("box_untrusted")
		self.textview_description = wtree.get_widget("textview_description")
		self.textview_sources = wtree.get_widget("textview_sources")
		self.button_view_recipe = wtree.get_widget("button_view_recipe")
		self.image_application_icon = wtree.get_widget("image_application_icon")
		self.textview_recipe = wtree.get_widget("textview_recipe")
		self.label_size = wtree.get_widget("label_size")
		self.label_summary = wtree.get_widget("label_summary")
		self.label_version = wtree.get_widget("label_version")
		self.label_secondary_trusted = wtree.get_widget("label_secondary_trusted")
		self.label_secondary_untrusted = wtree.get_widget("label_secondary_untrusted")
		self.label_signature = wtree.get_widget("label_signature")
		self.button_run = wtree.get_widget("button_run")
		self.combobox_jail_options = wtree.get_widget("combobox_jail_options")

		dic = {
			"on_download_prompt_destroy"   : self.on_cancel,
			"on_button_cancel_clicked"     : self.on_cancel,
			"on_button_run_clicked"        : self.on_button_run_clicked,
			"on_expander1_activate"        : self.on_expander1_activate,
			"on_button_about_clicked"      : self.on_button_about_clicked
		}
		wtree.signal_autoconnect(dic)
		self.window.connect("destroy", self.on_destroy)

	def destroy(self):
		self.window.destroy()

	def on_destroy(self, widget, data=None):
		gtk.main_quit()

	def on_button_about_clicked(self, widget):
		ka = KlikAbout(self.core.klik)
		ka.show()

	def on_expander1_activate(self, widget, evt=None):
		# The expander effectively enables window resizable mode
		self.window.set_resizable(not widget.get_expanded())

	def main(self):
		#self.image_application_icon.set_from_file( os.path.join(self.core.klik.sys_path , os.pardir, "share", "klik", "klik.png") )

		self.label_primary.set_label( "<span size='larger' weight='bold'>%s</span>"
					      % _("Download and run \"%s\"?")
					      % self.core.recipe.name )

		self.textview_description.get_buffer().set_text(self.core.recipe.description)
		self.label_summary.set_text( self.core.recipe.summary )
		self.label_version.set_text( self.core.recipe.version )

		self.textview_recipe.get_buffer().set_text(self.core.recipe.original_xml)

		if self.core.recipe.size != None:
			self.label_size.set_text( self.core.recipe.size )

		self.window.set_title("klik - " + self.core.recipe.name)

		buf = self.textview_sources.get_buffer()
		buf.create_tag("bold", weight = pango.WEIGHT_BOLD)
		buf.create_tag("indent", indent = 12)
		buf.set_text("")

		buf.insert_with_tags_by_name(buf.get_start_iter(), _("Packages:\n"), "bold")
		for package in self.core.recipe.packages:
			buf.insert_with_tags_by_name(buf.get_end_iter(), "- %s\n" % package.href, "indent")

		(trusted, text) = self.core.recipe.is_trusted()
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

		self.combobox_jail_options.set_active(self.core.klik.settings.default_jail_type)

		self.window.show()
		gtk.main()
		return self.return_value

	def on_cancel(self, widget):
		self.window.destroy()
		gtk.main_quit()

	def on_button_run_clicked(self, widget):
		self.return_value = True
		self.destroy()
		gtk.main_quit()
