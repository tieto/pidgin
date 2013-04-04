#!/bin/bash
# Script to generate zip file for GTK+ runtime to be included in Pidgin installer

PIDGIN_BASE=`pwd`
PIDGIN_BASE="$PIDGIN_BASE/../../.."
GPG_SIGN=$1

if [ ! -e $PIDGIN_BASE/ChangeLog ]; then
	echo "Pidgin base directory not found"
	exit 1
fi

STAGE_DIR=`readlink -f $PIDGIN_BASE/pidgin/win32/nsis/gtk_runtime_stage`
#Subdirectory of $STAGE_DIR
INSTALL_DIR=Gtk
CONTENTS_FILE=$INSTALL_DIR/CONTENTS
PIDGIN_VERSION=$( < $PIDGIN_BASE/VERSION )

#This needs to be changed every time there is any sort of change.
BUNDLE_VERSION=2.24.14.0
BUNDLE_SHA1SUM="402c265590f304537e31a1f3b04aad32c6eea620"
ZIP_FILE="$PIDGIN_BASE/pidgin/win32/nsis/gtk-runtime-$BUNDLE_VERSION.zip"

#Download the existing file (so that we distribute the exact same file for all releases with the same bundle version)
FILE="$ZIP_FILE"
if [ ! -e "$FILE" ]; then
	echo "Downloading the existing file"
	wget -q "https://pidgin.im/win32/download_redir.php?version=$PIDGIN_VERSION&gtk_version=$BUNDLE_VERSION&dl_pkg=gtk" -O "$FILE"
	if [ `stat -c %s $FILE` == 0 ]; then
		rm $FILE
	fi
fi
if [ -e "$FILE" ]; then
	CHECK_SHA1SUM=`sha1sum $FILE`
	CHECK_SHA1SUM=${CHECK_SHA1SUM%%\ *}
else
	CHECK_SHA1SUM=
fi
if [ "$CHECK_SHA1SUM" != "$BUNDLE_SHA1SUM" ]; then
	if [ "x$CHECK_SHA1SUM" != "x" ]; then
		echo "sha1sum ($CHECK_SHA1SUM) for $FILE doesn't match expected value of $BUNDLE_SHA1SUM"
	fi
	# Allow "devel" versions to build their own bundles if the download doesn't succeed
	if [[ "$PIDGIN_VERSION" == *"devel" ]]; then
		echo "Continuing GTK+ Bundle creation for development version of Pidgin"
	else
		exit 1
	fi
else
	exit 0
fi

#DOWNLOAD_HOST="http://download.opensuse.org/repositories/windows:/mingw:/win32/openSUSE_12.3/noarch/"
#TODO: this is just a temporary mirror - Tomek Wasilczyk's <tomkiewicz@cpw.pidgin.im> Dropbox
DOWNLOAD_HOST="http://dl.dropbox.com/u/5448886/pidgin-win32/runtime-deps/"

ATK="${DOWNLOAD_HOST}mingw32-atk-2.6.0-1.4.noarch.rpm ATK 2.6.0-1.4 sha1sum:d0792a3355b22cf4f0e218382dde71b1e22a2b03"
CAIRO2="${DOWNLOAD_HOST}mingw32-libcairo2-1.10.2-8.4.noarch.rpm Cairo 1.10.2-8.4 sha1sum:f69af74753c7fcd95b7778eee7c3d731d64749ba"
DBUS="${DOWNLOAD_HOST}mingw32-dbus-1-1.4.16-1.4.noarch.rpm D-Bus 1.4.16-1.4 sha1sum:f623a75dedc9646246582f5c62310627b323b30b"
DBUS_GLIB="${DOWNLOAD_HOST}mingw32-dbus-1-glib-0.92-3.4.noarch.rpm dbus-glib 0.92-3.4 sha1sum:3af1dd35cbe2cdf62ee5144862934f5f8dd5e20d"
ENCHANT="${DOWNLOAD_HOST}mingw32-enchant-1.6.0-3.7.noarch.rpm Enchant 1.6.0-3.4 sha1sum:f7e0571ef98833b087be6c9d71008b3c4c4435d6"
FONTCONFIG="${DOWNLOAD_HOST}mingw32-fontconfig-2.10.1-1.4.noarch.rpm fontconfig 2.10.1-1.4 sha1sum:64fa2a6f8576209dd2253fe52dc59ef8ac92ba6f"
FREETYPE="${DOWNLOAD_HOST}mingw32-freetype-2.4.10-1.4.noarch.rpm freetype 2.4.10-1.4 sha1sum:62a8a494df380c982d6d131ffa0846b498f3d7d0"
GDK_PIXBUF="${DOWNLOAD_HOST}mingw32-gdk-pixbuf-2.26.3-1.4.noarch.rpm gdk-pixbuf 2.26.3-1.4 sha1sum:e454ffc771d923a443553599c1981a3cc9e56bab"
GEOCLUE="${DOWNLOAD_HOST}mingw32-libgeoclue-0.12.99-1.4.noarch.rpm Geoclue 0.12.99-1.4 sha1sum:cf3df30e75c5b38fbe2f63689cadcc2930823b9c"
GLIB="${DOWNLOAD_HOST}mingw32-glib2-2.34.1-1.4.noarch.rpm Glib 2.34.1-1.4 sha1sum:34317487546e5ca5d493c38794c927ff94b6a5b7"
GST="${DOWNLOAD_HOST}mingw32-libgstreamer-0.10.35-1.4.noarch.rpm GStreamer 0.10.35-1.4 sha1sum:fd5bb6f8a9083eb3ca402670e7c38474f7270efe"
GST_INT="${DOWNLOAD_HOST}mingw32-libgstinterfaces-0.10.32-5.4.noarch.rpm GStreamer-interfaces 0.10.32-5.4 sha1sum:cec1dd36bbcc10716e9f8776e4bd53fb0b07d8bb"
GTK2="${DOWNLOAD_HOST}mingw32-gtk2-2.24.14-1.4.noarch.rpm GTK+ 2.24.14-1.4 sha1sum:71971fe63d355aa893536b691f249ace78d89a2b"
LIBFFI="${DOWNLOAD_HOST}mingw32-libffi-3.0.10-2.4.noarch.rpm libffi 3.0.10-2.4 sha1sum:871f13d5f02c03d62b882cc1fa4c98dcff76d4c5"
LIBGCC="${DOWNLOAD_HOST}mingw32-libgcc-4.7.2-2.4.noarch.rpm libgcc 4.7.2-2.4 sha1sum:9023897a5faf380efc89699ac5145c985d03a8bf"
LIBJASPER="${DOWNLOAD_HOST}mingw32-libjasper-1.900.1-6.4.noarch.rpm JasPer 1.900.1-6.4 sha1sum:25e01ed277b8dda6191afb7dd0e0928558c1f2d6"
LIBICU="${DOWNLOAD_HOST}mingw32-libicu-51.1-2.1.noarch.rpm ICU 51.1-2.1 sha1sum:97ec8264e38abceeadda4631730bb0b5f3f3ebfe"
LIBINTL="${DOWNLOAD_HOST}mingw32-libintl-0.18.1.1-13.4.noarch.rpm libintl 0.18.1.1-13.4 sha1sum:043c3b8eb9c872681faed5ec5263456a24bf29e4"
LIBJPEG="${DOWNLOAD_HOST}mingw32-libjpeg-8d-3.4.noarch.rpm libjpeg 8d-3.4 sha1sum:5d1db1eaabc6ababbed648408adbbe6eee0292fc"
LIBJSON="${DOWNLOAD_HOST}mingw32-libjson-glib-0.14.2-1.4.noarch.rpm json-glib 0.14.2-1.4 sha1sum:698194c97baf732bd6b109778f2834d71eedc524"
LIBLZMA="${DOWNLOAD_HOST}mingw32-liblzma-5.0.4-1.4.noarch.rpm liblzma 5.0.4-1.4 sha1sum:ef360fab4b6c77d1618891ccc8f52c2421f37c09"
LIBPNG="${DOWNLOAD_HOST}mingw32-libpng-1.5.11-1.4.noarch.rpm libpng 1.5.11-1.4 sha1sum:99d0a077f70e83f7df10f28915a2137a0ff34462"
LIBSOUP="${DOWNLOAD_HOST}mingw32-libsoup-2.40.3-1.4.noarch.rpm libsoup 2.40.3-1.4 sha1sum:3f2d836acea04741508a341b2bddaa55fd49f720"
LIBSTDCPP="${DOWNLOAD_HOST}mingw32-libstdc++-4.7.2-2.4.noarch.rpm libstdc++ 4.7.2-2.4 sha1sum:e031fad6b7bf54c9846d5a857bb8de6faefdcd1b"
LIBTIFF="${DOWNLOAD_HOST}mingw32-libtiff-4.0.2-1.4.noarch.rpm libtiff 4.0.2-1.4 sha1sum:9a8f8b018e8bafd47067fe6fd0debc1e887239b1"
LIBXSLT="${DOWNLOAD_HOST}mingw32-libxslt-1.1.27-1.4.noarch.rpm libxslt 1.1.27-1.4 sha1sum:4a08612ad73235b0fab95e17644d72e8f24097c3"
PANGO="${DOWNLOAD_HOST}mingw32-pango-1.30.1-1.4.noarch.rpm Pango 1.30.1-1.4 sha1sum:69c4515babdf99b0ded04c24dc3a7f33debac934"
PIXMAN="${DOWNLOAD_HOST}mingw32-pixman-0.26.0-1.4.noarch.rpm pixman 0.26.0-1.4 sha1sum:f751fe428ea83996daf7e57bff6f4f79361b0d29"
PTHREADS="${DOWNLOAD_HOST}mingw32-pthreads-2.8.0-14.6.noarch.rpm pthreads 2.8.0-14.6 sha1sum:e948ae221f82bbcb4fbfd991638e4170c150fe9f"
SQLITE="${DOWNLOAD_HOST}mingw32-libsqlite-3.7.6.2-1.6.noarch.rpm SQLite 3.7.6.2-1.6 sha1sum:f61529bc0c996d9af28a94648ce6102d579ed928"

#webkit 1.10 crashes when calling document.createElement, so I grabbed 1.8 from openSUSE_Factory instead
#TODO: investigate it
#WEBKITGTK="${DOWNLOAD_HOST}mingw32-libwebkitgtk-1.10.2-1.3.noarch.rpm WebKitGTK+ 1.10.2-1.3 sha1sum:33b558d2110fc2caf2c3c0ab24a6c18645814893"
WEBKITGTK="${DOWNLOAD_HOST}mingw32-libwebkitgtk-1.8.3-1.14.noarch.rpm WebKitGTK+ 1.8.3-1.14 sha1sum:ade86455fc2da257f4fe5831367f500a61a1af9a"

ZLIB="${DOWNLOAD_HOST}mingw32-zlib-1.2.7-1.4.noarch.rpm zlib 1.2.7-1.4 sha1sum:83e91f3b4d14e47131ca33fc69e12b82aabdd589"
ALL="ATK CAIRO2 DBUS DBUS_GLIB ENCHANT FONTCONFIG FREETYPE GDK_PIXBUF GEOCLUE GLIB GST GST_INT GTK2 LIBFFI LIBGCC LIBJASPER LIBICU LIBINTL LIBJPEG LIBJSON LIBLZMA LIBPNG LIBSOUP LIBSTDCPP LIBTIFF LIBXSLT PANGO PIXMAN PTHREADS SQLITE WEBKITGTK ZLIB"

mkdir -p $STAGE_DIR
cd $STAGE_DIR

rm -rf $INSTALL_DIR
mkdir $INSTALL_DIR

#new CONTENTS file
echo Bundle Version $BUNDLE_VERSION > $CONTENTS_FILE

#TODO: temporary mirror also
CPIO_URL="http://dl.dropbox.com/u/5448886/pidgin-win32/cpio/bsdcpio-3.0.3-1.4.zip"
CPIO_SHA1SUM="0cb99adb2c2d759c9a21228223e55c8bf227f736"
CPIO_DIR="bsdcpio"
FILE="bsdcpio.zip"
if [ ! -e "$FILE" ]; then
	echo "Downloading bsdcpio"
	wget -q "$CPIO_URL" -O "$FILE" || exit 1
fi
CHECK_SHA1SUM=`sha1sum $FILE`
CHECK_SHA1SUM=${CHECK_SHA1SUM%%\ *}
if [ "$CHECK_SHA1SUM" != "$CPIO_SHA1SUM" ]; then
	echo "sha1sum ($CHECK_SHA1SUM) for $FILE doesn't match expected value of $CPIO_SHA1SUM"
	rm $FILE
	exit 1
fi
rm -rf $CPIO_DIR
unzip -q $FILE -d . || exit 1

function download_and_extract {
	URL=${1%%\ *}
	VALIDATION=${1##*\ }
	NAME=${1%\ *}
	NAME=${NAME#*\ }
	FILE=$(basename $URL)
	MINGW_DIR="usr/i686-w64-mingw32/sys-root/mingw"
	MINGW_DIR_TOP="usr"
	if [ ! -e $FILE ]; then
		echo Downloading $NAME
		wget -q $URL || exit 1
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
			wget -q "$URL.asc" || exit 1
		fi
		#Use our own keyring to avoid adding stuff to the main keyring
		#This doesn't use $GPG_SIGN because we don't this validation to be bypassed when people are skipping signing output
		GPG_BASE="gpg -q --keyring $STAGE_DIR/$VALIDATION_VALUE-keyring.gpg" 
		if [[ ! -e $STAGE_DIR/$VALIDATION_VALUE-keyring.gpg \
				|| `$GPG_BASE --list-keys "$VALIDATION_VALUE" > /dev/null && echo -n "0"` -ne 0 ]]; then
			touch $STAGE_DIR/$VALIDATION_VALUE-keyring.gpg
		       	$GPG_BASE --no-default-keyring --keyserver pgp.mit.edu --recv-key "$VALIDATION_VALUE" || exit 1
		fi
		$GPG_BASE --verify "$FILE.asc" || (echo "$FILE failed signature verification"; exit 1) || exit 1
	else
		echo "Unrecognized validation type of $VALIDATION_TYPE"
		exit 1
	fi
	EXTENSION=${FILE##*.}
	#This is an OpenSuSE build service RPM
	if [ $EXTENSION == 'rpm' ]; then
		rm -rf $MINGW_DIR_TOP
		bsdcpio/bsdcpio.exe --quiet -f etc/fonts/conf.d -di < $FILE || exit 1
		cp -rf $MINGW_DIR/* $INSTALL_DIR
		rm -rf $MINGW_DIR_TOP
	else
		unzip -q $FILE -d $INSTALL_DIR || exit 1
	fi
	echo "$NAME" >> $CONTENTS_FILE
}

echo "Downloading and extracting components..."
for VAL in $ALL
do
	VAR=${!VAL}
	download_and_extract "$VAR"
done
rm -rf $CPIO_DIR
echo "All components ready"

cp $INSTALL_DIR/bin/libintl-8.dll $INSTALL_DIR/bin/intl.dll

#Default GTK+ Theme to MS-Windows (already set)
#echo gtk-theme-name = \"MS-Windows\" > $INSTALL_DIR/etc/gtk-2.0/gtkrc

#Blow away translations that we don't have in Pidgin (temporarily not included)
#for LOCALE_DIR in $INSTALL_DIR/share/locale/*
#do
#	LOCALE=$(basename $LOCALE_DIR)
#	if [ ! -e $PIDGIN_BASE/po/$LOCALE.po ]; then
#		echo Removing $LOCALE translation as it is missing from Pidgin
#		rm -r $LOCALE_DIR
#	fi
#done

#Generate zip file to be included in installer
rm -f $ZIP_FILE
zip -9 -r $ZIP_FILE Gtk
if [ "x$GPG_SIGN" != "x" ]; then
	($GPG_SIGN -ab $ZIP_FILE && $GPG_SIGN --verify $ZIP_FILE.asc) || exit 1
fi

exit 0
