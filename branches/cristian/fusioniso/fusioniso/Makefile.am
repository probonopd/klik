SUBDIRS = linux man src
EXTRA_DIST = fusioniso.spec


tags:
	ctags -R .

cscope:
	cscope -R -b

vim: tags cscope

CLEANFILES = ctags cscope.out


extraclean: maintainer-clean
	rm -rf aclocal.m4 autom4te.cache autoscan.log configure.scan \
	config.h.in* configure depcomp install-sh Makefile.in \
	config.guess config.sub \
	ltmain.sh missing `find . -name Makefile.in` \
	fusioniso-*.tar.gz \
	fusioniso-*.tar.bz2
