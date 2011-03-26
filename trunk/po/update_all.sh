#!/bin/sh

rm rawstudio.pot
make rawstudio.pot

for FILE in *.po; do
    intltool-update -d $(echo ${FILE}|cut -d"." -f1)
done

# Set a few headerlines in all .po files
sed -i 's/\"Project-Id-Version:.*\"/\"Project-Id-Version: Rawstudio 2.0\\n\"/g' *.po
sed -i 's/\"Report-Msgid-Bugs-To:.*\"/\"Report-Msgid-Bugs-To: rawstudio-dev@rawstudio.org\\n\"/g' *.po
sed -i 's/\"Language-Team:.*\"/\"Language-Team: Rawstudio development <rawstudio-dev@rawstudio.org>\\n\"/g' *.po