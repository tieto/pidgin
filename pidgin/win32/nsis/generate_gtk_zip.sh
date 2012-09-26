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
BUNDLE_VERSION=2.16.6.1

ATK="http://ftp.gnome.org/pub/gnome/binaries/win32/atk/1.32/atk_1.32.0-2_win32.zip ATK 1.32.0-2 3c31c9d6b19af840e2bd8ccbfef4072a6548dc4e"
#Cairo 1.10.2 has a bug that can be seen when selecting text
#CAIRO="http://ftp.gnome.org/pub/GNOME/binaries/win32/dependencies/cairo_1.10.2-2_win32.zip Cairo 1.10.2-2 d44cd66a9f4d7d29a8f2c28d1c1c5f9b0525ba44"
CAIRO="http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/cairo_1.8.10-1_win32.zip Cairo 1.8.10-1 a08476cccd807943958610977a138c4d6097c7b8"
EXPAT="http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/expat_2.1.0-1_win32.zip Expat 2.1.0-1 607ba00b8c7c4be5f1701f914b972c2b12005062"
FONTCONFIG="http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/fontconfig_2.8.0-2_win32.zip Fontconfig 2.8.0-2 37a3117ea6cc50c8a88fba9b6018f35a04fa71ce"
FREETYPE="http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/freetype_2.4.10-1_win32.zip Freetype 2.4.10-1 e4655cf2a590fd5fbe8861a9fcbfd32131e61cac"
GETTEXT="http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/gettext-runtime_0.18.1.1-2_win32.zip Gettext 0.18.1.1-2 a7cc1ce2b99b408d1bbea9a3b4520fcaf26783b3"
GLIB="http://ftp.gnome.org/pub/gnome/binaries/win32/glib/2.28/glib_2.28.8-1_win32.zip Glib 2.28.8-1 5d158f4c77ca0b5508e1042955be573dd940b574"
GTK="http://ftp.acc.umu.se/pub/gnome/binaries/win32/gtk+/2.16/gtk+_2.16.6-2_win32.zip GTK+ 2.16.6-2 012853e6de814ebda0cc4459f9eed8ae680e6d17"
LIBPNG="http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/libpng_1.4.12-1_win32.zip libpng 1.4.12-1 64f271ca9ae5dc6e5fc0a8129b9ef4297df7959f"
PANGO="http://ftp.gnome.org/pub/gnome/binaries/win32/pango/1.29/pango_1.29.4-1_win32.zip Pango 1.29.4-1 3959319bd04fbce513458857f334ada279b8cdd4"
ZLIB="http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/zlib_1.2.5-2_win32.zip zlib 1.2.5-2 568907188761df2d9309196e447d91bbc5555d2b"

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
ZIPFILE=../gtk-runtime-$BUNDLE_VERSION.zip
rm -f $ZIPFILE
zip -9 -r $ZIPFILE Gtk

exit 0

