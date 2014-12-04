SUBDIRS = \
	cache \
	colorspace-adobergb \
	colorspace-prophoto \
	colorspace-srgb \
	colorspace-transform \
	crop \
	dcp \
	demosaic \
	denoise \
	exposure-mask \
	fuji-rotate \
	input-file \
	input-image16 \
	lensfun \
	load-dcraw \
	load-gdk \
	load-png \
	load-rawspeed \
	meta-ciff \
	meta-exiv2 \
	meta-mrw \
	meta-raf \
	meta-tiff \
	meta-x3f \
	output-jpegfile \
	output-pngfile \
	output-tifffile \
	resample \
	rotate

# Remove .la and .a files. It's crude and ugly but it works. AC_ENABLE_STATIC(no) doesn't by the way.
install-exec-hook:
	find @RAWSTUDIO_PLUGINS_LIBS_DIR@ -type f -name \*.la -delete
	find @RAWSTUDIO_PLUGINS_LIBS_DIR@ -type f -name \*.a -delete
