CC = gcc

CFLAGS = -Wall -g -O2 `pkg-config --cflags gtk+-2.0`
CLIBS = -lm `pkg-config --libs gtk+-2.0`
PROG = rawstudio

#OBJS = $(PROG).o 
OBJS = rawstudio.o dcraw_api.o dcraw_nomain.o matrix.o gtk-interface.o

all: $(OBJS)
	gcc -o $(PROG) $(OBJS) $(CLIBS)

dcraw_nomain.o: dcraw.c
	$(CC) -c $(CFLAGS) -Wno-sign-compare -DDCRAW_NOMAIN -DNO_JPEG -DNO_LCMS -o $@ $<

dcraw_api.o: dcraw_api.c
	$(CC) -c $(CFLAGS) dcraw_api.c

clean:
	rm -f $(PROG) $(OBJS) *.c~ *.h~ 2>/dev/null
