#!/usr/bin/make -f

VERSION = $(shell awk '{print $$2; exit}' debian/changelog | sed 's|(||;s|)||')
pkgname = klik-$(VERSION)
prefix ?= /usr/local
DESTDIR = $(CURDIR)/release

# just create all the binaries and copy everything to $(DESTDIR)
default: daemon chroot cmginfo cmg_kde i18n

# do a complete clean and build
cleanbuild: distclean source daemon chroot cmginfo cmg_kde copy output

# wipe for a fresh build
distclean:
	[ ! -f kfakechroot/Makefile ] || $(MAKE) -C kfakechroot distclean
	-rm -f kfakechroot/build-stamp
#	-rm -f kfakechroot/config.sub kfakechroot/config.guess
	-rm -f kfakechroot/src/.libs/*.*o kfakechroot/src/.libs/*.*a
	-rm -f kfakechroot/src/*.o
	[ ! -f cmg_kfile/Makefile ] || $(MAKE) -C cmg_kfile distclean
	-rm -f cmg_kfile/po/*.mo
#	-rm -f cmg_kfile/admin/config.guess cmg_kfile/admin/config.sub
	[ ! -f cmg_kthumbnail/Makefile ] || $(MAKE) -C cmg_kthumbnail distclean
	-rm cmg_kthumbnail/doc/*/Makefile
#	-rm -f cmg_kthumbnail/admin/config.guess cmg_kthumbnail/admin/config.sub
	[ ! -f libCmgUtl/Makefile ] || $(MAKE) -C libCmgUtl clean
	[ ! -f klikd/Makefile ] || $(MAKE) -C klikd distclean
	[ ! -f klikclient/Makefile ] || $(MAKE) -C klikclient/po/ clean
	$(MAKE) -C klikclient/po/ clean
	-rm -rf release
	-rm -rf $(pkgname)

# trivially tar up the source
source:
	tar -czf ../$(pkgname).orig.tar.gz . --exclude="*.svn*"

# compile klikd
daemon:
	cd klikd && ./autogen.sh && ./configure --prefix=$(prefix)
	$(MAKE) -C klikd

# compile kfakechroot
chroot:
	cd kfakechroot && ./autogen.sh && ./configure --prefix=$(prefix) --mandir=$(prefix)/share/man --infodir=$(prefix)/share/info
	$(MAKE) -C kfakechroot

# compile cmginfo
cmginfo:
	cd libCmgUtl && ./autogen.sh && ./configure --prefix=$(prefix) --mandir=$(prefix)/share/man --infodir=$(prefix)/share/info
	$(MAKE) -C libCmgUtl

# compile kde thumbnailing
cmg_kde:
	cd cmg_kfile && ./configure --prefix=$(prefix) --mandir=$(prefix)/share/man --infodir=$(prefix)/share/info
	$(MAKE) -C cmg_kfile
	msgfmt cmg_kfile/po/kfile_cmg.fr.po -o cmg_kfile/po/kfile_cmg.fr.mo
	msgfmt cmg_kfile/po/kfile_cmg.es.po -o cmg_kfile/po/kfile_cmg.es.mo
	cd cmg_kthumbnail && ./configure --prefix=$(prefix) --mandir=$(prefix)/share/man --infodir=$(prefix)/share/info
	$(MAKE) -C cmg_kthumbnail

# compile klikclient language files
i18n:
	$(MAKE) -C klikclient/po/

# stage all non klikclient (python) parts in $(DESTDIR) (./release/ by default)
copy:
	$(MAKE) -C klikd DESTDIR=$(DESTDIR) install

	# FIXME we shouldn't have to hack the fakechroot install at all
	# Please see issue 198
	$(MAKE) -C kfakechroot DESTDIR=$(DESTDIR) install
	install -d $(DESTDIR)/$(prefix)/lib/klik/
	#install -m 755 $(DESTDIR)/$(prefix)/lib/kfakechroot/libkfakechroot.{so,la} $(DESTDIR)/$(prefix)/lib/klik/
	install -m 755 $(DESTDIR)/$(prefix)/lib/kfakechroot/libkfakechroot.so $(DESTDIR)/$(prefix)/lib/klik/
	install -m 755 $(DESTDIR)/$(prefix)/lib/kfakechroot/libkfakechroot.la $(DESTDIR)/$(prefix)/lib/klik/
	rm -rf $(DESTDIR)/$(prefix)/lib/kfakechroot
	rm -rf $(DESTDIR)/$(prefix)/bin/kfakechroot

	$(MAKE) -C libCmgUtl DESTDIR=$(DESTDIR) install

	$(MAKE) -C cmg_kthumbnail DESTDIR=$(DESTDIR) install
	install -D -m 644 cmg_kthumbnail/src/isofsthumbnail.desktop $(DESTDIR)/$(prefix)/share/services/isofsthumbnail.desktop

	install -D -m 644 ../binaryparts/share/servicemenus/klik.desktop $(DESTDIR)/$(prefix)/share/apps/konqueror/servicemenus/klik.desktop

	$(MAKE) -C cmg_kfile DESTDIR=$(DESTDIR) install
	install -D -m 644 cmg_kfile/po/kfile_cmg.fr.mo $(DESTDIR)/$(prefix)/share/locale/fr/LC_MESSAGES/kfile_cmg.mo
	install -D -m 644 cmg_kfile/po/kfile_cmg.es.mo $(DESTDIR)/$(prefix)/share/locale/es/LC_MESSAGES/kfile_cmg.mo

	$(MAKE) -C klikclient/po/ DESTDIR=$(DESTDIR) prefix=$(prefix) install

# create a klik binary tar for install.py using release for binaryparts
output:
	mkdir -p $(pkgname)/binaryparts
	cp -a $(DESTDIR)/* $(pkgname)/binaryparts
	mkdir -p $(pkgname)/client
	cp -a klikclient install install.py uninstall.py README README.kde-thumbnailer $(pkgname)/client
	tar -czf ../$(pkgname).tar.gz $(pkgname) --exclude="*.svn*"

# install klik client under $(ROOT)
setup:
# Python client
	install -d                                ${DESTDIR}/${prefix}/
	cp -a ./klikclient/{lib,share,bin}        ${DESTDIR}/${prefix}/
# Misc
	[ -n `which svnversion` ] && svnversion > $(DESTDIR)/${prefix}/share/klik/svnversion
	install -D install/klik.completion $(DESTDIR)/etc/bash_completion.d/klik
# Cleanup
	find $(DESTDIR)/$(prefix) -depth -name .svn -exec rm -rf '{}' \;
