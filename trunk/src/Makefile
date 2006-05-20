CC = gcc
OPTFLAGS = -g -O3
CFLAGS = -Wall `pkg-config --cflags gtk+-2.0 gconf-2.0` `xml2-config --cflags` -DWITH_GCONF -fomit-frame-pointer $(OPTFLAGS)
CLIBS = -lm `pkg-config --libs gtk+-2.0 gconf-2.0` `xml2-config --libs`
PROG = rawstudio

#OBJS = $(PROG).o 
OBJS = rawstudio.o dcraw_api.o dcraw_nomain.o matrix.o gtk-interface.o conf_interface.o rs-cache.o drawingarea.o toolbox.o tiff-meta.o

all: $(OBJS)
	$(CC) -o $(PROG) $(OBJS) $(CLIBS)

dcraw_nomain.o: dcraw.c
	$(CC) -c $(CFLAGS) -Wno-sign-compare -DDCRAW_NOMAIN -DNO_JPEG -DNO_LCMS -o $@ $<

dcraw_api.o: dcraw_api.c
	$(CC) -c $(CFLAGS) dcraw_api.c

clean:
	rm -f $(PROG) $(OBJS) *.c~ *.h~ 2>/dev/null
