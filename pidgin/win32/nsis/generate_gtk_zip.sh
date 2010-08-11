#!/bin/bash
# Script to generate zip file for GTK+ runtime to be included in Pidgin installer

PIDGIN_BASE=$1

if [ ! -e $PIDGIN_BASE/ChangeLog.win32 ]; then
	echo `basename $0` must must have the pidgin base dir specified as a parameter.
	exit 1
fi

STAGE_DIR=$PIDGIN_BASE/pidgin/win32/nsis/gtk_runtime_stage
#Subdirectory of $STAGE_DIR
INSTALL_DIR=Gtk
CONTENTS_FILE=$INSTALL_DIR/CONTENTS

#This needs to be changed every time there is any sort of change.
BUNDLE_VERSION=2.16.6.0

ATK="http://ftp.acc.umu.se/pub/gnome/binaries/win32/atk/1.26/atk_1.26.0-1_win32.zip ATK 1.26.0-1"
CAIRO="http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/cairo_1.8.10-1_win32.zip Cairo 1.8.10-1"
EXPAT="http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/expat_2.0.1-1_win32.zip Expat 2.0.1-1"
FONTCONFIG="http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/fontconfig_2.8.0-2_win32.zip Fontconfig 2.8.0-2"
FREETYPE="http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/freetype_2.3.11-2_win32.zip Freetype 2.3.11-2"
GETTEXT="http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/gettext-runtime-0.17-1.zip Gettext 0.17-1"
GLIB="http://ftp.gnome.org/pub/gnome/binaries/win32/glib/2.20/glib_2.20.5-1_win32.zip Glib 2.20.5-1"
GTK="http://ftp.acc.umu.se/pub/gnome/binaries/win32/gtk+/2.16/gtk+_2.16.6-2_win32.zip GTK+ 2.16.6-2"
LIBPNG="http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/libpng_1.4.0-1_win32.zip libpng 1.4.0-1"
PANGO="http://ftp.gnome.org/pub/gnome/binaries/win32/pango/1.26/pango_1.26.2-1_win32.zip Pango 1.26.2-1"
ZLIB="http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/zlib-1.2.3.zip zlib 1.2.3"

ALL="ATK CAIRO EXPAT FONTCONFIG FREETYPE GETTEXT GLIB GTK LIBPNG PANGO ZLIB"

mkdir -p $STAGE_DIR
cd $STAGE_DIR

rm -rf $INSTALL_DIR
mkdir $INSTALL_DIR

#new CONTENTS file
echo Bundle Version $BUNDLE_VERSION > $CONTENTS_FILE

function download_and_extract {
	URL=${1%%\ *}
	NAME=${1#*\ }
	FILE=`basename $URL`
	if [ ! -e $FILE ]; then
		echo Downloading $NAME
		wget $URL
	fi
	unzip -q $FILE -d $INSTALL_DIR
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
	LOCALE=`basename $LOCALE_DIR`
	if [ ! -e $PIDGIN_BASE/po/$LOCALE.po ]; then
		echo Remove $LOCALE translation as it is missing from Pidgin
		rm -r $LOCALE_DIR
	fi
done

#Generate zip file to be included in installer
zip -9 -r ../gtk-runtime-$BUNDLE_VERSION.zip Gtk

