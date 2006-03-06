#!/bin/sh

if test -x /usr/bin/gnome-raw-thumbnailer
then
    for i in *.cr2 *.CR2 *.tif *.TIF *.orf *.ORF *.nef *.NEF
      do 
      gnome-raw-thumbnailer -s 128 file:///home/akv/Projects/rawstudio/src/raws/$i $i.png
    done 
else
    echo "Install gnome-raw-thumbnailer!"
fi
