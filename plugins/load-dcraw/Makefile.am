#
# Note for Rawstudio developers:
#
# load-dcraw is copied from ufraw-0.22.tar.gz
# https://sourceforge.net/projects/ufraw/files/ufraw/ufraw-0.22/ufraw-0.22.tar.gz
#
# From ufraw we copied:
# dcraw.cc
# dcraw.h
# dcraw_api.cc
# dcraw_api.h
# dcraw_indi.c
# nikon_curve.c
# nikon_curve.h
# uf_glib.h
# uf_progress.h
# ufobject.cc
# ufobject.h
# ufraw-batch.c
# ufraw.h
# ufraw_colorspaces.c
# ufraw_colorspaces.h
# ufraw_conf.c
# ufraw_developer.c
# ufraw_embedded.c
# ufraw_exiv2.cc
# ufraw_message.c
# ufraw_routines.c
# ufraw_settings.cc
# ufraw_ufraw.c
# ufraw_writer.c
# wb_presets.c
#
# - Thanks Dave and Udi, you rock!
#

AM_CFLAGS =\
	-DDCRAW_NOMAIN\
	-DDCRAW_NOLCMS\
	-DWITH_MMAP_HACK\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@ @LCMS_CFLAGS@\
	-I$(top_srcdir)/librawstudio/ \
	-I$(top_srcdir)/

AM_CXXFLAGS = $(AM_CFLAGS)
AM_CPPFLAGS = -DDCRAW_NOMAIN \
	-DUFRAW_LOCALEDIR=\"$(datadir)/locale\"


lib_LTLIBRARIES = load_dcraw.la

libdir = @RAWSTUDIO_PLUGINS_LIBS_DIR@

load_dcraw_la_LIBADD = @PACKAGE_LIBS@ @LCMS_LIBS@
load_dcraw_la_LDFLAGS = -module -avoid-version
load_dcraw_la_SOURCES =  dcrawloader.c mmap-hack.c mmap-hack.h dcraw_indi.c ufraw_ufraw.c ufraw_conf.c wb_presets.c ufraw_routines.c ufraw_developer.c ufraw_message.c ufraw_embedded.c ufraw_colorspaces.c ufraw-batch.c ufraw_writer.c nikon_curve.c ufraw_settings.cc ufobject.cc dcraw.cc dcraw.h dcraw_api.cc ufraw_exiv2.cc dcraw_api.h uf_glib.h uf_progress.h ufraw.h ufobject.h nikon_curve.h ufraw_colorspaces.h
