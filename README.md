About Rawstudio
===============

Rawstudio is an open-source program to read and manipulate RAW images from 
digital cameras.

To get the best quality out of your digital camera, it is often recommended
that you record your pictures in RAW format. This format is mostly specific
to a camera and cannot be read by most image editing applications. 
Our goal is to supply you with a tool, where you can have the benefits of 
RAW images and the ease of use of JPEG images.

The main focus of Rawstudio is to enable efficient review and fast processing 
of large image collections. We aim to supply you with a tool that makes it
possible for you to review and process several hundred images
in a matter of a few hours.

Rawstudio will convert your RAW files into JPEG, PNG or TIF images which you
can then print or send to friends and clients.

Rawstudio is intended as the first tool in your image processing chain. 
After you have made your overall image adjustments to your image, you can 
use an image editing application to further work on your images. 
Rawstudio itself is a highly specialized application for reviewing and 
processing RAW images, not a fully featured image editing application.

Feature List
============

* Intuitive GTK+ interface
* Full DNG Color Profile support
* Batch processing
* Tethered shooting
* Various post-shot controls (white balance, saturation and exposure compensation among others)
* Easy and flexible copy&paste settings between images
* Develop images directly on storage card
* Image tagging and sorting
* Automatic lens distortion correction
* Advanced noise reduction
* Unique intelligent sharpening
* Chromatic aberration and vignetting correction
* Exposure mask
* Cropping
* Straighten
* Fullscreen mode
* Secondary monitor support
* Image location independent
* Automatic filenaming based on EXIF information
* 32 bit float point precision image processing
* Optimized for and SSE and SSE2 (detected runtime) and fully multithreaded
* And much more...

Building from git
=================

Building Rawstudio yourself is possible with a few library requirements.

For Ubuntu 19.10 the following should install all build dependencies:

```bash
$ sudo apt install make \
    gcc \
    autoconf \
    libtool-bin \
    libglib2.0-dev-bin \
    automake \
    libjpeg-turbo8-dev \
    libtiff5-dev \
    libglib2.0-dev \
    libgtk-3-dev \
    libxml2-dev \
    libgconf2-dev \
    libsqlite3-dev \
    liblensfun-dev \
    liblcms2-dev \
    libgphoto2-dev \
    libexiv2-dev \
    libfftw3-dev
```

The following should install Rawstudio to `/tmp/rs-prefix/bin/rawstudio`:

```bash
$ ./autogen.sh --prefix=/tmp/rs-prefix
$ make
$ make install
```
