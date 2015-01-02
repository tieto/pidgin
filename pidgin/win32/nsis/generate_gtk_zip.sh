#!/bin/bash
# Script to generate zip file for GTK+ runtime to be included in Pidgin installer

PIDGIN_BASE=$1
GPG_SIGN=$2

if [ ! -e $PIDGIN_BASE/ChangeLog ]; then
	echo $(basename $0) must must have the pidgin base dir specified as a parameter.
	exit 1
fi

STAGE_DIR=`readlink -f $PIDGIN_BASE/pidgin/win32/nsis/gtk_runtime_stage`
#Subdirectory of $STAGE_DIR
INSTALL_DIR=Gtk
SRC_INSTALL_DIR=src_install
CONTENTS_FILE=$INSTALL_DIR/CONTENTS
PIDGIN_VERSION=$( < $PIDGIN_BASE/VERSION )

#This needs to be changed every time there is any sort of change.
BUNDLE_VERSION=2.16.6.3
BUNDLE_SHA1SUM=e1b1ec8d2159fa98b2a9f516dbfe745bf7a22169
ZIP_FILE="$PIDGIN_BASE/pidgin/win32/nsis/gtk-runtime-$BUNDLE_VERSION.zip"
SRC_ZIP_FILE="$PIDGIN_BASE/pidgin/win32/nsis/gtk-runtime-$BUNDLE_VERSION-src.zip"

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


#Format for packages is "binary_url name version binary_validation src_url src_validation"
ATK="http://ftp.gnome.org/pub/gnome/binaries/win32/atk/1.32/atk_1.32.0-2_win32.zip ATK 1.32.0-2 sha1sum:3c31c9d6b19af840e2bd8ccbfef4072a6548dc4e http://ftp.gnome.org/pub/gnome/sources/atk/1.32/atk-1.32.0.tar.bz2|https://raw.githubusercontent.com/dieterv/legacynativebuilds/692072d1c571ef50f8bbe01cd005313d2302bef0/devel/src/dieterv/packaging/atk_1.32.0-2_win32.sh sha256sum:e9a3e598f75c4db1af914f8b052dd9f7e89e920a96cc187c18eb06b8339cb16e|sha256sum:94cf905cee30b461194fa4cdfebedb0013bca46cdc52228ea2f23ef595de158b"
#Cairo 1.10.2 has a bug that can be seen when selecting text
#CAIRO="http://ftp.gnome.org/pub/GNOME/binaries/win32/dependencies/cairo_1.10.2-2_win32.zip Cairo 1.10.2-2 sha1sum:d44cd66a9f4d7d29a8f2c28d1c1c5f9b0525ba44"
CAIRO="http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/cairo_1.8.10-1_win32.zip Cairo 1.8.10-1 sha1sum:a08476cccd807943958610977a138c4d6097c7b8 http://cairographics.org/releases/cairo-1.8.10.tar.gz|https://developer.pidgin.im/static/win32/cairo_1.8.10-1_win32.sh sha1sum:fd5e8ca82ff0e8542ea4c51612cad387f2a49df3|sha1sum:b2ac2ae06a5ea9f15802209707607fd40b4aa47d"
EXPAT="http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/expat_2.1.0-1_win32.zip Expat 2.1.0-1 gpg:0x71D4DDE53F188CBE http://downloads.sourceforge.net/project/expat/expat/2.1.0/expat-2.1.0.tar.gz|https://raw.githubusercontent.com/dieterv/legacynativebuilds/692072d1c571ef50f8bbe01cd005313d2302bef0/devel/src/dieterv/packaging/expat_2.1.0-1_win32.sh sha1sum:b08197d146930a5543a7b99e871cba3da614f6f0|sha1sum:5b1c345147bbbabeae0bc6649c19ea11fab3902c"
FONTCONFIG="http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/fontconfig_2.8.0-2_win32.zip Fontconfig 2.8.0-2 sha1sum:37a3117ea6cc50c8a88fba9b6018f35a04fa71ce http://www.fontconfig.org/release/fontconfig-2.8.0.tar.gz|https://raw.githubusercontent.com/dieterv/legacynativebuilds/692072d1c571ef50f8bbe01cd005313d2302bef0/devel/src/tml/packaging/fontconfig_2.8.0-2_win32.sh sha1sum:570fb55eb14f2c92a7b470b941e9d35dbfafa716|sha1sum:2e8a0e473344b68c440f1a56f33eb669ccd0bf87"
FREETYPE="http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/freetype_2.4.10-1_win32.zip Freetype 2.4.10-1 gpg:0x71D4DDE53F188CBE http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/freetype-2.4.2.tar.bz2|https://raw.githubusercontent.com/dieterv/legacynativebuilds/692072d1c571ef50f8bbe01cd005313d2302bef0/devel/src/dieterv/packaging/freetype_2.4.10-1_win32.sh sha1sum:cc257ceda2950b8c80950d780ccf3ce665a815d1|sha1sum:3c3c97099f0b1e7c25d55d6d3614b9cdb2da83e7"
GETTEXT="http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/gettext-runtime_0.18.1.1-2_win32.zip Gettext 0.18.1.1-2 sha1sum:a7cc1ce2b99b408d1bbea9a3b4520fcaf26783b3 http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/gettext-0.18.1.1.tar.gz|https://raw.githubusercontent.com/dieterv/legacynativebuilds/692072d1c571ef50f8bbe01cd005313d2302bef0/devel/src/tml/packaging/gettext_0.18.1.1-2_win32.sh sha1sum:5009deb02f67fc3c59c8ce6b82408d1d35d4e38f|sha1sum:8a7c1bb692c8d4f589c077c8b332c74040bca31c"
GLIB="http://ftp.gnome.org/pub/gnome/binaries/win32/glib/2.28/glib_2.28.8-1_win32.zip Glib 2.28.8-1 sha1sum:5d158f4c77ca0b5508e1042955be573dd940b574 http://ftp.gnome.org/pub/gnome/sources/glib/2.28/glib-2.28.8.tar.bz2|https://raw.githubusercontent.com/dieterv/legacynativebuilds/692072d1c571ef50f8bbe01cd005313d2302bef0/devel/src/dieterv/packaging/glib_2.28.8-1_win32.sh sha256sum:222f3055d6c413417b50901008c654865e5a311c73f0ae918b0a9978d1f9466f|sha256sum:907ebd40af90ea92fdafca44f4c3792ed6c041b7cd9e8d98ae534f51283ab164"
GTK="http://ftp.gnome.org/pub/gnome/binaries/win32/gtk+/2.16/gtk+_2.16.6-2_win32.zip GTK+ 2.16.6-2 sha1sum:012853e6de814ebda0cc4459f9eed8ae680e6d17 http://ftp.acc.umu.se/pub/gnome/sources/gtk+/2.16/gtk+-2.16.6.tar.bz2|https://developer.pidgin.im/static/win32/gtk+_2.16.6-2_win32.sh sha256sum:18e0f9792028e6cc5108447678f17d396f9a2cdfec1e6ab5dca98cb844f954af|sha256sum:47ac17cf3f638464ae8ed54a0a9532693373b5f81752cf92590e3d79c5c976ec"
LIBPNG="http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/libpng_1.4.12-1_win32.zip libpng 1.4.12-1 gpg:0x71D4DDE53F188CBE http://download.sourceforge.net/libpng/libpng-1.4.12.tar.gz|https://raw.githubusercontent.com/dieterv/legacynativebuilds/cff4e06f877242f8db485318eccd5f8aa01fb199/devel/src/dieterv/packaging/libpng_1.4.12-1_win32.sh sha1sum:d22b339f3261140fb9de83784d05ce5b86c077fb|sha1sum:e00bc64d84ca50127c0233c045ff0147175f705b"
PANGO="https://developer.pidgin.im/static/win32/pango_1.29.4-1daa_win32.zip Pango 1.29.4-1daa gpg:0x86723FEEDE890574 http://ftp.gnome.org/pub/gnome/sources/pango/1.29/pango-1.29.4.tar.bz2|https://raw.githubusercontent.com/dieterv/legacynativebuilds/692072d1c571ef50f8bbe01cd005313d2302bef0/devel/src/dieterv/packaging/pango_1.29.4-1_win32.sh|https://developer.pidgin.im/static/win32/pango_1.29.4-1daa_win32.zip.patch sha256sum:f15deecaecf1e9dcb7db0e4947d12b5bcff112586434f8d30a5afd750747ff2b|sha256sum:743bb703b36f367b5569e031a107fff51eef409650e635e1a48a23f9ac38ef71|sha256sum:4d241c3835217deab280a8c1f2154932a4fba118f0b02a22c10fa041359381cf"
ZLIB="https://developer.pidgin.im/static/win32/zlib-1.2.8_daa1_win32.zip zlib 1.2.8_daa1 gpg:0x86723FEEDE890574 http://zlib.net/zlib-1.2.8.tar.gz|https://developer.pidgin.im/static/win32/build_zlib-1.2.8_daa1.sh sha1sum:a4d316c404ff54ca545ea71a27af7dbc29817088|sha1sum:bcac4cfea2ebf274c5b72c2507105b50bbeed1d6"

ALL="ATK CAIRO EXPAT FONTCONFIG FREETYPE GETTEXT GLIB GTK LIBPNG PANGO ZLIB"

mkdir -p $STAGE_DIR
mkdir -p $STAGE_DIR/src
cd $STAGE_DIR

rm -rf $INSTALL_DIR
mkdir $INSTALL_DIR

rm -rf $SRC_INSTALL_DIR
mkdir $SRC_INSTALL_DIR

#new CONTENTS file
echo Bundle Version $BUNDLE_VERSION > $CONTENTS_FILE

function validate_file {
	VALIDATION=$1
	FILE=$2
	VALIDATION_TYPE=${VALIDATION%%:*}
	VALIDATION_VALUE=${VALIDATION##*:}
	VALIDATION_TYPE=${VALIDATION%%:*}
	VALIDATION_VALUE=${VALIDATION##*:}

	if [ $VALIDATION_TYPE == 'sha1sum' -o $VALIDATION_TYPE == 'sha256sum' ]; then
		CHECK_SUM=`$VALIDATION_TYPE "$FILE"`
		CHECK_SUM=${CHECK_SUM%%\ *}
		if [ "$CHECK_SUM" != "$VALIDATION_VALUE" ]; then
			echo "$VALIDATION_TYPE ($CHECK_SUM) for $FILE doesn't match expected value of $VALIDATION_VALUE"
			exit 1
		fi
	elif [ $VALIDATION_TYPE == 'gpg' ]; then
		if [ ! -e "$FILE.asc" ]; then
			echo Downloading GPG key for $NAME
			wget "$URL.asc" || exit 1
		fi
		#Use our own keyring to avoid adding stuff to the main keyring
		#This doesn't use $GPG_SIGN because we don't this validation to be bypassed when people are skipping signing output
		GPG_BASE="gpg -q --keyring $STAGE_DIR/$VALIDATION_VALUE-keyring.gpg"
		if [[ ! -e "$STAGE_DIR/$VALIDATION_VALUE-keyring.gpg" \
				|| `$GPG_BASE --list-keys "$VALIDATION_VALUE" > /dev/null && echo -n "0"` -ne 0 ]]; then
			touch "$STAGE_DIR/$VALIDATION_VALUE-keyring.gpg"
			$GPG_BASE --no-default-keyring --keyserver pgp.mit.edu --recv-key "$VALIDATION_VALUE" || exit 1
		fi
		$GPG_BASE --verify "$FILE.asc" || (echo "$FILE failed signature verification"; exit 1) || exit 1
	else
		echo "Unrecognized validation type of $VALIDATION_TYPE"
		exit 1
	fi
}

function download_and_validate {
	PREFIX=$1
	URLS=$2
	VALIDATIONS=$3
	EXTRACT=$4
	OLD_IFS=$IFS
	IFS='|'
	URL_SPLIT=($URLS)
	VALIDATION_SPLIT=($VALIDATIONS)
	IFS=$OLD_IFS

	if [ ${#URL_SPLIT[@]} -ne ${#VALIDATION_SPLIT[@]} ]; then
		echo "URL and validation counts don't match for $VAL"
		exit 1
	fi

	if [ "x$PREFIX" != "x" ]; then
		mkdir -p "$PREFIX"
	fi

	LEN=${#URL_SPLIT[@]}
	for (( i = 0; i < ${LEN}; i++ )); do
		URL=${URL_SPLIT[$i]}
		VALIDATION=${VALIDATION_SPLIT[$i]}
		FILE=${PREFIX}$(basename $URL)
		if [ ! -e "$FILE" ]; then
			echo Downloading $FILE for $NAME ...
			wget -P "$PREFIX" $URL || exit 1
		fi
		validate_file "$VALIDATION" "$FILE"
		EXTENSION=${FILE##*.}
		#This is an OpenSuSE build service RPM
		if [ $EXTENSION == 'rpm' ]; then
			echo "Generating zip from $FILE"
			FILE=$(../rpm2zip.sh $FILE)
		fi
		if [ $EXTRACT == "1" ]; then
			unzip -q "$FILE" -d "$INSTALL_DIR" || exit 1
		else
			mkdir -p "$SRC_INSTALL_DIR/$PREFIX"
			cp "$FILE" "$SRC_INSTALL_DIR/$FILE"
		fi
	done
}

function process_package {
	SPLIT=($1)
	URL=${SPLIT[0]}
	NAME="${SPLIT[1]} ${SPLIT[2]}"
	VALIDATION=${SPLIT[3]}
	download_and_validate "" "$URL" "$VALIDATION" "1"

	SRC_URL=${SPLIT[4]}
	SRC_VALIDATION=${SPLIT[5]}
	download_and_validate "src/$NAME/" "$SRC_URL" "$SRC_VALIDATION" "0"

	echo "$NAME" >> $CONTENTS_FILE
}

for VAL in $ALL
do
	VAR=${!VAL}
	SPLIT=($VAR)
	if [ ${#SPLIT[@]} -lt 6 ]; then
		echo "$VAL has only ${#SPLIT[@]} attributes"
		exit 1
	fi
	process_package "$VAR"
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
($GPG_SIGN -ab $ZIP_FILE && $GPG_SIGN --verify $ZIP_FILE.asc) || exit 1

#Generate src zip file
rm -f $SRC_ZIP_FILE
(cd $SRC_INSTALL_DIR/src && zip -9 -r $SRC_ZIP_FILE *)
($GPG_SIGN -ab $SRC_ZIP_FILE && $GPG_SIGN --verify $SRC_ZIP_FILE.asc) || exit 1

exit 0

