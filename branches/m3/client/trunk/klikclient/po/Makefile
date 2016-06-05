# About plural forms: http://www.gnu.org/software/gettext/manual/html_mono/gettext.html#Plural-forms
# Python + Glade: http://www.learningpython.com/2006/12/03/translating-your-pythonpygtk-application/

# Creating a new language:
#   make pot
#   msginit -l es (changing 'es' with your language's locale)
#   If your language has non-ASCII characters, change the 'charset' field in the header to UTF-8
#
# Updating po files
#   make
#
# Compiling and installing po files in the system (as root)
#   make install

source_py=`find ../../../ -iname "[_a-z]*.py" -print`
glade_source=../share/klik/klik.glade
glade_header=./tmp/klik.glade.h
pot_file=klikclient.pot

COMPILER=msgfmt
#COMPILER=msgfmt.py

all: pot
	for po in *.po; do \
		msgmerge -U $${po} ${pot_file}; \
	done

pot:
	xgettext -o ${pot_file} ${source_py} ${glade_source}
	#pygettext.py -k_ -kn_ -o ${pot_file} ${source_py}
	#echo -e "\n### Glade interface\n\n" >> ${pot_file}
	#intltool-extract -l --type=gettext/glade ${glade_source}
	#xgettext -k_ -kN_ -o - --omit-header ${glade_header} >> ${pot_file}

install:
	for po in *.po; do \
		${COMPILER} -o /usr/share/locale/$${po%.*}/LC_MESSAGES/klikclient.mo $${po}; \
	done

check:
	${COMPILER} -cv *.po

clean:
	rm -rf ${pot_file} ${glade_header} *~ ./tmp
