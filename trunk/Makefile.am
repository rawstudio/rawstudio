## Process this file with automake to produce Makefile.in

SUBDIRS = src po pixmaps

desktopdir = $(datadir)/applications
desktop_DATA = rawstudio.desktop

EXTRA_DIST = \
	autogen.sh \
	src/dcraw.c \
	ChangeLog \
	rawstudio.desktop

ChangeLog:
	svn2cl -i
