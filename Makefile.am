## Process this file with automake to produce Makefile.in

SUBDIRS = src po

EXTRA_DIST = \
	autogen.sh \
	src/dcraw.c \
	ChangeLog \
	rawstudio.desktop

ChangeLog:
	svn2cl -i

install-data-local:
	@$(NORMAL_INSTALL)
	if test -d $(srcdir)/pixmaps; then \
	  $(mkinstalldirs) $(DESTDIR)/$(prefix)/share/pixmaps; \
	  for pixmap in $(srcdir)/pixmaps/*; do \
	    if test -f $$pixmap; then \
	      $(INSTALL_DATA) $$pixmap $(DESTDIR)/$(prefix)/share/pixmaps; \
	    fi \
	  done \
	fi
	$(mkinstalldirs) $(DESTDIR)/$(prefix)/share/applications; \
	if test -f rawstudio.desktop; then \
	  $(INSTALL_DATA) rawstudio.desktop $(DESTDIR)/$(prefix)/share/applications; \
	fi

dist-hook:
	if test -d pixmaps; then \
	  mkdir $(DESTDIR)/$(distdir)/pixmaps; \
	  for pixmap in pixmaps/*; do \
	    if test -f $$pixmap; then \
	      cp -p $$pixmap $(DESTDIR)/$(distdir)/pixmaps; \
	    fi \
	  done \
	fi

