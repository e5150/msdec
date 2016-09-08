PKG=msdec

include mk.config

CPPFLAGS += -I.
LDFLAGS  += -L.

.SUFFIXES:
.SUFFIXES: .o .c

EXE_SRC =               \
	msgui.c         \
	msdec.c         \
	rtl-modes.c

LIB_SRC=                \
	message.c       \
	histogram.c     \
	aircraft.c      \
	fields.c        \
	parse.c         \
	stats.c         \
	es.c            \
	df00.c          \
	df04.c          \
	df05.c          \
	df11.c          \
	df16.c          \
	df17.c          \
	df18.c          \
	df19.c          \
	df20.c          \
	df21.c          \
	df24.c          \
	cpr.c           \
	crc.c           \
	compass.c       \
	dump.c          \
	mac.c           \
	nation.c        \
	util.c          \
	tisb_c.c        \
	tisb_f.c        \
	bds_05.c        \
	bds_06.c        \
	bds_08.c        \
	bds_09.c        \
	bds_30.c        \
	bds_61.c        \
	bds_62.c        \
	bds_65.c        \
	bds_f2.c

GUI_SRC=map.c

HDR = $(LIB_SRC:.c=.h) map.h sources.h arg.h flags.h

GUI_OBJ = $(GUI_SRC:.c=.o)
LIB_OBJ = $(LIB_SRC:.c=.o)
EXE_OBJ = $(EXE_SRC:.c=.o)
SRC = $(EXE_SRC) $(LIB_SRC) $(GUI_SRC)
OBJ = $(EXE_OBJ) $(LIB_OBJ) $(GUI_OBJ)
PRG = $(EXE_SRC:.c=)

all: config.h mk.config $(PRG)

include mk.depend

libmsdec.a: $(LIB_OBJ)
	$(AR) rc $@ $(LIB_OBJ)

config.h:
	cat config.def.h > $@

mshist: mshist.o
	$(CC) $(LDFLAGS) -o $@ $< $(LDLIBS)

mscksum: mscksum.o libmsdec.a
	$(CC) $(LDFLAGS) -o $@ $< $(LDLIBS) -lmsdec

map.o: map.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $< $(shell pkg-config --cflags gtk+-2.0 libsoup-2.4)

msgui.o: msgui.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $< $(shell pkg-config --cflags gtk+-2.0 libsoup-2.4)

msgui: msgui.o libmsdec.a $(GUI_OBJ)
	$(CC) $(LDFLAGS) -o $@ $< $(GUI_OBJ) $(LDLIBS) -lm -lmsdec $(shell pkg-config --libs gtk+-2.0 libsoup-2.4)

msdec: msdec.o libmsdec.a
	$(CC) $(LDFLAGS) -o $@ $< $(LDLIBS) -lm -lmsdec

rtl-modes: rtl-modes.o libmsdec.a
	$(CC) $(LDFLAGS) -o $@ $< $(LDLIBS) $(RTLLIBS) -lm -lpthread -lmsdec

.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<
.o:
	$(CC) $(LDFLAGS) -o $@ $< $(LDLIBS)

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install -m 0755 $(PRG) $(DESTDIR)$(PREFIX)/bin

depend: $(SRC) $(HDR)
	gcc -I. -MM $^ > mk.$@

clean:
	rm -f $(OBJ) $(PRG) libmsdec.a

dist:
	mkdir -p $(PKG)-$(VERSION)
	tar -cf- $(SRC) $(HDR) flags config.def.h mk.depend mk.config Makefile | tar -C $(PKG)-$(VERSION) -xf-
	tar czf $(PKG)-$(VERSION).tar.gz $(PKG)-$(VERSION)
	rm -rf $(PKG)-$(VERSION)

i: install

c: clean

.PHONY:
	all depend install clean dist i c
