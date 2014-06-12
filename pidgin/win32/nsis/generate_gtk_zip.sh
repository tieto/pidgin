#!/bin/bash
# Script to generate zip file for GTK+ runtime to be included in Pidgin installer

PIDGIN_BASE=`pwd`
PIDGIN_BASE="$PIDGIN_BASE/../../.."
GPG_SIGN=$1

if [ ! -e $PIDGIN_BASE/ChangeLog ]; then
	echo "Pidgin base directory not found"
	exit 1
fi

if [ ! -e $PIDGIN_BASE/VERSION ]; then
	cd ../../..
	make -f Makefile.mingw VERSION
	cd - > /dev/null
fi

STAGE_DIR=`readlink -f $PIDGIN_BASE/pidgin/win32/nsis/gtk_runtime_stage`
CERT_PATH=`readlink -f $PIDGIN_BASE/pidgin/win32/nsis`/cacert.pem
#Subdirectory of $STAGE_DIR
INSTALL_DIR=Gtk
CONTENTS_FILE=$INSTALL_DIR/CONTENTS
PIDGIN_VERSION=$( < $PIDGIN_BASE/VERSION )

#This needs to be changed every time there is any sort of change.
BUNDLE_VERSION=2.24.18.0
BUNDLE_SHA1SUM="5957b0bf3f5e520863cf8ba64db7592383e9dd42"
ZIP_FILE="$PIDGIN_BASE/pidgin/win32/nsis/gtk-runtime-$BUNDLE_VERSION.zip"
#BUNDLE_URL="https://pidgin.im/win32/download_redir.php?version=$PIDGIN_VERSION&gtk_version=$BUNDLE_VERSION&dl_pkg=gtk"
BUNDLE_URL="https://pidgin.im/~twasilczyk/win32/gtk-runtime-$BUNDLE_VERSION.zip"

if [ "x`uname`" == "xLinux" ]; then
	is_win32="no"
else
	is_win32="yes"
fi

function download() {
	if [ -e "$2" ]; then
		echo "File exists"
		exit 1
	fi
	failed=0
	wget -t 5 "$1" -O "$2" -o "wget.log" --retry-connrefused --waitretry=5 \
		--ca-certificate="$CERT_PATH" \
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

cat $PIDGIN_BASE/share/ca-certs/*.pem > "$CERT_PATH"

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

# origin: http://download.opensuse.org/repositories/windows:/mingw:/win32/openSUSE_12.3/noarch/
DOWNLOAD_HOST="https://pidgin.im/~twasilczyk/win32/runtime-deps/"

ALL=""

ARC_ATK="${DOWNLOAD_HOST}mingw32-atk-2.8.0-1.5.noarch.rpm ATK 2.8.0-1.5 sha1sum:0c682eadc299963aaa5d7998d655e46ead7d7515"
ALL+="ARC_ATK "

ARC_CAIRO2="${DOWNLOAD_HOST}mingw32-libcairo2-1.10.2-8.12.noarch.rpm Cairo 1.10.2-8.12 sha1sum:e33c58603678a8ba113bffe282bec810bd70172e"
ALL+="ARC_CAIRO2 "

ARC_DBUS="${DOWNLOAD_HOST}mingw32-dbus-1-1.8.0-1.19.noarch.rpm D-Bus 1.8.0-1.19 sha1sum:c951d935f58212abb70ba5edf1794390f8434832"
ALL+="ARC_DBUS "

ARC_DBUS_GLIB="${DOWNLOAD_HOST}mingw32-dbus-1-glib-0.100.2-1.2.noarch.rpm dbus-glib 0.100.2-1.2 sha1sum:72d27c0c01ce3e94e4dddc653c6ac0544c28362f"
ALL+="ARC_DBUS_GLIB "

ARC_ENCHANT="${DOWNLOAD_HOST}mingw32-enchant-1.6.0-3.9.noarch.rpm Enchant 1.6.0-3.9 sha1sum:a8569c8dcc77f69d065320388fca822ef5bf1fe5"
ALL+="ARC_ENCHANT "

ARC_FONTCONFIG="${DOWNLOAD_HOST}mingw32-fontconfig-2.10.92-1.2.noarch.rpm fontconfig 2.10.92-1.2 sha1sum:d8da351f5d4d9816f02ac5287dd7887c711902ed"
ALL+="ARC_FONTCONFIG "

ARC_FREETYPE="${DOWNLOAD_HOST}mingw32-freetype-2.4.12-3.10.noarch.rpm freetype 2.4.12-3.10 sha1sum:d0c8fc8ba3785f5445a429ae7b6a3fce6d1f0333"
ALL+="ARC_FREETYPE "

ARC_GDK_PIXBUF="${DOWNLOAD_HOST}mingw32-gdk-pixbuf-2.28.0-1.2.noarch.rpm gdk-pixbuf 2.28.0-1.2 sha1sum:8673e06c3a838e47a093043bf86bb62ea3627fe0"
ALL+="ARC_GDK_PIXBUF "

ARC_GEOCLUE="${DOWNLOAD_HOST}mingw32-libgeoclue-0.12.99-1.10.noarch.rpm Geoclue 0.12.99-1.10 sha1sum:84410ca9a6d2fac46217c51e22ebbc5ac3cae040"
ALL+="ARC_GEOCLUE "

ARC_GLIB="${DOWNLOAD_HOST}mingw32-glib2-2.38.0-1.4.noarch.rpm Glib 2.38.0-1.4 sha1sum:e71d8c2f105548e752df4a0b6ba5958ab4826707"
ALL+="ARC_GLIB "

ARC_GNUTLS="${DOWNLOAD_HOST}mingw32-libgnutls-3.1.22-1.3.noarch.rpm GnuTLS 3.1.22-1.3 sha1sum:e744435362f4124210a7b70d75f154b7ac0bf1fd"
ALL+="ARC_GNUTLS "

ARC_GNUTLS_GCRYPT="${DOWNLOAD_HOST}mingw32-libgcrypt-1.6.0-2.4.noarch.rpm libgcrypt 1.6.0-2.4 sha1sum:1d6ce973a866d93dc083888b19d64a7891ff224f"
ALL+="ARC_GNUTLS_GCRYPT "

ARC_GNUTLS_GPGERR="${DOWNLOAD_HOST}mingw32-libgpg-error-1.12-2.5.noarch.rpm gpg-error 1.12-2.5 sha1sum:af3a21a58fce483196e882ab1bf801203535f46e"
ALL+="ARC_GNUTLS_GPGERR "

ARC_GTK2="${DOWNLOAD_HOST}mingw32-gtk2-2.24.18-3.4.noarch.rpm GTK+ 2.24.18-3.4 sha1sum:31bc7f8aa2222517a3ec614924a07432983dc20d"
ALL+="ARC_GTK2 "

ARC_LIBFFI="${DOWNLOAD_HOST}mingw32-libffi-3.0.13-2.2.noarch.rpm libffi 3.0.13-2.2 sha1sum:0751dddb44eba3f553534c0a2a8ed438ed84a793"
ALL+="ARC_LIBFFI "

ARC_LIBGADU="${DOWNLOAD_HOST}mingw32-libgadu-1.12.0rc3-6.1.noarch.rpm libgadu 1.12.0rc3-6.1 sha1sum:ec2f3ccbc850c29bb26318a46ccba4db39b0a328"
ALL+="ARC_LIBGADU "

ARC_LIBGCC="${DOWNLOAD_HOST}mingw32-libgcc-4.8.2-3.7.noarch.rpm libgcc 4.8.2-3.7 sha1sum:91ab8f6881ce1004dda5279920742f78743e897b"
ALL+="ARC_LIBGCC "

ARC_LIBGMP="${DOWNLOAD_HOST}mingw32-libgmp-5.0.5-2.2.noarch.rpm libgmp 5.0.5-2.2 sha1sum:30c8c403d4d2dead7674e567d83c8c069b603e49"
ALL+="ARC_LIBGMP "

ARC_LIBGNURX="${DOWNLOAD_HOST}mingw32-libgnurx-2.5-4.6.noarch.rpm libgnurx 2.5-4.6 sha1sum:51571e6b1e5e9fb865c110cae04c582ff3c44cb7"
ALL+="ARC_LIBGNURX "

ARC_LIBHB="${DOWNLOAD_HOST}mingw32-libharfbuzz-0.9.19-3.5.noarch.rpm libharfbuzz 0.9.19-3.5 sha1sum:a31cd8ed4a7a8b75cf7b1252706f79b19e612d68"
ALL+="ARC_LIBHB "

ARC_LIBHOGWEED="${DOWNLOAD_HOST}mingw32-libhogweed-2.7-2.2.noarch.rpm libhogweed 2.7-2.2 sha1sum:c22ea84a8a5037be6021f9494b8252861dee63b5"
ALL+="ARC_LIBHOGWEED "

ARC_LIBJASPER="${DOWNLOAD_HOST}mingw32-libjasper-1.900.1-6.6.noarch.rpm JasPer 1.900.1-6.6 sha1sum:1a0f0072e0b0f73bd8d4e26aed93baa10d77e504"
ALL+="ARC_LIBJASPER "

ARC_LIBICU="${DOWNLOAD_HOST}mingw32-libicu-51.1-2.3.noarch.rpm ICU 51.1-2.3 sha1sum:c259c9d7f9f58934ebb49ecc80b15b7492e5a245"
ALL+="ARC_LIBICU "

ARC_LIBIDN="${DOWNLOAD_HOST}mingw32-libidn-1.22-3.8.noarch.rpm libidn 1.22-3.8 sha1sum:2052ea6fc2e789b2c252f621a7134ea4286cf5cc"
ALL+="ARC_LIBIDN "

ARC_LIBINTL="${DOWNLOAD_HOST}mingw32-libintl-0.18.1.1-15.11.noarch.rpm libintl 0.18.1.1-15.11 sha1sum:29812544f7362dde1378f71feb31fed4f9cb640e"
ALL+="ARC_LIBINTL "

ARC_LIBJPEG="${DOWNLOAD_HOST}mingw32-libjpeg-8d-3.6.noarch.rpm libjpeg 8d-3.6 sha1sum:db85723377243045388a5d3c873262cd83ffa7e2"
ALL+="ARC_LIBJPEG "

ARC_LIBJSON="${DOWNLOAD_HOST}mingw32-libjson-glib-0.14.2-2.1.noarch.rpm json-glib 0.14.2-2.1 sha1sum:366bf545855ced7fdfefc57b75ef7bbb5ebc249b"
ALL+="ARC_LIBJSON "

ARC_LIBLZMA="${DOWNLOAD_HOST}mingw32-liblzma-5.0.4-1.6.noarch.rpm liblzma 5.0.4-1.6 sha1sum:67bad5204ae09d163f799adec3286fff297e3bc8"
ALL+="ARC_LIBLZMA "

ARC_LIBNETTLE="${DOWNLOAD_HOST}mingw32-libnettle-2.7-2.2.noarch.rpm libnettle 2.7-2.2 sha1sum:45337e6ccb46c0752d2761b6c48a20e97dd09195"
ALL+="ARC_LIBNETTLE "

ARC_LIBP11="${DOWNLOAD_HOST}mingw32-libp11-kit0-0.20.1-4.3.noarch.rpm libp11 0.20.1-4.3 sha1sum:ee5b7a3e16b29f26ee1c275d8228ba0bb6a27190"
ALL+="ARC_LIBP11 "

ARC_LIBPNG="${DOWNLOAD_HOST}mingw32-libpng-1.5.17-2.11.noarch.rpm libpng 1.5.17-2.11 sha1sum:ff2c29197b5529d2cdc936f7b2b61af5c1175d29"
ALL+="ARC_LIBPNG "

ARC_LIBSILC="${DOWNLOAD_HOST}mingw32-libsilc-1.1.10-2.1.noarch.rpm libsilc 1.1.10-2.1 sha1sum:b7690eac1a91caf2b02b058483a3768705a6f3df"
ALL+="ARC_LIBSILC "

ARC_LIBSILCCL="${DOWNLOAD_HOST}mingw32-libsilcclient-1.1.10-2.1.noarch.rpm libsilcclient 1.1.10-2.1 sha1sum:88b84ff4c43643ce4b8ec1eb345e73c139cc164a"
ALL+="ARC_LIBSILCCL "

ARC_LIBSOUP="${DOWNLOAD_HOST}mingw32-libsoup-2.42.2-1.1.noarch.rpm libsoup 2.42.2-1.1 sha1sum:f0af29ceb420daaa549dd5dc470fbd62bc732252"
ALL+="ARC_LIBSOUP "

ARC_LIBSSP="${DOWNLOAD_HOST}mingw32-libssp-4.8.2-3.7.noarch.rpm LibSSP 4.8.2-3.7 sha1sum:bc42038985d5c75dd3b1511849db2c9f80dbbd7e"
ALL+="ARC_LIBSSP "

ARC_LIBSTDCPP="${DOWNLOAD_HOST}mingw32-libstdc++-4.8.2-3.7.noarch.rpm libstdc++ 4.8.2-3.7 sha1sum:1f9779c16afb0bbfab6dc79389cf6f6198292062"
ALL+="ARC_LIBSTDCPP "

ARC_LIBTASN="${DOWNLOAD_HOST}mingw32-libtasn1-3.3-3.2.noarch.rpm libtasn 3.3-3.2 sha1sum:3d5bb0df8eb7ed5e3f05b5378d3d61dbbbdbbd3f"
ALL+="ARC_LIBTASN "

ARC_LIBTIFF="${DOWNLOAD_HOST}mingw32-libtiff-4.0.2-1.6.noarch.rpm libtiff 4.0.2-1.6 sha1sum:3a082540386748ead608d388ce55a0c1dd28715d"
ALL+="ARC_LIBTIFF "

ARC_LIBXML="${DOWNLOAD_HOST}mingw32-libxml2-2.9.0-2.1.noarch.rpm libxml 2.9.0-2.1 sha1sum:de73090544effcd167f94fcfe8e2d1f005adbea7"
ALL+="ARC_LIBXML "

ARC_LIBXSLT="${DOWNLOAD_HOST}mingw32-libxslt-1.1.28-1.2.noarch.rpm libxslt 1.1.28-1.2 sha1sum:6ee150c6271edded95f92285f59d02c2896e459e"
ALL+="ARC_LIBXSLT "

ARC_MEANW="${DOWNLOAD_HOST}mingw32-meanwhile-1.0.2-3.2.noarch.rpm Meanwhile 1.0.2-3.2 sha1sum:6b0fd8d94205d80eba37ea3e3f19ded7a1297473"
ALL+="ARC_MEANW "

ARC_MOZNSS="${DOWNLOAD_HOST}mingw32-mozilla-nss-3.14.5-3.9.noarch.rpm NSS 3.14.5-3.9 sha1sum:434bcb5073bae6d16ab248280b2b033507d20453"
ALL+="ARC_MOZNSS "

ARC_MOZNSPR="${DOWNLOAD_HOST}mingw32-mozilla-nspr-4.10.2-2.8.noarch.rpm NSPR 4.10.2-2.8 sha1sum:ca61d4453042725e4f700a4b51859dc5f58110c4"
ALL+="ARC_MOZNSPR "

ARC_NCURSES="${DOWNLOAD_HOST}mingw32-ncurses-5.9-20140422.1.noarch.rpm ncurses 5.9-20140422.1 sha1sum:4873c22e5f67d0bc72bbb89b71a3967bce6067e0"
ALL+="ARC_NCURSES "

ARC_PANGO="${DOWNLOAD_HOST}mingw32-pango-1.34.0-2.3.noarch.rpm Pango 1.34.0-2.3 sha1sum:65b55b73c4f5c8107fdf48ef2e4f5c351189cd4f"
ALL+="ARC_PANGO "

ARC_PIXMAN="${DOWNLOAD_HOST}mingw32-pixman-0.30.0-3.10.noarch.rpm pixman 0.30.0-3.10 sha1sum:ed63261f29c356a58276435df013376e688a3a6b"
ALL+="ARC_PIXMAN "

ARC_PROTOBUFC="${DOWNLOAD_HOST}mingw32-protobuf-c-0.15-6.1.noarch.rpm protobuf-c 0.15-6.1 sha1sum:b58ef0aca3c99d956479ec1510e3ca62d79a443f"
ALL+="ARC_PROTOBUFC "

ARC_PTHREADS="${DOWNLOAD_HOST}mingw32-pthreads-2.8.0-14.6.noarch.rpm pthreads 2.8.0-14.6 sha1sum:e948ae221f82bbcb4fbfd991638e4170c150fe9f"
ALL+="ARC_PTHREADS "

ARC_SQLITE="${DOWNLOAD_HOST}mingw32-libsqlite3-0-3.8.4.1-1.4.noarch.rpm SQLite 3.8.4.1-1.4 sha1sum:1c42db1a48f616d824c3ae8e0a8eb0693ddac88f"
ALL+="ARC_SQLITE "

ARC_VV_FARST="${DOWNLOAD_HOST}mingw32-farstream-0.1.2-5.3.noarch.rpm farstream 0.1.2-5.3 sha1sum:0334213ece2f339cba38aff9290ef07238763c5c"
ALL+="ARC_VV_FARST "

ARC_VV_GST="${DOWNLOAD_HOST}mingw32-gstreamer-0.10.36-6.3.noarch.rpm GStreamer 0.10.36-6.3 sha1sum:3fd80dfc05c64f277d787c60799638701e0f058e"
ALL+="ARC_VV_GST "

ARC_VV_GST_LIB="${DOWNLOAD_HOST}mingw32-libgstreamer-0.10.36-6.3.noarch.rpm GStreamer-libgstreamer 0.10.36-6.3 sha1sum:eef44d1ff93f0c2ddffdbaecc65f08a5617b4724"
ALL+="ARC_VV_GST_LIB "

ARC_VV_GST_INT="${DOWNLOAD_HOST}mingw32-libgstinterfaces-0.10.36-5.4.noarch.rpm GStreamer-interfaces 0.10.36-5.4 sha1sum:d974b38c1da02191103c253e27a15ec7f160000f"
ALL+="ARC_VV_GST_INT "

ARC_VV_GST_PLBAD="${DOWNLOAD_HOST}mingw32-gst-plugins-bad-0.10.23-5.4.noarch.rpm GStreamer-plugins-bad 0.10.23-5.4 sha1sum:d2754a1358edab0c06b4038123274025f58af6ef"
ALL+="ARC_VV_GST_PLBAD "

ARC_VV_GST_PLBASE="${DOWNLOAD_HOST}mingw32-gst-plugins-base-0.10.36-5.4.noarch.rpm GStreamer-plugins-base 0.10.36-5.4 sha1sum:9e642d5a1e71dfeaa5b38b7ebf0ade4442ee763b"
ALL+="ARC_VV_GST_PLBASE "

ARC_VV_GST_PLGOOD="${DOWNLOAD_HOST}mingw32-gst-plugins-good-0.10.31-5.4.noarch.rpm GStreamer-plugins-good 0.10.31-5.4 sha1sum:3e0daa815e4d51749fc6d2e9353245d09ee9854d"
ALL+="ARC_VV_GST_PLGOOD "

ARC_VV_LIBNICE="${DOWNLOAD_HOST}mingw32-libnice-0.1.4-5.3.noarch.rpm libnice 0.1.4-5.3 sha1sum:abbabaa03d81202f2d78adca2b833d1072dfecf0"
ALL+="ARC_VV_LIBNICE "

ARC_VV_LIBOGG="${DOWNLOAD_HOST}mingw32-libogg-1.3.0-1.8.noarch.rpm libogg 1.3.0-1.8 sha1sum:1978cbd5148630fc95d4a6b1c5024f76f519fcd4"
ALL+="ARC_VV_LIBOGG "

ARC_VV_LIBTHEORA="${DOWNLOAD_HOST}mingw32-libtheora-1.1.1-5.8.noarch.rpm libtheora 1.1.1-5.8 sha1sum:9809978e4e7c0a620dd735218bb1bd317fe32149"
ALL+="ARC_VV_LIBTHEORA "

ARC_VV_LIBVORBIS="${DOWNLOAD_HOST}mingw32-libvorbis-1.3.3-1.8.noarch.rpm libvorbis 1.3.3-1.8 sha1sum:c9efd698ed62c26cf62442dafc2d9d2dcbcd651c"
ALL+="ARC_VV_LIBVORBIS "

ARC_WEBKITGTK="${DOWNLOAD_HOST}mingw32-libwebkitgtk-1.10.2-9.2.noarch.rpm WebKitGTK+ 1.10.2-9.2 sha1sum:010dbad413f824696cd1e32fe70046c9a1cb425f"
ALL+="ARC_WEBKITGTK "

ARC_ZLIB="${DOWNLOAD_HOST}mingw32-zlib-1.2.8-2.6.noarch.rpm zlib 1.2.8-2.6 sha1sum:bb75b2a341309eb75daacb93d43d6c072c71923c"
ALL+="ARC_ZLIB "

mkdir -p $STAGE_DIR
cd $STAGE_DIR

rm -rf $INSTALL_DIR
mkdir $INSTALL_DIR

#new CONTENTS file
echo Bundle Version $BUNDLE_VERSION > $CONTENTS_FILE

if [ $is_win32 = "yes" ]; then
CPIO_URL="https://pidgin.im/~twasilczyk/win32/devel-deps/cpio/bsdcpio-3.0.3-1.4.tar.gz"
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
BSDCPIO=bsdcpio/bsdcpio.exe
else
BSDCPIO=`which bsdcpio`
fi

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
		$BSDCPIO --quiet -f etc/fonts/conf.d -di < $FILE || exit 1
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
rm "$CERT_PATH"

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
