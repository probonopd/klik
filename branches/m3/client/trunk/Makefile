#!/usr/bin/make -f

VERSION = $(shell awk '{print $$2; exit}' debian/changelog | sed 's|(||;s|)||')

# just create all the binaries and copy everything to release/
build: daemon chroot cmginfo cmg_kde copy

# do a complete clean and build
cleanbuild: distclean source daemon chroot cmginfo cmg_kde copy output

# wipe for a fresh build
distclean:
	-rm -f fakechroot/build-stamp
	[ ! -f fakechroot/trunk/Makefile ] || make -C fakechroot distclean
#	-rm -f fakechroot/config.sub fakechroot/config.guess
	-rm -f fakechroot/src/.libs/*.*o fakechroot/src/.libs/*.*a
	-rm -f fakechroot/src/*.o
	make -C CIsofsUtl clean
	[ ! -f klikd/trunk/Makefile ] || make -C klikd/trunk distclean
	[ ! -f cmg_kfile/Makefile ] || make -C cmg_kfile distclean
	-rm -f cmg_kfile/po/*.mo
#	-rm -f cmg_kfile/admin/config.guess cmg_kfile/admin/config.sub
	[ ! -f cmg_kthumbnail/Makefile ] || make -C cmg_kthumbnail distclean
	-rm cmg_kthumbnail/doc/*/Makefile
#	-rm -f cmg_kthumbnail/admin/config.guess cmg_kthumbnail/admin/config.sub
	-rm -rf release
	-rm -rf klik-$(VERSION)

# trivially tar up the source
source:
	tar -czf ../klik_$(VERSION).orig.tar.gz .

# compile klikd
daemon:
	cd klikd/trunk && ./autogen.sh && ./configure
	$(MAKE) -C klikd/trunk

# compile fakechroot
chroot:
	cd fakechroot && ./autogen.sh && ./configure --prefix=/usr --mandir=/usr/share/man --infodir=/usr/share/info
	$(MAKE) -C fakechroot

# compile cmginfo
cmginfo:
	$(MAKE) -C CIsofsUtl

# compile kde thumbnailing
cmg_kde:
	cd cmg_kfile && ./configure --prefix=/usr --mandir=/usr/share/man --infodir=/usr/share/info
	$(MAKE) -C cmg_kfile
	msgfmt cmg_kfile/po/kfile_cmg.fr.po -o cmg_kfile/po/kfile_cmg.fr.mo
	msgfmt cmg_kfile/po/kfile_cmg.es.po -o cmg_kfile/po/kfile_cmg.es.mo
	cd cmg_kthumbnail && ./configure --prefix=/usr --mandir=/usr/share/man --infodir=/usr/share/info
	$(MAKE) -C cmg_kthumbnail

# stage all non klikclient (python) parts in release/
copy:
	mkdir -p release/usr/bin
	cp klikd/trunk/src/klikd release/usr/bin/
	mkdir -p release/usr/lib/klik
	$(MAKE) -C fakechroot DESTDIR=$(CURDIR)/release install
	mv release/usr/lib/fakechroot/libfakechroot.so release/usr/lib/fakechroot/libfakechroot.la release/usr/lib/klik/
	rm -rf release/usr/lib/fakechroot
	cp CIsofsUtl/cmginfo release/usr/bin
	$(MAKE) -C cmg_kfile DESTDIR=$(CURDIR)/release install
	mkdir -p release/usr/share/locale/fr/LC_MESSAGES release/usr/share/locale/es/LC_MESSAGES/
	cp cmg_kfile/po/kfile_cmg.fr.mo release/usr/share/locale/fr/LC_MESSAGES/kfile_cmg.mo
	cp cmg_kfile/po/kfile_cmg.es.mo release/usr/share/locale/es/LC_MESSAGES/kfile_cmg.mo
	$(MAKE) -C cmg_kthumbnail DESTDIR=$(CURDIR)/release install
	mkdir -p release/usr/share/services
	cp cmg_kthumbnail/src/isofsthumbnail.desktop release/usr/share/services/

# create a klik binary tar for install.py using release for binaryparts
output:
	mkdir -p klik-$(VERSION)/binaryparts
	cp -a release/* klik-$(VERSION)/binaryparts
	mkdir -p klik-$(VERSION)/client
	cp -a klikclient install install.py uninstall.py README README.kde-thumbnailer klik-$(VERSION)/client
	tar -czf ../klik-$(VERSION).tar.gz klik-$(REVISION)

