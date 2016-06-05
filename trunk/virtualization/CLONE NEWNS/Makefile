.PHONY: install all clean distclean

CFLAGS=-Werror
VERSION=0.5

klik_suid: klik_suid.o
	gcc klik_suid.o -o klik_suid

all: klik_suid

install: all
	install -m4755 klik_suid $(DESTDIR)/usr/bin
	install -m755 klik-overlay $(DESTDIR)/usr/bin
	install -m755 fuse/funionfs_klik $(DESTDIR)/usr/bin
	mkdir -p $(DESTDIR)/usr/share/doc/klik-overlay/examples/
	install -m755 examples/.klik examples/.zAppRun $(DESTDIR)/usr/share/doc/klik-overlay/examples/

clean distclean:
	rm -f klik_suid *.o
