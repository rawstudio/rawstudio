## Process this file with automake to produce Makefile.in

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = librawstudio plugins src po pixmaps profiles

desktopdir = $(datadir)/applications
desktop_DATA = rawstudio.desktop

appdatadir = $(datarootdir)/appdata
dist_appdata_DATA = rawstudio.appdata.xml

EXTRA_DIST = \
	autogen.sh \
	ChangeLog \
	rawstudio.desktop \
	gettext.h

ChangeLog:
	git log >$@
