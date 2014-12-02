## Process this file with automake to produce Makefile.in

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = librawstudio plugins src po pixmaps profiles

desktopdir = $(datadir)/applications
desktop_DATA = rawstudio.desktop

EXTRA_DIST = \
	autogen.sh \
	ChangeLog \
	rawstudio.desktop \
	gettext.h \
	.version $(SVNINFO)

ChangeLog:
	svn2cl -i
