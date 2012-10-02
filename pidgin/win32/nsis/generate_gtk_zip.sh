#!/bin/bash
# Script to generate zip file for GTK+ runtime to be included in Pidgin installer

PIDGIN_BASE=$1

if [ ! -e $PIDGIN_BASE/ChangeLog ]; then
	echo $(basename $0) must must have the pidgin base dir specified as a parameter.
	exit 1
fi

STAGE_DIR=`readlink -f $PIDGIN_BASE/pidgin/win32/nsis/gtk_runtime_stage`
#Subdirectory of $STAGE_DIR
INSTALL_DIR=Gtk
CONTENTS_FILE=$INSTALL_DIR/CONTENTS
PIDGIN_VERSION=$( < $PIDGIN_BASE/VERSION )

#This needs to be changed every time there is any sort of change.
BUNDLE_VERSION=2.16.6.1
BUNDLE_SHA1SUM=5e16b7efb11943e8c80bc390f6c38df904fd36ed
ZIP_FILE="$PIDGIN_BASE/pidgin/win32/nsis/gtk-runtime-$BUNDLE_VERSION.zip"

#Download the existing file (so that we distribute the exact same file for all releases with the same bundle version)
FILE="$ZIP_FILE"
if [ ! -e "$FILE" ]; then
	wget "https://pidgin.im/win32/download_redir.php?version=$PIDGIN_VERSION&gtk_version=$BUNDLE_VERSION&dl_pkg=gtk" -O "$FILE"
fi
CHECK_SHA1SUM=`sha1sum $FILE`
CHECK_SHA1SUM=${CHECK_SHA1SUM%%\ *}
if [ "$CHECK_SHA1SUM" != "$BUNDLE_SHA1SUM" ]; then
	echo "sha1sum ($CHECK_SHA1SUM) for $FILE doesn't match expected value of $BUNDLE_SHA1SUM"
	# Allow "devel" versions to build their own bundles if the download doesn't succeed
	if [[ "$PIDGIN_VERSION" == *"devel" ]]; then
		echo "Continuing GTK+ Bundle creation for development version of Pidgin"
	else
		exit 1
	fi
else
	exit 0
fi


ATK="http://ftp.gnome.org/pub/gnome/binaries/win32/atk/1.32/atk_1.32.0-2_win32.zip ATK 1.32.0-2 sha1sum:3c31c9d6b19af840e2bd8ccbfef4072a6548dc4e"
#Cairo 1.10.2 has a bug that can be seen when selecting text
#CAIRO="http://ftp.gnome.org/pub/GNOME/binaries/win32/dependencies/cairo_1.10.2-2_win32.zip Cairo 1.10.2-2 sha1sum:d44cd66a9f4d7d29a8f2c28d1c1c5f9b0525ba44"
CAIRO="http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/cairo_1.8.10-1_win32.zip Cairo 1.8.10-1 sha1sum:a08476cccd807943958610977a138c4d6097c7b8"
EXPAT="http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/expat_2.1.0-1_win32.zip Expat 2.1.0-1 gpg:0x71D4DDE53F188CBE"
FONTCONFIG="http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/fontconfig_2.8.0-2_win32.zip Fontconfig 2.8.0-2 sha1sum:37a3117ea6cc50c8a88fba9b6018f35a04fa71ce"
FREETYPE="http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/freetype_2.4.10-1_win32.zip Freetype 2.4.10-1 gpg:0x71D4DDE53F188CBE"
GETTEXT="http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/gettext-runtime_0.18.1.1-2_win32.zip Gettext 0.18.1.1-2 sha1sum:a7cc1ce2b99b408d1bbea9a3b4520fcaf26783b3"
GLIB="http://ftp.gnome.org/pub/gnome/binaries/win32/glib/2.28/glib_2.28.8-1_win32.zip Glib 2.28.8-1 sha1sum:5d158f4c77ca0b5508e1042955be573dd940b574"
GTK="http://ftp.acc.umu.se/pub/gnome/binaries/win32/gtk+/2.16/gtk+_2.16.6-2_win32.zip GTK+ 2.16.6-2 sha1sum:012853e6de814ebda0cc4459f9eed8ae680e6d17"
LIBPNG="http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/libpng_1.4.12-1_win32.zip libpng 1.4.12-1 gpg:0x71D4DDE53F188CBE"
PANGO="http://ftp.gnome.org/pub/gnome/binaries/win32/pango/1.29/pango_1.29.4-1_win32.zip Pango 1.29.4-1 sha1sum:3959319bd04fbce513458857f334ada279b8cdd4"
ZLIB="http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/zlib_1.2.5-2_win32.zip zlib 1.2.5-2 sha1sum:568907188761df2d9309196e447d91bbc5555d2b"

ALL="ATK CAIRO EXPAT FONTCONFIG FREETYPE GETTEXT GLIB GTK LIBPNG PANGO ZLIB"

mkdir -p $STAGE_DIR
cd $STAGE_DIR

rm -rf $INSTALL_DIR
mkdir $INSTALL_DIR

#new CONTENTS file
echo Bundle Version $BUNDLE_VERSION > $CONTENTS_FILE

function download_and_extract {
	URL=${1%%\ *}
	VALIDATION=${1##*\ }
	NAME=${1%\ *}
	NAME=${NAME#*\ }
	FILE=$(basename $URL)
	if [ ! -e $FILE ]; then
		echo Downloading $NAME
		wget $URL || exit 1
	fi
	VALIDATION_TYPE=${VALIDATION%%:*}
	VALIDATION_VALUE=${VALIDATION##*:}
	if [ $VALIDATION_TYPE == 'sha1sum' ]; then
		CHECK_SHA1SUM=`sha1sum $FILE`
		CHECK_SHA1SUM=${CHECK_SHA1SUM%%\ *}
		if [ "$CHECK_SHA1SUM" != "$VALIDATION_VALUE" ]; then
			echo "sha1sum ($CHECK_SHA1SUM) for $FILE doesn't match expected value of $VALIDATION_VALUE"
			exit 1
		fi
	elif [ $VALIDATION_TYPE == 'gpg' ]; then
		if [ ! -e "$FILE.asc" ]; then
			echo Downloading GPG key for $NAME
			wget "$URL.asc" || exit 1
		fi
		#Use our own keyring to avoid adding stuff to the main keyring
		GPG="gpg -q --keyring $VALIDATION_VALUE-keyring.gpg"
		$GPG --list-keys "$VALIDATION_VALUE" > /dev/null
		if [ $? -ne 0 ]; then
		       	$GPG --keyserver pgp.mit.edu --recv-key "$VALIDATION_VALUE" || exit 1
		fi
		$GPG --verify "$FILE.asc" || (echo "$FILE failed signature verification"; exit 1) || exit 1
	else
		echo "Unrecognized validation type of $VALIDATION_TYPE"
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
rm -f $ZIP_FILE
zip -9 -r $ZIP_FILE Gtk

exit 0

