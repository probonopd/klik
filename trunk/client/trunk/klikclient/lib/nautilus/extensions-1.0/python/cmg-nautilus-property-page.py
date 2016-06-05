import md5
import urllib
import sys
import gtk
import nautilus
import traceback

# Add the klik modules
sys.path.insert(1, "/opt/klik/lib")
from klik.base import KlikBase

class CMGPropertyPage(nautilus.PropertyPageProvider):
	def __init__(self):
		pass

	def get_property_pages(self, files):
	
		if len(files) != 1:
			return

		file = files[0]
		
		if file.get_uri_scheme() != 'file': return
		if file.is_directory():	return
		if not file.is_mime_type('application/x-application-cmg'): return

		filename = urllib.unquote(file.get_uri()[7:])

		klik = KlikBase()
		recipe = klik.extract_recipe(filename)

		self.property_label = gtk.Label('Application')
		self.property_label.show()

		self.vbox = gtk.VBox(False, 12)
		self.vbox.set_border_width(12)

		info = []
		info.append(('Application', recipe.name))
		info.append(('Summary', recipe.summary))
		info.append(('Version', recipe.version))
		info.append(('Description', recipe.description))
		info.append(('Original Source', recipe.source_uri))
		
		trusted, signer = recipe.is_trusted()
		if trusted:
			info.append(('Trusted', signer))
		else:
			info.append(('Trusted', "NOT TRUSTED"))
			
		self.vbox.pack_start(
			self.create_frame('Compressed Application Image', self.create_info_table(info)), False, False)

		self.vbox.show_all()

		return nautilus.PropertyPage("NautilusPython::cmg", self.property_label, self.vbox),
	

			
	def create_frame(self, labeltext, view, xscale=0.0, yscale=0.0):
		frame = gtk.Frame()
		frame.set_shadow_type(gtk.SHADOW_NONE)

		label = gtk.Label()
		label.set_use_markup(True)
		label.set_markup('<b>%s</b>' % labeltext)
		frame.set_label_widget(label)

		alignment = gtk.Alignment(xscale=xscale, yscale=yscale)
		alignment.set_padding(0, 0, 12, 0)
		alignment.add(view)
		view.set_border_width(6)

		frame.add(alignment)
		return frame
		
	def create_info_table(self, rows):
		table = gtk.Table(rows=len(rows), columns=2)
		table.set_row_spacings(6)
		table.set_col_spacings(12)

		i = 0
		for row in rows:
			label = gtk.Label('%s:' % row[0])
			alignment = gtk.Alignment()
			alignment.add(label)
			table.attach(alignment, 0, 1, i, i+1, gtk.FILL)

			data = gtk.Label(row[1])
			data.set_line_wrap(True)
			alignment = gtk.Alignment()
			alignment.add(data)
			table.attach(alignment, 1, 2, i, i+1, gtk.FILL)

			i += 1
		return table
