PROG=	ar
CFLAGS += -I.
MAN=	ar.1 ar.5
OBJLIST = append.o ar.o archive.o contents.o delete.o extract.o misc.o \
	move.o print.o replace.o strmode.o

all: $(PROG)

$(PROG): $(OBJLIST)
	$(CC) -o $(PROG) $(OBJLIST)

%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

clean:
	rm -rf *.o

mrproper: clean
	rm -rf $(PROG)
