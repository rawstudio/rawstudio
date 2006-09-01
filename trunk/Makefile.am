## Process this file with automake to produce Makefile.in

SUBDIRS = src po

EXTRA_DIST = \
	autogen.sh \
	src/dcraw.c \
	ChangeLog

ChangeLog:
	svn2cl -i

install-data-local:
	@$(NORMAL_INSTALL)
	if test -d $(srcdir)/pixmaps; then \
	  $(mkinstalldirs) $(prefix)/share/pixmaps; \
	  for pixmap in $(srcdir)/pixmaps/*; do \
	    if test -f $$pixmap; then \
	      $(INSTALL_DATA) $$pixmap $(prefix)/share/pixmaps; \
	    fi \
	  done \
	fi
	$(mkinstalldirs) $(prefix)/share/applications; \
	if test -f rawstudio.desktop; then \
	  $(INSTALL_DATA) rawstudio.desktop $(prefix)/share/applications; \
	fi

dist-hook:
	if test -d pixmaps; then \
	  mkdir $(distdir)/pixmaps; \
	  for pixmap in pixmaps/*; do \
	    if test -f $$pixmap; then \
	      cp -p $$pixmap $(distdir)/pixmaps; \
	    fi \
	  done \
	fi

