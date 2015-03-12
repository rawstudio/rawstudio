rawstudiodir = $(datadir)/rawstudio/profiles

RECIPE_FILES := $(wildcard recipes/*.dcpr)
SIMPLE_SOURCE_FILES := $(wildcard simple-src/*.xml)
DCP_FILES := $(notdir $(SIMPLE_SOURCE_FILES:.xml=.dcp))

%.dcp: simple-src/%.xml write-dcp
	./write-dcp "$<" "$@"

noinst_PROGRAMS = write-dcp

write_dcp_SOURCES = write-dcp.c

write_dcp_LDADD = @LIBTIFF@ @PACKAGE_LIBS@
write_dcp_CFLAGS = @PACKAGE_CFLAGS@

rawstudio_DATA = rawstudio-cameras.xml\
	generic_camera_profile.icc\
	sRGB.icc\
	prophoto.icc\
	compatibleWithAdobeRGB1998.icc\
	sRGB-linear.icc\
	prophoto-linear.icc\
	compatibleWithAdobeRGB1998-linear.icc\
	$(DCP_FILES)

EXTRA_DIST = $(rawstudio_DATA) \
	$(RECIPE_FILES) \
	$(SIMPLE_SOURCE_FILES)
