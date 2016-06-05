# About plural forms: http://www.gnu.org/software/gettext/manual/html_mono/gettext.html#Plural-forms
# Python + Glade: http://www.learningpython.com/2006/12/03/translating-your-pythonpygtk-application/

# Creating a new language:
#   make update_pot
#   msginit -l es (changing 'es' with your language's locale)
#   If your language has non-ASCII characters, change the 'charset' field in the header to UTF-8
#
# Update po files, compile them and generate statistics
#   make
#
# Install message catalogs
#   make install

PY_SOURCE:=$(shell find ../{bin,lib} -iname "[_a-z]*.py" -print)
GLADE_SOURCE=../share/klik/klik.glade
GLADE_HEADER=./tmp/klik.glade.h

POT_FILE=klikclient.pot
PO_FILES:=$(wildcard *.po)
MO_FILES:=$(shell find . -iname "*.mo")

prefix=/usr/local/share/
DESTDIR=/

MSGFMT=msgfmt
#MSGFMT=msgfmt.py
STATSFILE=statistics.txt

all:
	rm -f $(STATSFILE)
	make $(PO_FILES)

.PHONY: *.po
*.po:
	@install -d $(CURDIR)/locale/$(basename $@)/LC_MESSAGES/
	@echo -n "$(basename $@) - " | tee -a $(STATSFILE)
	@$(MSGFMT) -cvo $(CURDIR)/locale/$(basename $@)/LC_MESSAGES/klikclient.mo $@ 2>&1 | tee -a $(STATSFILE)
	@$(MSGFMT) -C $@ &>$(basename $@).errors || exit 0

update_pot: $(PY_SOURCE) $(GLADE_SOURCE)
# Generate .pot file
	xgettext -o $(POT_FILE) $(PY_SOURCE) $(GLADE_SOURCE)
	@#pygettext.py -k_ -kn_ -o $(pot_file) $(source_py)
	@#intltool-extract -l --type=gettext/glade $(glade_source)
	@#xgettext -k_ -kN_ -o - --omit-header $(glade_header) >> $(pot_file)
# Update .po files
	for po in $(PO_FILES); do msgmerge -U $${po} $(POT_FILE); done

install:
	cp -a $(CURDIR)/locale $(DESTDIR)/${prefix}/share/

clean:
	rm -rf $(GLADE_HEADER) *~ tmp locale
