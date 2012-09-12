#!/bin/bash
# Script to generate zip file for GTK+ runtime to be included in Pidgin installer

PIDGIN_BASE=$1

if [ ! -e $PIDGIN_BASE/ChangeLog ]; then
	echo $(basename $0) must must have the pidgin base dir specified as a parameter.
	exit 1
fi

STAGE_DIR=$PIDGIN_BASE/pidgin/win32/nsis/gtk_runtime_stage
#Subdirectory of $STAGE_DIR
INSTALL_DIR=Gtk
CONTENTS_FILE=$INSTALL_DIR/CONTENTS

#This needs to be changed every time there is any sort of change.
BUNDLE_VERSION=2.16.6.0

ATK="http://ftp.acc.umu.se/pub/gnome/binaries/win32/atk/1.26/atk_1.26.0-1_win32.zip ATK 1.26.0-1 97efc4c2640e7bae38f672c5e1ff66542a202756"
CAIRO="http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/cairo_1.8.10-1_win32.zip Cairo 1.8.10-1 a08476cccd807943958610977a138c4d6097c7b8"
EXPAT="http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/expat_2.0.1-1_win32.zip Expat 2.0.1-1 f47790b9e324cd8613acc9a17fd56bf2c14745fc"
FONTCONFIG="http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/fontconfig_2.8.0-2_win32.zip Fontconfig 2.8.0-2 37a3117ea6cc50c8a88fba9b6018f35a04fa71ce"
FREETYPE="http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/freetype_2.3.11-2_win32.zip Freetype 2.3.11-2 4d40ac1a44d818ac6720c2e93503346b91e99561"
GETTEXT="http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/gettext-runtime-0.17-1.zip Gettext 0.17-1 ad486eed8a531fba1d3cc7ad2f04e8e03367a962"
GLIB="http://ftp.gnome.org/pub/gnome/binaries/win32/glib/2.20/glib_2.20.5-1_win32.zip Glib 2.20.5-1 b670b37559ef4d088153f77960c6e24a2747efe7"
GTK="http://ftp.acc.umu.se/pub/gnome/binaries/win32/gtk+/2.16/gtk+_2.16.6-2_win32.zip GTK+ 2.16.6-2 012853e6de814ebda0cc4459f9eed8ae680e6d17"
LIBPNG="http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/libpng_1.4.0-1_win32.zip libpng 1.4.0-1 9f08167d43a19e4e2efac458f776f64d61544cb5"
PANGO="http://ftp.gnome.org/pub/gnome/binaries/win32/pango/1.26/pango_1.26.2-1_win32.zip Pango 1.26.2-1 f0e70127f7bb7a784a66d406cabf244da8316d31"
ZLIB="http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/zlib-1.2.3.zip zlib 1.2.3 0ee1e581e99fb761a5b2d46c534c861ca0f58175"

ALL="ATK CAIRO EXPAT FONTCONFIG FREETYPE GETTEXT GLIB GTK LIBPNG PANGO ZLIB"

mkdir -p $STAGE_DIR
cd $STAGE_DIR

rm -rf $INSTALL_DIR
mkdir $INSTALL_DIR

#new CONTENTS file
echo Bundle Version $BUNDLE_VERSION > $CONTENTS_FILE

function download_and_extract {
	URL=${1%%\ *}
	SHA1SUM=${1##*\ }
	NAME=${1%\ *}
	NAME=${NAME#*\ }
	FILE=$(basename $URL)
	if [ ! -e $FILE ]; then
		echo Downloading $NAME
		wget $URL || exit 1
	fi
	CHECK_SHA1SUM=`sha1sum $FILE`
	CHECK_SHA1SUM=${CHECK_SHA1SUM%%\ *}
	if [ "$CHECK_SHA1SUM" != "$SHA1SUM" ]; then
		echo "sha1sum ($CHECK_SHA1SUM) for $FILE doesn't match expected value of $SHA1SUM"
		exit 1
	fi
	EXTENSION=${FILE##*.}
	#This is an OpenSuSE build service RPM
	if [ $EXTENSION == 'rpm' ]; then
		echo "Generating zip from $FILE"
		FILE=$(../rpm2zip.sh $FILE)
	fi
	unzip -q $FILE -d $INSTALL_DIR || exit 1
	echo "$NAME" >> $CONTENTS_FILE
}

for VAL in $ALL
do
	VAR=${!VAL}
	download_and_extract "$VAR"
done

#Default GTK+ Theme to MS-Windows
echo gtk-theme-name = \"MS-Windows\" > $INSTALL_DIR/etc/gtk-2.0/gtkrc

#Blow away translations that we don't have in Pidgin
for LOCALE_DIR in $INSTALL_DIR/share/locale/*
do
	LOCALE=$(basename $LOCALE_DIR)
	if [ ! -e $PIDGIN_BASE/po/$LOCALE.po ]; then
		echo Removing $LOCALE translation as it is missing from Pidgin
		rm -r $LOCALE_DIR
	fi
done

#Generate zip file to be included in installer
zip -9 -r ../gtk-runtime-$BUNDLE_VERSION.zip Gtk

exit 0

