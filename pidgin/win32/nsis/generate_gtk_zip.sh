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
BUNDLE_SHA1SUM="f9945cbdcd591eed7af569d2c4ac806fe27ea97a"
ZIP_FILE="$PIDGIN_BASE/pidgin/win32/nsis/gtk-runtime-$BUNDLE_VERSION.zip"
#BUNDLE_URL="https://pidgin.im/win32/download_redir.php?version=$PIDGIN_VERSION&gtk_version=$BUNDLE_VERSION&dl_pkg=gtk"
BUNDLE_URL="https://dl.dropbox.com/u/5448886/pidgin-win32/gtk-runtime-2.24.14.0.zip"

function download() {
	if [ -e "$2" ]; then
		echo "File exists"
		exit 1
	fi
	failed=0
	wget -t 5 "$1" -O "$2" -o "wget.log" --retry-connrefused --waitretry=5 \
		--ca-certificate="${STAGE_DIR}/../cacert.pem" \
		|| failed=1
	if [ $failed != 0 ] ; then
		if [ "$3" != "quiet" ] ; then
			echo "Download failed"
			cat "wget.log"
		fi
		rm "wget.log"
		rm -f "$2"
		return 1
	fi
	rm "wget.log"
	return 0
}

cat $PIDGIN_BASE/share/ca-certs/*.pem > $STAGE_DIR/../cacert.pem

#Download the existing file (so that we distribute the exact same file for all releases with the same bundle version)
FILE="$ZIP_FILE"
if [ ! -e "$FILE" ]; then
	echo "Downloading the existing file"
	download "$BUNDLE_URL" "$FILE" "quiet"
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
DOWNLOAD_HOST="https://dl.dropbox.com/u/5448886/pidgin-win32/runtime-deps/"

ATK="${DOWNLOAD_HOST}mingw32-atk-2.8.0-1.5.noarch.rpm ATK 2.8.0-1.5 sha1sum:0c682eadc299963aaa5d7998d655e46ead7d7515"
CAIRO2="${DOWNLOAD_HOST}mingw32-libcairo2-1.10.2-8.12.noarch.rpm Cairo 1.10.2-8.12 sha1sum:e33c58603678a8ba113bffe282bec810bd70172e"
DBUS="${DOWNLOAD_HOST}mingw32-dbus-1-1.7.0-2.1.noarch.rpm D-Bus 1.7.0-2.1 sha1sum:6961e4df15172b057e91b266dbb7cbd01a736e56"
DBUS_GLIB="${DOWNLOAD_HOST}mingw32-dbus-1-glib-0.100.2-1.2.noarch.rpm dbus-glib 0.100.2-1.2 sha1sum:72d27c0c01ce3e94e4dddc653c6ac0544c28362f"
ENCHANT="${DOWNLOAD_HOST}mingw32-enchant-1.6.0-3.9.noarch.rpm Enchant 1.6.0-3.9 sha1sum:a8569c8dcc77f69d065320388fca822ef5bf1fe5"
FONTCONFIG="${DOWNLOAD_HOST}mingw32-fontconfig-2.10.92-1.2.noarch.rpm fontconfig 2.10.92-1.2 sha1sum:d8da351f5d4d9816f02ac5287dd7887c711902ed"
FREETYPE="${DOWNLOAD_HOST}mingw32-freetype-2.4.11-2.1.noarch.rpm freetype 2.4.11-2.1 sha1sum:0eee29696e87d47e7487ba5e90530bf20159f31e"
GDK_PIXBUF="${DOWNLOAD_HOST}mingw32-gdk-pixbuf-2.28.0-1.2.noarch.rpm gdk-pixbuf 2.28.0-1.2 sha1sum:8673e06c3a838e47a093043bf86bb62ea3627fe0"
GEOCLUE="${DOWNLOAD_HOST}mingw32-libgeoclue-0.12.99-1.10.noarch.rpm Geoclue 0.12.99-1.10 sha1sum:84410ca9a6d2fac46217c51e22ebbc5ac3cae040"
GLIB="${DOWNLOAD_HOST}mingw32-glib2-2.36.1-1.1.noarch.rpm Glib 2.36.1-1.1 sha1sum:ed468f064f61c5a12b716c83cba8ccbe05d22992"
GST="${DOWNLOAD_HOST}mingw32-libgstreamer-0.10.35-1.10.noarch.rpm GStreamer 0.10.35-1.10 sha1sum:7097499f3a34b42c25a200fa18f5df6f3f3ba527"
GST_INT="${DOWNLOAD_HOST}mingw32-libgstinterfaces-0.10.32-5.10.noarch.rpm GStreamer-interfaces 0.10.32-5.10 sha1sum:29cfb0668a2e54287ea969eab1a4f3672a09c04a"
GTK2="${DOWNLOAD_HOST}mingw32-gtk2-2.24.14-2.7.noarch.rpm GTK+ 2.24.14-2.7 sha1sum:a6f98a0c974c0273127c6423d89fca5f48c7d30c"
GTKSPELL="${DOWNLOAD_HOST}mingw32-gtkspell-2.0.16-2.10.noarch.rpm GtkSpell 2.0.16-2.10 sha1sum:623afdc7cc2c43c1f5d39be797c3ec8ee1ab5570"
LIBFFI="${DOWNLOAD_HOST}mingw32-libffi-3.0.10-2.7.noarch.rpm libffi 3.0.10-2.7 sha1sum:628b014349dc132d3aa46362b30fc1cdd61f6b97"
LIBGCC="${DOWNLOAD_HOST}mingw32-libgcc-4.8.0-6.1.noarch.rpm libgcc 4.8.0-6.1 sha1sum:ab599bf07bf2d56367c57b442440598358c943af"
LIBGNURX="${DOWNLOAD_HOST}mingw32-libgnurx-2.5-4.6.noarch.rpm libgnurx 2.5-4.6 sha1sum:51571e6b1e5e9fb865c110cae04c582ff3c44cb7"
LIBHB="${DOWNLOAD_HOST}mingw32-libharfbuzz0-0.9.16-3.1.noarch.rpm libharfbuzz 0.9.16-3.1 sha1sum:5c377190429f45e566b07439c99937798c4c13f0"
LIBJASPER="${DOWNLOAD_HOST}mingw32-libjasper-1.900.1-6.6.noarch.rpm JasPer 1.900.1-6.6 sha1sum:1a0f0072e0b0f73bd8d4e26aed93baa10d77e504"
LIBICU="${DOWNLOAD_HOST}mingw32-libicu-51.1-2.3.noarch.rpm ICU 51.1-2.3 sha1sum:c259c9d7f9f58934ebb49ecc80b15b7492e5a245"
LIBINTL="${DOWNLOAD_HOST}mingw32-libintl-0.18.1.1-13.6.noarch.rpm libintl 0.18.1.1-13.6 sha1sum:0e6fde8e86788874366f308e25634f95613e906a"
LIBJPEG="${DOWNLOAD_HOST}mingw32-libjpeg-8d-3.6.noarch.rpm libjpeg 8d-3.6 sha1sum:db85723377243045388a5d3c873262cd83ffa7e2"
LIBJSON="${DOWNLOAD_HOST}mingw32-libjson-glib-0.14.2-2.1.noarch.rpm json-glib 0.14.2-2.1 sha1sum:366bf545855ced7fdfefc57b75ef7bbb5ebc249b"
LIBLZMA="${DOWNLOAD_HOST}mingw32-liblzma-5.0.4-1.6.noarch.rpm liblzma 5.0.4-1.6 sha1sum:67bad5204ae09d163f799adec3286fff297e3bc8"
LIBPNG="${DOWNLOAD_HOST}mingw32-libpng-1.5.11-1.6.noarch.rpm libpng 1.5.11-1.6 sha1sum:bb28549351c1f0d7a8afd129ac656be18a616149"
LIBSILC="${DOWNLOAD_HOST}mingw32-libsilc-1.1.10-2.1.noarch.rpm libsilc 1.1.10-2.1 sha1sum:b7690eac1a91caf2b02b058483a3768705a6f3df"
LIBSILCCL="${DOWNLOAD_HOST}mingw32-libsilcclient-1.1.10-2.1.noarch.rpm libsilcclient 1.1.10-2.1 sha1sum:88b84ff4c43643ce4b8ec1eb345e73c139cc164a"
LIBSOUP="${DOWNLOAD_HOST}mingw32-libsoup-2.42.2-1.1.noarch.rpm libsoup 2.42.2-1.1 sha1sum:f0af29ceb420daaa549dd5dc470fbd62bc732252"
LIBSSP="${DOWNLOAD_HOST}mingw32-libssp-4.8.0-6.1.noarch.rpm LibSSP 4.8.0-6.1 sha1sum:c05b2e0470f41d26f8ebfff93dfd51263842a4ea"
LIBSTDCPP="${DOWNLOAD_HOST}mingw32-libstdc++-4.8.0-6.1.noarch.rpm libstdc++ 4.8.0-6.1 sha1sum:627860950e951764fe1aa229d3a63bb01618ba90"
LIBTIFF="${DOWNLOAD_HOST}mingw32-libtiff-4.0.2-1.6.noarch.rpm libtiff 4.0.2-1.6 sha1sum:3a082540386748ead608d388ce55a0c1dd28715d"
LIBXML="${DOWNLOAD_HOST}mingw32-libxml2-2.9.0-2.1.noarch.rpm libxml 2.9.0-2.1 sha1sum:de73090544effcd167f94fcfe8e2d1f005adbea7"
LIBXSLT="${DOWNLOAD_HOST}mingw32-libxslt-1.1.28-1.2.noarch.rpm libxslt 1.1.28-1.2 sha1sum:6ee150c6271edded95f92285f59d02c2896e459e"
MEANW="${DOWNLOAD_HOST}mingw32-meanwhile-1.0.2-3.2.noarch.rpm Meanwhile 1.0.2-3.2 sha1sum:6b0fd8d94205d80eba37ea3e3f19ded7a1297473"
MOZNSS="${DOWNLOAD_HOST}mingw32-mozilla-nss-3.14.3-2.2.noarch.rpm NSS 3.14.3-2.2 sha1sum:b0353cda8b5670796f0246f1da27754c3205a009"
MOZNSPR="${DOWNLOAD_HOST}mingw32-mozilla-nspr-4.9.6-4.1.noarch.rpm NSPR 4.9.6-4.1 sha1sum:42b1a803c54639f6cf0aa1f6287324d0658c705c"
PANGO="${DOWNLOAD_HOST}mingw32-pango-1.34.0-2.3.noarch.rpm Pango 1.34.0-2.3 sha1sum:65b55b73c4f5c8107fdf48ef2e4f5c351189cd4f"
PIXMAN="${DOWNLOAD_HOST}mingw32-pixman-0.26.0-1.6.noarch.rpm pixman 0.26.0-1.6 sha1sum:b0a440a3761e77d890a2e7de52405e2ce364c9b2"
PTHREADS="${DOWNLOAD_HOST}mingw32-pthreads-2.8.0-14.6.noarch.rpm pthreads 2.8.0-14.6 sha1sum:e948ae221f82bbcb4fbfd991638e4170c150fe9f"
SQLITE="${DOWNLOAD_HOST}mingw32-libsqlite-3.7.6.2-1.6.noarch.rpm SQLite 3.7.6.2-1.6 sha1sum:f61529bc0c996d9af28a94648ce6102d579ed928"
TCL="${DOWNLOAD_HOST}mingw32-tcl-8.5.9-14.1.noarch.rpm Tcl 8.5.9-14.1 sha1sum:1178fc95ab97d274504f3d76c3ebe9bdbb39847a"
TK="${DOWNLOAD_HOST}mingw32-tk-8.5.9-8.7.noarch.rpm Tk 8.5.9-8.7 sha1sum:4ec1595fdf06eaa9f4d38b433edc430217697995"
WEBKITGTK="${DOWNLOAD_HOST}mingw32-libwebkitgtk-1.10.2-9.2.noarch.rpm WebKitGTK+ 1.10.2-9.2 sha1sum:010dbad413f824696cd1e32fe70046c9a1cb425f"
ZLIB="${DOWNLOAD_HOST}mingw32-zlib-1.2.7-1.4.noarch.rpm zlib 1.2.7-1.4 sha1sum:83e91f3b4d14e47131ca33fc69e12b82aabdd589"

ALL="ATK CAIRO2 DBUS DBUS_GLIB ENCHANT FONTCONFIG FREETYPE GDK_PIXBUF GEOCLUE GLIB GST GST_INT GTK2 GTKSPELL LIBFFI LIBGCC LIBGNURX LIBHB LIBJASPER LIBICU LIBINTL LIBJPEG LIBJSON LIBLZMA LIBPNG LIBSILC LIBSILCCL LIBSOUP LIBSSP LIBSTDCPP LIBTIFF LIBXML LIBXSLT MEANW MOZNSS MOZNSPR PANGO PIXMAN PTHREADS SQLITE TCL TK WEBKITGTK ZLIB"

mkdir -p $STAGE_DIR
cd $STAGE_DIR

rm -rf $INSTALL_DIR
mkdir $INSTALL_DIR

#new CONTENTS file
echo Bundle Version $BUNDLE_VERSION > $CONTENTS_FILE

#TODO: temporary mirror also
CPIO_URL="https://dl.dropbox.com/u/5448886/pidgin-win32/cpio/bsdcpio-3.0.3-1.4.tar.gz"
CPIO_SHA1SUM="0460c7a52f8c93d3c4822d6d1aaf9410f21bd4da"
CPIO_DIR="bsdcpio"
FILE="bsdcpio.tar.gz"
if [ ! -e "$FILE" ]; then
	echo "Downloading bsdcpio"
	download "$CPIO_URL" "$FILE" || exit 1
fi
CHECK_SHA1SUM=`sha1sum $FILE`
CHECK_SHA1SUM=${CHECK_SHA1SUM%%\ *}
if [ "$CHECK_SHA1SUM" != "$CPIO_SHA1SUM" ]; then
	echo "sha1sum ($CHECK_SHA1SUM) for $FILE doesn't match expected value of $CPIO_SHA1SUM"
	rm $FILE
	exit 1
fi
rm -rf "$CPIO_DIR"
mkdir "$CPIO_DIR"
tar xf "$FILE" --strip-components=1 --directory="$CPIO_DIR" || exit 1

function download_and_extract {
	URL=${1%%\ *}
	VALIDATION=${1##*\ }
	NAME=${1%\ *}
	NAME=${NAME#*\ }
	FILE=$(basename $URL)
	MINGW_DIR="usr/i686-w64-mingw32/sys-root/mingw"
	MINGW_DIR_TOP="usr"
	if [ ! -e $FILE ]; then
		echo "Downloading $NAME"
		download "$URL" "$FILE" || exit 1
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
			download "$URL.asc" "$FILE.asc" || exit 1
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
mv "${STAGE_DIR}/${INSTALL_DIR}/share/tcl8.5" "${STAGE_DIR}/${INSTALL_DIR}/lib/"
rm -rf $CPIO_DIR
rm "${STAGE_DIR}/../cacert.pem"

echo "All components ready"

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

if [ "`$GPG_SIGN -K 2> /dev/null`" != "" ]; then
	($GPG_SIGN -ab $ZIP_FILE && $GPG_SIGN --verify $ZIP_FILE.asc) || exit 1
else
	echo "Warning: cannot sign generated bundle"
fi

exit 0
