#!/bin/sh

if test -x /usr/bin/gnome-raw-thumbnailer
then
    if ! test -d .rawstudio
	then mkdir .rawstudio
    fi
    echo -n "Extracting thumbnails"
    for i in *.cr2 *.CR2 *.tif *.TIF *.orf *.ORF *.nef *.NEF
      do 
      gnome-raw-thumbnailer -s 128 file://`pwd`/$i .rawstudio/$i_thumb.png 2>&1 > /dev/null
      echo -n "."
    done 
    echo "done."
else
    echo "Install gnome-raw-thumbnailer!"
fi
