dnl Process this file with autoconf to produce a configure script.

AC_INIT([rawstudio], [2.1])
AM_INIT_AUTOMAKE([foreign subdir-objects])
AC_CONFIG_HEADERS(config.h)
AM_MAINTAINER_MODE
AM_PROG_LIBTOOL
AC_CONFIG_MACRO_DIR([m4])

AC_ISC_POSIX
AC_PROG_CC
AC_PROG_CXX
AC_HEADER_STDC

AC_ARG_ENABLE(experimental, 
	AS_HELP_STRING(
		[--enable-experimental],
		[Enable experimental code (default: disabled)]),
	[EXPERIMENTAL=yes]
)

AC_DEFUN([AX_CHECK_COMPILER_FLAGS],
[AC_PREREQ(2.59) dnl for _AC_LANG_PREFIX
AC_MSG_CHECKING([whether _AC_LANG compiler accepts $1])
dnl Some hackery here since AC_CACHE_VAL can't handle a non-literal varname:
AS_LITERAL_IF([$1],
  [AC_CACHE_VAL(AS_TR_SH(ax_cv_[]_AC_LANG_ABBREV[]_flags_[$1]), [
      ax_save_FLAGS=$[]_AC_LANG_PREFIX[]FLAGS
      _AC_LANG_PREFIX[]FLAGS="$1"
      AC_COMPILE_IFELSE([AC_LANG_PROGRAM()],
        AS_TR_SH(ax_cv_[]_AC_LANG_ABBREV[]_flags_[$1])=yes,
        AS_TR_SH(ax_cv_[]_AC_LANG_ABBREV[]_flags_[$1])=no)
      _AC_LANG_PREFIX[]FLAGS=$ax_save_FLAGS])],
  [ax_save_FLAGS=$[]_AC_LANG_PREFIX[]FLAGS
   _AC_LANG_PREFIX[]FLAGS="$1"
   AC_COMPILE_IFELSE([AC_LANG_PROGRAM()],
     eval AS_TR_SH(ax_cv_[]_AC_LANG_ABBREV[]_flags_[$1])=yes,
     eval AS_TR_SH(ax_cv_[]_AC_LANG_ABBREV[]_flags_[$1])=no)
   _AC_LANG_PREFIX[]FLAGS=$ax_save_FLAGS])
eval ax_check_compiler_flags=$AS_TR_SH(ax_cv_[]_AC_LANG_ABBREV[]_flags_[$1])
AC_MSG_RESULT($ax_check_compiler_flags)
if test "x$ax_check_compiler_flags" = xyes; then
	m4_default([$2], :)
else
	m4_default([$3], :)
fi
])dnl AX_CHECK_COMPILER_FLAGS

if test "$EXPERIMENTAL" == "yes"; then
	AC_DEFINE([EXPERIMENTAL],[1],[Enable experimental code])
fi

dnl libjpeg
if test -z "$LIBJPEG"; then
  AC_CHECK_LIB(jpeg, jpeg_destroy_decompress, jpeg_ok=yes, jpeg_ok=no)
  if test "$jpeg_ok" = yes; then
    AC_CHECK_HEADER(jpeglib.h, jpeg_ok=yes, jpeg_ok=no)
    if test "$jpeg_ok" = yes; then
      LIBJPEG='-ljpeg'
    else
      AC_MSG_ERROR([*** JPEG header files not found.])
    fi
  else
    AC_MSG_ERROR([*** Rawstudio requires libjpeg.])
  fi
fi
AC_SUBST(LIBJPEG)

dnl libtiff
if test -z "$LIBTIFF"; then
AC_CHECK_LIB(tiff, TIFFOpen, tiff_ok=yes, tiff_ok=no)
  if test "$tiff_ok" = yes; then
    AC_CHECK_HEADER(tiffio.h, tiff_ok=yes, tiff_ok=no)
    if test "$tiff_ok" = yes; then
      LIBTIFF='-ltiff'
    else
      AC_MSG_ERROR([*** libtiff header files not found.])
    fi
  else
    AC_MSG_ERROR([*** Rawstudio requires libtiff.])
  fi
fi
AC_SUBST(LIBTIFF)

pkg_modules="glib-2.0 >= 2.32 gtk+-3.0 >= 3.4 libxml-2.0 >= 2.4 x11 gthread-2.0 gmodule-no-export-2.0"
PKG_CHECK_MODULES(PACKAGE, [$pkg_modules])
AC_SUBST(PACKAGE_CFLAGS)
AC_SUBST(PACKAGE_LIBS)

PKG_CHECK_MODULES(GCONF, [gconf-2.0 >= 2.0])
AC_SUBST(GCONF_CFLAGS)
AC_SUBST(GCONF_LIBS)

PKG_CHECK_MODULES(SQLITE3, [sqlite3])
AC_SUBST(SQLITE3_CFLAGS)
AC_SUBST(SQLITE3_LIBS)

PKG_CHECK_MODULES(LENSFUN, [lensfun])
AC_SUBST(LENSFUN_CFLAGS)
AC_SUBST(LENSFUN_LIBS)

PKG_CHECK_MODULES(LCMS, [lcms2])
AC_SUBST(LCMS_CFLAGS)
AC_SUBST(LCMS_LIBS)

PKG_CHECK_MODULES(LIBGPHOTO2, [libgphoto2])
AC_SUBST(LIBGPHOTO2_CFLAGS)
AC_SUBST(LIBGPHOTO2_LIBS)

PKG_CHECK_MODULES(EXIV2, [exiv2])
AC_SUBST(EXIV2_CFLAGS)
AC_SUBST(EXIV2_LIBS)

PKG_CHECK_MODULES(FFTW3F, [fftw3f])
AC_SUBST(FFTW3F_CFLAGS)
AC_SUBST(FFTW3F_LIBS)

PKG_CHECK_MODULES(DBUS, [dbus-1])
AC_SUBST(DBUS_CFLAGS)
AC_SUBST(DBUS_LIBS)

GETTEXT_PACKAGE=rawstudio
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [Gettext package.])

dnl Add the languages which your application supports here.
ALL_LINGUAS="cs da de en fr it nb pl ru fi es sv nl pt_BR ca hu ja"
AM_GLIB_GNU_GETTEXT
AC_CHECK_FUNCS(memmem)
AC_CHECK_FUNCS(strcasecmp)

AX_CHECK_COMPILER_FLAGS("-msse2", [_CAN_COMPILE_SSE2=yes], [_CAN_COMPILE_SSE2=no]) 
AX_CHECK_COMPILER_FLAGS("-msse4.1", [_CAN_COMPILE_SSE4_1=yes],[_CAN_COMPILE_SSE4_1=no]) 
AX_CHECK_COMPILER_FLAGS("-mavx", [_CAN_COMPILE_AVX=yes],[_CAN_COMPILE_AVX=no]) 

AM_CONDITIONAL(CAN_COMPILE_SSE4_1,  test "$_CAN_COMPILE_SSE4_1" = yes)
AM_CONDITIONAL(CAN_COMPILE_SSE2, test "$_CAN_COMPILE_SSE2" = yes)
AM_CONDITIONAL(CAN_COMPILE_AVX, test "$_CAN_COMPILE_AVX" = yes)

if test -d .git; then
  SRCINFO=-$(date +"%Y%m%d")-$(git log -n 1 --pretty="format:%h")
else
  SRCINFO=""
fi

RAWSTUDIO_VERSION="${VERSION}${SRCINFO}"
AC_DEFINE_UNQUOTED([RAWSTUDIO_VERSION], ["$RAWSTUDIO_VERSION"], ["Public revision"])


RAWSTUDIO_PLUGINS_LIBS_DIR="$libdir/rawstudio/plugins"
AC_SUBST(RAWSTUDIO_PLUGINS_LIBS_DIR)

AC_OUTPUT([
Makefile
librawstudio/Makefile
librawstudio/rawstudio-${VERSION}.pc
plugins/Makefile
plugins/cache/Makefile
plugins/colorspace-adobergb/Makefile
plugins/colorspace-prophoto/Makefile
plugins/colorspace-srgb/Makefile
plugins/colorspace-transform/Makefile
plugins/crop/Makefile
plugins/dcp/Makefile
plugins/demosaic/Makefile
plugins/denoise/Makefile
plugins/exposure-mask/Makefile
plugins/fuji-rotate/Makefile
plugins/input-file/Makefile
plugins/input-image16/Makefile
plugins/lensfun/Makefile
plugins/load-dcraw/Makefile
plugins/load-gdk/Makefile
plugins/load-png/Makefile
plugins/load-rawspeed/Makefile
plugins/meta-ciff/Makefile
plugins/meta-exiv2/Makefile
plugins/meta-mrw/Makefile
plugins/meta-raf/Makefile
plugins/meta-tiff/Makefile
plugins/meta-x3f/Makefile
plugins/output-jpegfile/Makefile
plugins/output-pngfile/Makefile
plugins/output-tifffile/Makefile
plugins/resample/Makefile
plugins/rotate/Makefile
src/Makefile
po/Makefile.in
pixmaps/Makefile
profiles/Makefile
])

