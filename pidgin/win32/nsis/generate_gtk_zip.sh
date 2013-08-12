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
BUNDLE_SHA1SUM="df28047f00934e6a00a5962387a1005114ec772e"
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
		echo "Couldn't download GTK+ Bundle"
		exit 1
	fi
else
	echo "GTK+ Bundle is up to date"
	exit 0
fi

#DOWNLOAD_HOST="http://download.opensuse.org/repositories/windows:/mingw:/win32/openSUSE_12.3/noarch/"
#TODO: this is just a temporary mirror - Tomek Wasilczyk's <tomkiewicz@cpw.pidgin.im> Dropbox
DOWNLOAD_HOST="https://dl.dropbox.com/u/5448886/pidgin-win32/runtime-deps/"

ALL=""

ARC_ATK="${DOWNLOAD_HOST}mingw32-atk-2.8.0-1.5.noarch.rpm ATK 2.8.0-1.5 sha1sum:0c682eadc299963aaa5d7998d655e46ead7d7515"
ALL+="ARC_ATK "

ARC_CAIRO2="${DOWNLOAD_HOST}mingw32-libcairo2-1.10.2-8.12.noarch.rpm Cairo 1.10.2-8.12 sha1sum:e33c58603678a8ba113bffe282bec810bd70172e"
ALL+="ARC_CAIRO2 "

ARC_DBUS="${DOWNLOAD_HOST}mingw32-dbus-1-1.7.0-2.1.noarch.rpm D-Bus 1.7.0-2.1 sha1sum:6961e4df15172b057e91b266dbb7cbd01a736e56"
ALL+="ARC_DBUS "

ARC_DBUS_GLIB="${DOWNLOAD_HOST}mingw32-dbus-1-glib-0.100.2-1.2.noarch.rpm dbus-glib 0.100.2-1.2 sha1sum:72d27c0c01ce3e94e4dddc653c6ac0544c28362f"
ALL+="ARC_DBUS_GLIB "

ARC_ENCHANT="${DOWNLOAD_HOST}mingw32-enchant-1.6.0-3.9.noarch.rpm Enchant 1.6.0-3.9 sha1sum:a8569c8dcc77f69d065320388fca822ef5bf1fe5"
ALL+="ARC_ENCHANT "

ARC_FONTCONFIG="${DOWNLOAD_HOST}mingw32-fontconfig-2.10.92-1.2.noarch.rpm fontconfig 2.10.92-1.2 sha1sum:d8da351f5d4d9816f02ac5287dd7887c711902ed"
ALL+="ARC_FONTCONFIG "

ARC_FREETYPE="${DOWNLOAD_HOST}mingw32-freetype-2.4.11-2.1.noarch.rpm freetype 2.4.11-2.1 sha1sum:0eee29696e87d47e7487ba5e90530bf20159f31e"
ALL+="ARC_FREETYPE "

ARC_GDK_PIXBUF="${DOWNLOAD_HOST}mingw32-gdk-pixbuf-2.28.0-1.2.noarch.rpm gdk-pixbuf 2.28.0-1.2 sha1sum:8673e06c3a838e47a093043bf86bb62ea3627fe0"
ALL+="ARC_GDK_PIXBUF "

ARC_GEOCLUE="${DOWNLOAD_HOST}mingw32-libgeoclue-0.12.99-1.10.noarch.rpm Geoclue 0.12.99-1.10 sha1sum:84410ca9a6d2fac46217c51e22ebbc5ac3cae040"
ALL+="ARC_GEOCLUE "

ARC_GLIB="${DOWNLOAD_HOST}mingw32-glib2-2.36.1-1.1.noarch.rpm Glib 2.36.1-1.1 sha1sum:ed468f064f61c5a12b716c83cba8ccbe05d22992"
ALL+="ARC_GLIB "

ARC_GNUTLS="${DOWNLOAD_HOST}mingw32-libgnutls-2.12.22-2.2.noarch.rpm GnuTLS 2.12.22-2.2 sha1sum:ee65a8971582f55aa469dbce82eb180fb1b35705"
ALL+="ARC_GNUTLS "

ARC_GNUTLS_GCRYPT="${DOWNLOAD_HOST}mingw32-libgcrypt-1.5.2-1.1.noarch.rpm libgcrypt 1.5.2-1.1 sha1sum:861335a6edaa8419bc8f2d4ba6104c8da197e8e2"
ALL+="ARC_GNUTLS_GCRYPT "

ARC_GNUTLS_GPGERR="${DOWNLOAD_HOST}mingw32-libgpg-error-1.10-1.6.noarch.rpm gpg-error 1.10-1.6 sha1sum:51a649ee41167ed3fafd243f8ded5d30b53f213d"
ALL+="ARC_GNUTLS_GPGERR "

ARC_GTK2="${DOWNLOAD_HOST}mingw32-gtk2-2.24.14-2.7.noarch.rpm GTK+ 2.24.14-2.7 sha1sum:a6f98a0c974c0273127c6423d89fca5f48c7d30c"
ALL+="ARC_GTK2 "

ARC_GTKSPELL="${DOWNLOAD_HOST}mingw32-gtkspell-2.0.16-2.10.noarch.rpm GtkSpell 2.0.16-2.10 sha1sum:623afdc7cc2c43c1f5d39be797c3ec8ee1ab5570"
ALL+="ARC_GTKSPELL "

ARC_LIBFFI="${DOWNLOAD_HOST}mingw32-libffi-3.0.10-2.7.noarch.rpm libffi 3.0.10-2.7 sha1sum:628b014349dc132d3aa46362b30fc1cdd61f6b97"
ALL+="ARC_LIBFFI "

ARC_LIBGCC="${DOWNLOAD_HOST}mingw32-libgcc-4.8.0-6.1.noarch.rpm libgcc 4.8.0-6.1 sha1sum:ab599bf07bf2d56367c57b442440598358c943af"
ALL+="ARC_LIBGCC "

ARC_LIBGNURX="${DOWNLOAD_HOST}mingw32-libgnurx-2.5-4.6.noarch.rpm libgnurx 2.5-4.6 sha1sum:51571e6b1e5e9fb865c110cae04c582ff3c44cb7"
ALL+="ARC_LIBGNURX "

ARC_LIBHB="${DOWNLOAD_HOST}mingw32-libharfbuzz0-0.9.16-3.1.noarch.rpm libharfbuzz 0.9.16-3.1 sha1sum:5c377190429f45e566b07439c99937798c4c13f0"
ALL+="ARC_LIBHB "

ARC_LIBJASPER="${DOWNLOAD_HOST}mingw32-libjasper-1.900.1-6.6.noarch.rpm JasPer 1.900.1-6.6 sha1sum:1a0f0072e0b0f73bd8d4e26aed93baa10d77e504"
ALL+="ARC_LIBJASPER "

ARC_LIBICU="${DOWNLOAD_HOST}mingw32-libicu-51.1-2.3.noarch.rpm ICU 51.1-2.3 sha1sum:c259c9d7f9f58934ebb49ecc80b15b7492e5a245"
ALL+="ARC_LIBICU "

ARC_LIBINTL="${DOWNLOAD_HOST}mingw32-libintl-0.18.1.1-13.6.noarch.rpm libintl 0.18.1.1-13.6 sha1sum:0e6fde8e86788874366f308e25634f95613e906a"
ALL+="ARC_LIBINTL "

ARC_LIBJPEG="${DOWNLOAD_HOST}mingw32-libjpeg-8d-3.6.noarch.rpm libjpeg 8d-3.6 sha1sum:db85723377243045388a5d3c873262cd83ffa7e2"
ALL+="ARC_LIBJPEG "

ARC_LIBJSON="${DOWNLOAD_HOST}mingw32-libjson-glib-0.14.2-2.1.noarch.rpm json-glib 0.14.2-2.1 sha1sum:366bf545855ced7fdfefc57b75ef7bbb5ebc249b"
ALL+="ARC_LIBJSON "

ARC_LIBLZMA="${DOWNLOAD_HOST}mingw32-liblzma-5.0.4-1.6.noarch.rpm liblzma 5.0.4-1.6 sha1sum:67bad5204ae09d163f799adec3286fff297e3bc8"
ALL+="ARC_LIBLZMA "

ARC_LIBPNG="${DOWNLOAD_HOST}mingw32-libpng-1.5.11-1.6.noarch.rpm libpng 1.5.11-1.6 sha1sum:bb28549351c1f0d7a8afd129ac656be18a616149"
ALL+="ARC_LIBPNG "

ARC_LIBSILC="${DOWNLOAD_HOST}mingw32-libsilc-1.1.10-2.1.noarch.rpm libsilc 1.1.10-2.1 sha1sum:b7690eac1a91caf2b02b058483a3768705a6f3df"
ALL+="ARC_LIBSILC "

ARC_LIBSILCCL="${DOWNLOAD_HOST}mingw32-libsilcclient-1.1.10-2.1.noarch.rpm libsilcclient 1.1.10-2.1 sha1sum:88b84ff4c43643ce4b8ec1eb345e73c139cc164a"
ALL+="ARC_LIBSILCCL "

ARC_LIBSOUP="${DOWNLOAD_HOST}mingw32-libsoup-2.42.2-1.1.noarch.rpm libsoup 2.42.2-1.1 sha1sum:f0af29ceb420daaa549dd5dc470fbd62bc732252"
ALL+="ARC_LIBSOUP "

ARC_LIBSSP="${DOWNLOAD_HOST}mingw32-libssp-4.8.0-6.1.noarch.rpm LibSSP 4.8.0-6.1 sha1sum:c05b2e0470f41d26f8ebfff93dfd51263842a4ea"
ALL+="ARC_LIBSSP "

ARC_LIBSTDCPP="${DOWNLOAD_HOST}mingw32-libstdc++-4.8.0-6.1.noarch.rpm libstdc++ 4.8.0-6.1 sha1sum:627860950e951764fe1aa229d3a63bb01618ba90"
ALL+="ARC_LIBSTDCPP "

ARC_LIBTIFF="${DOWNLOAD_HOST}mingw32-libtiff-4.0.2-1.6.noarch.rpm libtiff 4.0.2-1.6 sha1sum:3a082540386748ead608d388ce55a0c1dd28715d"
ALL+="ARC_LIBTIFF "

ARC_LIBXML="${DOWNLOAD_HOST}mingw32-libxml2-2.9.0-2.1.noarch.rpm libxml 2.9.0-2.1 sha1sum:de73090544effcd167f94fcfe8e2d1f005adbea7"
ALL+="ARC_LIBXML "

ARC_LIBXSLT="${DOWNLOAD_HOST}mingw32-libxslt-1.1.28-1.2.noarch.rpm libxslt 1.1.28-1.2 sha1sum:6ee150c6271edded95f92285f59d02c2896e459e"
ALL+="ARC_LIBXSLT "

ARC_MEANW="${DOWNLOAD_HOST}mingw32-meanwhile-1.0.2-3.2.noarch.rpm Meanwhile 1.0.2-3.2 sha1sum:6b0fd8d94205d80eba37ea3e3f19ded7a1297473"
ALL+="ARC_MEANW "

ARC_MOZNSS="${DOWNLOAD_HOST}mingw32-mozilla-nss-3.14.3-2.2.noarch.rpm NSS 3.14.3-2.2 sha1sum:b0353cda8b5670796f0246f1da27754c3205a009"
ALL+="ARC_MOZNSS "

ARC_MOZNSPR="${DOWNLOAD_HOST}mingw32-mozilla-nspr-4.9.6-4.1.noarch.rpm NSPR 4.9.6-4.1 sha1sum:42b1a803c54639f6cf0aa1f6287324d0658c705c"
ALL+="ARC_MOZNSPR "

ARC_PANGO="${DOWNLOAD_HOST}mingw32-pango-1.34.0-2.3.noarch.rpm Pango 1.34.0-2.3 sha1sum:65b55b73c4f5c8107fdf48ef2e4f5c351189cd4f"
ALL+="ARC_PANGO "

ARC_PIXMAN="${DOWNLOAD_HOST}mingw32-pixman-0.26.0-1.6.noarch.rpm pixman 0.26.0-1.6 sha1sum:b0a440a3761e77d890a2e7de52405e2ce364c9b2"
ALL+="ARC_PIXMAN "

ARC_PTHREADS="${DOWNLOAD_HOST}mingw32-pthreads-2.8.0-14.6.noarch.rpm pthreads 2.8.0-14.6 sha1sum:e948ae221f82bbcb4fbfd991638e4170c150fe9f"
ALL+="ARC_PTHREADS "

ARC_SQLITE="${DOWNLOAD_HOST}mingw32-libsqlite-3.7.6.2-1.6.noarch.rpm SQLite 3.7.6.2-1.6 sha1sum:f61529bc0c996d9af28a94648ce6102d579ed928"
ALL+="ARC_SQLITE "

ARC_VV_FARST="${DOWNLOAD_HOST}mingw32-farstream-0.1.2-19.1.noarch.rpm farstream 0.1.2-19.1 sha1sum:1c46b2c2f6b669824a70ca05d3958b2dc2197dc3"
ALL+="ARC_VV_FARST "

ARC_VV_GST="${DOWNLOAD_HOST}mingw32-gstreamer-0.10.36-10.1.noarch.rpm GStreamer 0.10.36-10.1 sha1sum:79ec8a1c0613211345efd0e88e7f3e5668e01150"
ALL+="ARC_VV_GST "

ARC_VV_GST_LIB="${DOWNLOAD_HOST}mingw32-libgstreamer-0.10.36-10.1.noarch.rpm GStreamer-libgstreamer 0.10.36-10.1 sha1sum:3066c1047fba2181e688671fa9e12ed818d7df4a"
ALL+="ARC_VV_GST_LIB "

ARC_VV_GST_INT="${DOWNLOAD_HOST}mingw32-libgstinterfaces-0.10.36-15.1.noarch.rpm GStreamer-interfaces 0.10.36-15.1 sha1sum:77123565b8254bd2a741fce3b769ccaa2d5297f9"
ALL+="ARC_VV_GST_INT "

ARC_VV_GST_PLBAD="${DOWNLOAD_HOST}mingw32-gst-plugins-bad-0.10.23-28.1.noarch.rpm GStreamer-plugins-bad 0.10.23-28.1 sha1sum:994ff1e2c7f839a5ef2dedfb01eeed0d9fcd4be3"
ALL+="ARC_VV_GST_PLBAD "

ARC_VV_GST_PLBASE="${DOWNLOAD_HOST}mingw32-gst-plugins-base-0.10.36-15.1.noarch.rpm GStreamer-plugins-base 0.10.36-15.1 sha1sum:e9026a63c3c7d3f3f8d8ccbf255ed8e865b34c39"
ALL+="ARC_VV_GST_PLBASE "

ARC_VV_GST_PLGOOD="${DOWNLOAD_HOST}mingw32-gst-plugins-good-0.10.31-5.1.noarch.rpm GStreamer-plugins-good 0.10.31-5.1 sha1sum:96f24a739f2f243ab55bc3cc159d43256b464861"
ALL+="ARC_VV_GST_PLGOOD "

ARC_VV_LIBNICE="${DOWNLOAD_HOST}mingw32-libnice-0.1.4-19.1.noarch.rpm libnice 0.1.4-19.1 sha1sum:c1916d36f8083f76ef19437ddd255b12077ad465"
ALL+="ARC_VV_LIBNICE "

ARC_VV_LIBOGG="${DOWNLOAD_HOST}mingw32-libogg-1.3.0-1.8.noarch.rpm libogg 1.3.0-1.8 sha1sum:1978cbd5148630fc95d4a6b1c5024f76f519fcd4"
ALL+="ARC_VV_LIBOGG "

ARC_VV_LIBTHEORA="${DOWNLOAD_HOST}mingw32-libtheora-1.1.1-5.8.noarch.rpm libtheora 1.1.1-5.8 sha1sum:9809978e4e7c0a620dd735218bb1bd317fe32149"
ALL+="ARC_VV_LIBTHEORA "

ARC_VV_LIBVORBIS="${DOWNLOAD_HOST}mingw32-libvorbis-1.3.3-1.8.noarch.rpm libvorbis 1.3.3-1.8 sha1sum:c9efd698ed62c26cf62442dafc2d9d2dcbcd651c"
ALL+="ARC_VV_LIBVORBIS "

ARC_WEBKITGTK="${DOWNLOAD_HOST}mingw32-libwebkitgtk-1.10.2-9.2.noarch.rpm WebKitGTK+ 1.10.2-9.2 sha1sum:010dbad413f824696cd1e32fe70046c9a1cb425f"
ALL+="ARC_WEBKITGTK "

ARC_ZLIB="${DOWNLOAD_HOST}mingw32-zlib-1.2.7-1.4.noarch.rpm zlib 1.2.7-1.4 sha1sum:83e91f3b4d14e47131ca33fc69e12b82aabdd589"
ALL+="ARC_ZLIB "

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

function rpm_install {
	PKG_NAME=${NAME%%\ *}
	if [ "$PKG_NAME" = "GStreamer-plugins-bad" ]; then
		cp $MINGW_DIR/lib/gstreamer-0.10/libgstdirectdrawsink.dll $INSTALL_DIR/lib/gstreamer-0.10/
		cp $MINGW_DIR/lib/gstreamer-0.10/libgstdirectsoundsrc.dll $INSTALL_DIR/lib/gstreamer-0.10/
		cp $MINGW_DIR/lib/gstreamer-0.10/libgstliveadder.dll $INSTALL_DIR/lib/gstreamer-0.10/
		cp $MINGW_DIR/lib/gstreamer-0.10/libgstrtpmux.dll $INSTALL_DIR/lib/gstreamer-0.10/
		cp $MINGW_DIR/lib/gstreamer-0.10/libgstwinks.dll $INSTALL_DIR/lib/gstreamer-0.10/
	elif [ "$PKG_NAME" = "GStreamer-plugins-good" ]; then
		cp $MINGW_DIR/lib/gstreamer-0.10/libgstalaw.dll $INSTALL_DIR/lib/gstreamer-0.10/
		cp $MINGW_DIR/lib/gstreamer-0.10/libgstdirectsoundsink.dll $INSTALL_DIR/lib/gstreamer-0.10/
		cp $MINGW_DIR/lib/gstreamer-0.10/libgstlevel.dll $INSTALL_DIR/lib/gstreamer-0.10/
		cp $MINGW_DIR/lib/gstreamer-0.10/libgstmulaw.dll $INSTALL_DIR/lib/gstreamer-0.10/
		cp $MINGW_DIR/lib/gstreamer-0.10/libgstrtp.dll $INSTALL_DIR/lib/gstreamer-0.10/
		cp $MINGW_DIR/lib/gstreamer-0.10/libgstrtpmanager.dll $INSTALL_DIR/lib/gstreamer-0.10/
		cp $MINGW_DIR/lib/gstreamer-0.10/libgstrtsp.dll $INSTALL_DIR/lib/gstreamer-0.10/
		cp $MINGW_DIR/lib/gstreamer-0.10/libgstwavparse.dll $INSTALL_DIR/lib/gstreamer-0.10/
	else
		cp -rf $MINGW_DIR/* $INSTALL_DIR
	fi
}

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
	else
		echo "Extracting $NAME"
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
		rpm_install
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
rm "${STAGE_DIR}/../cacert.pem"

#mv "${STAGE_DIR}/${INSTALL_DIR}/share/tcl8.5" "${STAGE_DIR}/${INSTALL_DIR}/lib/"
rm "${STAGE_DIR}/${INSTALL_DIR}/lib/gstreamer-0.10/libfsmsnconference.dll"
rm "${STAGE_DIR}/${INSTALL_DIR}/lib/gstreamer-0.10/libgstgnomevfs.dll"

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
