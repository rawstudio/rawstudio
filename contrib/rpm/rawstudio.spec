Name:           rawstudio
Version:        2.1
Release:        0.3%{?dist}
Summary:        Read, manipulate and convert digital camera raw images

Group:          Applications/Multimedia
License:        GPLv2+
URL:            http://rawstudio.org

Source0:        http://rawstudio.org/files/release/%{name}-%{version}.tar.gz
Source1:        rawspeed.tar.gz
# Packaging a snapshot created with
# git clone git@github.com:rawstudio/rawstudio.git
# cd rawstudio
# git archive master --prefix=rawstudio-2.1/ | gzip > ../rawstudio-2.1.tar.gz
# git submodule init
# git submodule update
# cd plugins/load-rawspeed/rawspeed
# git archive 8ea2a3a --prefix=plugins/load-rawspeed/rawspeed/ | gzip > ../../../../rawspeed.tar.gz
#Source0:        rawstudio.tar.gz

BuildRequires:  gtk2-devel libxml2-devel GConf2-devel dbus-devel
BuildRequires:  lcms2-devel libjpeg-devel libtiff-devel exiv2-devel
BuildRequires:  lensfun-devel fftw-devel
BuildRequires:  osm-gps-map-devel = 0.7.3
BuildRequires:  sqlite-devel openssl-devel gphoto2-devel
BuildRequires:  desktop-file-utils
BuildRequires:  intltool
BuildRequires:  libtool autoconf automake

%description
Rawstudio is a highly specialized application for processing RAW images
from digital cameras. It is not a fully featured image editing application.

The RAW format is often recommended to get the best quality out of digital
camera images.  The format is specific to cameras and cannot be read by most
image editing applications.

Rawstudio makes it possible to read and manipulate RAW images, experiment
with the controls to see how they affect the image, and finally export into
JPEG, PNG or TIF format images from most digital cameras.


%package -n librawstudio-devel
Summary: librawstudio development files
Requires: librawstudio = %{version}-%{release}

%description -n librawstudio-devel
Development files for rawstudio backend library


%package -n librawstudio
Summary: Rawstudio backend library

%description -n librawstudio
Rawstudio backend library


%prep
%setup -q
%setup -q -D -T -a1

%build
# I give up ! copying for autogen.sh I don't need run all autogen.sh ... only this part.
for coin in `find $srcdir -name configure.ac -print`
do
  dr=`dirname $coin`
  if test -f $dr/NO-AUTO-GEN; then
    echo skipping $dr -- flagged as no auto-gen
  else
    echo processing $dr
    ( cd $dr

      aclocalinclude="$ACLOCAL_FLAGS"

      if grep "^AM_GLIB_GNU_GETTEXT" configure.ac >/dev/null; then
    echo "Creating $dr/aclocal.m4 ..."
    test -r $dr/aclocal.m4 || touch $dr/aclocal.m4
    echo "Running glib-gettextize...  Ignore non-fatal messages."
    echo "no" | glib-gettextize --force --copy
    echo "Making $dr/aclocal.m4 writable ..."
    test -r $dr/aclocal.m4 && chmod u+w $dr/aclocal.m4
      fi
      if grep "^AC_PROG_INTLTOOL" configure.ac >/dev/null; then
        echo "Running intltoolize..."
    intltoolize --copy --force --automake
      fi
      if grep "^AM_PROG_XML_I18N_TOOLS" configure.ac >/dev/null; then
        echo "Running xml-i18n-toolize..."
    xml-i18n-toolize --copy --force --automake
      fi
      if grep "^AM_PROG_LIBTOOL" configure.ac >/dev/null; then
    if test -z "$NO_LIBTOOLIZE" ; then
      echo "Running libtoolize..."
      libtoolize --force --copy
    fi
      fi
      echo "Running aclocal $aclocalinclude ..."
      aclocal $aclocalinclude
      if grep "^AM_CONFIG_HEADER" configure.ac >/dev/null; then
    echo "Running autoheader..."
    autoheader
      fi
      echo "Running automake --gnu $am_opt ..."
      automake --add-missing --gnu $am_opt
      echo "Running autoconf ..."
      autoconf
    )
  fi
done
#autoreconf -vif
%configure --disable-static --enable-experimental --enable-maintainer-mode
make %{?_smp_mflags}


%install
make install DESTDIR=$RPM_BUILD_ROOT
%find_lang %{name}

desktop-file-install \
        --dir ${RPM_BUILD_ROOT}%{_datadir}/applications         \
        --remove-category Application                           \
        --delete-original                                       \
        ${RPM_BUILD_ROOT}%{_datadir}/applications/rawstudio.desktop


%post
update-desktop-database &> /dev/null ||:

%postun
update-desktop-database &> /dev/null ||:


%files -f %{name}.lang
%doc README.md NEWS COPYING AUTHORS
%{_bindir}/rawstudio
%{_datadir}/rawstudio
%{_datadir}/rawspeed
%{_datadir}/pixmaps/rawstudio
%{_datadir}/applications/*rawstudio.desktop
%{_datadir}/icons/rawstudio.png

%files -n librawstudio
%{_libdir}/librawstudio-%{version}.so.*

%files -n librawstudio-devel
%{_includedir}/rawstudio-%{version}
%{_libdir}/librawstudio-%{version}.so
%{_libdir}/pkgconfig/rawstudio-%{version}.pc

%changelog
* Sun Dec 07 2014 Sérgio Basto <sergio@serjux.com> - 2.1-0.3
- More improvements.

* Thu Dec 04 2014 Sérgio Basto <sergio@serjux.com> - 2.1-0.2
- rawstudio from github.

* Tue Oct 14 2014 Sérgio Basto <sergio@serjux.com> - 2.1-0.1
- Update to svn rev 4430

* Sun Aug 17 2014 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 2.0-16
- Rebuilt for https://fedoraproject.org/wiki/Fedora_21_22_Mass_Rebuild

* Fri Jun 20 2014 Peter Robinson <pbrobinson@fedoraproject.org> 2.0-15
- Fix builds for new arches (aarch64/ppc64le)
- Modernise spec

* Sun Jun 08 2014 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 2.0-14
- Rebuilt for https://fedoraproject.org/wiki/Fedora_21_Mass_Rebuild

* Tue Dec 03 2013 Rex Dieter <rdieter@fedoraproject.org> - 2.0-13
- rebuild (exiv2)

* Sun Aug 04 2013 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 2.0-12
- Rebuilt for https://fedoraproject.org/wiki/Fedora_20_Mass_Rebuild

* Wed May 01 2013 Jon Ciesla <limburgher@gmail.com> - 2.0-11
- Drop desktop vendor tag.

* Thu Feb 14 2013 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 2.0-10
- Rebuilt for https://fedoraproject.org/wiki/Fedora_19_Mass_Rebuild

* Fri Jan 18 2013 Adam Tkac <atkac redhat com> - 2.0-9
- rebuild due to "jpeg8-ABI" feature drop

* Fri Dec 21 2012 Adam Tkac <atkac redhat com> - 2.0-8
- rebuild against new libjpeg

* Sat Jul 21 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 2.0-7
- Rebuilt for https://fedoraproject.org/wiki/Fedora_18_Mass_Rebuild

* Wed Jul 11 2012 Gianluca Sforna <giallu@gmail.com> - 2.0-6
- rebuild (flickcurl)
- add patch for newer lensfun headers location

* Wed May 02 2012 Rex Dieter <rdieter@fedoraproject.org> - 2.0-5
- rebuild (exiv2)

* Tue Feb 28 2012 Gianluca Sforna <giallu@gmail.com> - 2.0-4
- Fix FTBS with in F17+ (patch from upstream) 

* Sat Jan 14 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 2.0-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_17_Mass_Rebuild

* Fri Oct 14 2011 Rex Dieter <rdieter@fedoraproject.org> - 2.0-2
- rebuild (exiv2)

* Fri Apr  8 2011 Gianluca Sforna <giallu@gmail.com> - 2.0-1
- Update to final release
- remove upstreamed patch

* Sat Mar 26 2011 Gianluca Sforna <giallu@gmail.com> - 2.0-0.1.beta1
- Update to released beta
- Split librawstudio library in own package

* Mon Mar 14 2011 Gianluca Sforna <giallu@gmail.com> - 1.2-10
- update to newer snapshot, another fixed crash

* Mon Feb 21 2011 Gianluca Sforna <giallu@gmail.com> - 1.2-9
- update to newer snapshot, includes fixes for #635964 and #636919
- remove upstreamed patch, add new one to remove -Werror
- require gphoto2

* Wed Feb 09 2011 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.2-8.20100907svn3521
- Rebuilt for https://fedoraproject.org/wiki/Fedora_15_Mass_Rebuild

* Sun Jan 02 2011 Rex Dieter <rdieter@fedoraproject.org> - 1.2-7.20100907svn3521
- rebuild (exiv2)

* Wed Sep  8 2010 Gianluca Sforna <giallu gmail com>
- Fix BuildRequires
- Add updated patch for X11 link issue

* Tue Sep  7 2010 Gianluca Sforna <giallu gmail com>
- move to a snapshot
- drop upstreamed patches
- add find-lang
- remove .la files
- disable static library build

* Mon May 31 2010 Rex Dieter <rdieter@fedoraproject.org> - 1.2-5 
- rebuild (exiv2)

* Sat Feb 13 2010 Gianluca Sforna <giallu gmail com> - 1.2-4
- Add explicit link to libX11 (#564638)

* Mon Jan 04 2010 Rex Dieter <rdieter@fedoraproject.org> - 1.2-3 
- rebuild (exiv2)

* Sun Jul 26 2009 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.2-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_12_Mass_Rebuild

* Fri Apr 10 2009 Gianluca Sforna <giallu gmail com> - 1.2-1
- New upstream release

* Thu Feb 26 2009 Gianluca Sforna <giallu gmail com> - 1.1.1-4
- Fix build with newer glibc

* Wed Feb 25 2009 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.1.1-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_11_Mass_Rebuild

* Thu Dec 18 2008 Rex Dieter <rdieter@fedoraproject.org> - 1.1.1-2 
- respin (eviv2)

* Mon Oct 13 2008 Gianluca Sforna <giallu gmail com> - 1.1.1-1
- new upstream release

* Tue Sep 16 2008 Gianluca Sforna <giallu gmail com> - 1.1-1
- new upstream release

* Thu May  1 2008 Gianluca Sforna <giallu gmail com> - 1.0-1
- new upstream release
- drop upstreamed patch
- slightly improved summary

* Tue Feb 26 2008 Gianluca Sforna <giallu gmail com> - 0.7-2
- rebuild with gcc 4.3

* Thu Jan 24 2008 Gianluca Sforna <giallu gmail com> - 0.7-1
- New upstream release
- Improved package description
- Add fix for PPC build

* Sun Aug 19 2007 Gianluca Sforna <giallu gmail com> 0.6-1
- New upstream release
- Updated License field
- Include new pixmaps directory

* Wed Feb 21 2007 Gianluca Sforna <giallu gmail com> 0.5.1-1
- New upstream release
- Fix desktop-file-install warnings

* Tue Feb 06 2007 Gianluca Sforna <giallu gmail com> 0.5-1
- new upstream version
- add libtiff-devel BR
- drop upstreamed patch
- drop dcraw runtime Require

* Wed Sep 27 2006 Gianluca Sforna <giallu gmail com> 0.4.1-1
- new upstream version
- Add DESTDIR patch (and BR: automake)
- New .desktop file and icon

* Fri Jul 28 2006 Gianluca Sforna <giallu gmail com> 0.3-1
- Initial package. Adapted from fedora-rpmdevtools template.
