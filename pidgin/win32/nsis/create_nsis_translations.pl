#!/usr/bin/perl
#
# create_nsis_translations.pl
#
# Copyright (C) 2000-2009 Bruno Coudoin
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <http://www.gnu.org/licenses/>.
#

use strict;

sub usage {
    print 'create_nsis_translations.pl translations installer tmp_dir
  translations
     This is an input file that contains all the
     translated strings. If must be formated as a GNU/Linux
     desktop file and contains multiple strings entry.
     For example you must have:
     toBe=To be or not to be
     toBe[fr]=Etre ou ne pas etre

  installer
     This is your nsis installer source file. You must include
     in it the marker @INSERT_TRANSLATIONS@ before you use any
     translation string.
     After that you can use the variable $(toBe) in your file.

  tmp_dir
     This is a directory in which temporary files needed for
     the translation system.
     It will be created if non existant.
     You can remove it once you have created your installer.
';
}

my $translations;
if (! $ARGV[0] || ! -f $ARGV[0])
{
    usage();
}
else
{
    $translations = $ARGV[0];
}

shift;
my $installer;
if (! $ARGV[0] || ! -f $ARGV[0])
{
    usage();
}
else
{
    $installer = $ARGV[0];
}

shift;
my $tmp_dir;
if (! $ARGV[0] )
{
    usage();
}
else
{
    $tmp_dir = $ARGV[0];

    if ( ! -d $tmp_dir )
    {
	mkdir $tmp_dir or die "ERROR: '$tmp_dir' $!\n";
    }
}

print "Processing translation file '$translations'\n";
print "          NSIS source file  '$installer'\n";
print "                Working dir '$tmp_dir'\n";

# Commented out locales that are not available in nsis
# Map value is ["NSISFilename", "Encoding", "LCID"]
my %localeNames = (
  "af" =>	["Afrikaans", "WINDOWS-1252", "1078"],
#  "am" =>	["Amharic", "UTF-8"],
  "ar" =>	["Arabic", "WINDOWS-1256", "1025"],
  "be" =>	["Belarusian", "WINDOWS-1251", "1059"],
  "bg" =>	["Bulgarian", "WINDOWS-1251", "1026"],
  "bs" =>	["Bosnian", "WINDOWS-1250", "5146"],
  "br" =>	["Breton", "WINDOWS-1252", "1150"],
  "ca" =>	["Catalan", "WINDOWS-1252", "1027"],
  "cs" =>	["Czech", "WINDOWS-1250", "1029"],
  "cy" =>	["Welsh", "WINDOWS-1252", "1160"],
  "da" =>	["Danish", "WINDOWS-1252", "1030"],
  "de" =>	["German", "WINDOWS-1252", "1031"],
#  "dz" =>	["Dzongkha", "UTF-8"],
  "el" =>	["Greek", "WINDOWS-1253", "1032"],
  "en" =>	["English", "WINDOWS-1252", "1033"],
  "eo" =>	["Esperanto", "WINDOWS-1252", "9998"],
  "es" =>	["Spanish", "WINDOWS-1252", "1034"],
  "et" =>	["Estonian", "WINDOWS-1257", "1061"],
  "eu" =>	["Basque", "WINDOWS-1252", "1069"],
  "fa" =>	["Farsi", "WINDOWS-1256", "1065"],
  "fi" =>	["Finnish", "WINDOWS-1252", "1035"],
  "fr" =>	["French", "WINDOWS-1252", "1036"],
  "ga" =>	["Irish", "WINDOWS-1252", "2108"],
  "gl" =>	["Galician", "WINDOWS-1252", "1110"],
#  "gu" =>	["Gujarati", "UTF-8"],
  "he" =>	["Hebrew", "WINDOWS-1255", "1037"],
#  "hi" =>	["Hindi", "UTF-8"],
  "hr" =>	["Croatian", "WINDOWS-1250", "1050"],
  "hu" =>	["Hungarian", "WINDOWS-1250", "1038"],
  "id" =>	["Indonesian", "WINDOWS-1252", "1057"],
  "is" =>	["Icelandic", "WINDOWS-1252", "15"], #This should be 1039!
  "it" =>	["Italian", "WINDOWS-1252", "1040"],
  "ja" =>	["Japanese", "CP932", "1041"],
#  "ka" =>	["Georgian", "UTF-8"],
  "ko" =>	["Korean", "CP949", "1042"],
  "ku" =>	["Kurdish", "WINDOWS-1254", "9999"],
  "lb" =>	["Luxembourgish", "WINDOWS-1252", "4103"],
  "lt" =>	["Lithuanian", "WINDOWS-1257", "1063"],
  "lv" =>	["Latvian", "WINDOWS-1257", "1062"],
  "mk" =>	["Macedonian", "WINDOWS-1251", "1071"],
#  "ml" =>	["Malayalam", "UTF-8"],
#  "mr" =>	["Marathi", "UTF-8"],
  "mn" =>	["Mongolian", "WINDOWS-1251", "1104"],
  "ms" =>	["Malay", "WINDOWS-1252", "1086"],
  "nb" =>	["Norwegian", "WINDOWS-1252", "1044"],
#  "ne" =>	["Nepal", "UTF-8"],
  "nl" =>	["Dutch", "WINDOWS-1252", "1043"],
  "nn" =>	["NorwegianNynorsk", "WINDOWS-1252", "2068"],
#  "oc" =>	["Occitan", "WINDOWS-1252"],
#  "pa" =>	["Punjabi", "UTF-8"],
  "pl" =>	["Polish", "WINDOWS-1250", "1045"],
  "pt" =>	["Portuguese", "WINDOWS-1252", "2070"],
  "pt_BR" =>	["PortugueseBR", "WINDOWS-1252", "1046"],
  "ro" =>	["Romanian", "WINDOWS-1250", "1048"],
  "ru" =>	["Russian", "WINDOWS-1251", "1049"],
#  "rw" =>	["Kinyarwanda", "UTF-8"],
  "sk" =>	["Slovak", "WINDOWS-1250", "1051"],
  "sl" =>	["Slovenian", "WINDOWS-1250", "1060"],
#  "so" =>	["Somali", "UTF-8"],
  "sq" =>	["Albanian", "WINDOWS-1252", "1052"],
  "sr" =>	["Serbian", "WINDOWS-1251", "3098"],
  "sr\@latin" =>	["SerbianLatin", "WINDOWS-1250", "2074"],
  "sv" =>	["Swedish", "WINDOWS-1252", "1053"],
#  "ta" =>	["Tamil", "UTF-8"],
  "th" =>	["Thai", "WINDOWS-874", "1054"],
  "tr" =>	["Turkish", "WINDOWS-1254", "1055"],
  "uk" =>	["Ukrainian", "WINDOWS-1251", "1058"],
  "uz" =>	["Uzbek", "WINDOWS-1252", "1091"],
#  "ur" =>	["Urdu", "UTF-8"],
#  "vi" =>	["Vietnamese", "WINDOWS-1258"],
#  "wa" =>	["Walloon", "WINDOWS-1252"],
  "zh_CN" =>	["SimpChinese", "WINDOWS-936", "2052"],
  "zh_TW" =>	["TradChinese", "CP950", "1028"],
);

my @localeKeys = keys(%localeNames);

# Create the holder for the results
# %result{"locale"}{"stringname"} = result line
print "Parsing nsis_translations.desktop\n";
my %result;

# Create a hash of the keys to translate
open (MYFILE, $translations);
while (<MYFILE>) {
    chomp $_;
    if ($_ =~ /Encoding=UTF-8/)
    {
	next;
    }
    elsif ($_ =~ /^(\w+)=(.*)/)
    {
	my $line = "!define $1 \"$2\"\n";
	$result{"en"}{"$1"} = $line;
    }
    elsif ($_ =~ /^(\w+)\[(\w+)\]=(.*)/)
    {
	my $line = "!define $1 \"$3\"\n";
	$result{"$2"}{"$1"} = $line;
    }
}
close (MYFILE);

# Lets insert the default languages
# in the installer file which means replacing:
#   @INSERTMACRO_MUI_LANGUAGE@
# By the list of locales:
#   !insertmacro MUI_LANGUAGE "French"

my $muiLanguages;
$muiLanguages = '
  ;; English goes first because its the default. The rest are
  ;; in alphabetical order (at least the strings actually displayed
  ;; will be).
  !insertmacro MUI_LANGUAGE "English"
';

# The specific GCompris translation for the installer
# replacing:
#   @GCOMPRIS_MACRO_INCLUDE_LANGFILE@
# By the list of locales:
#   !insertmacro GCOMPRIS_MACRO_INCLUDE_LANGFILE "ALBANIAN" "${GCOMPRIS_NSIS_INCLUDE_PATH}\translations\albanian.nsh"

my $gcomprisLanguages;

$gcomprisLanguages .= '
;--------------------------------
;Translations
  !define GCOMPRIS_DEFAULT_LANGFILE "${GCOMPRIS_NSIS_INCLUDE_PATH}\\translations\\en.nsh"
;;
;; Windows GCompris NSIS installer language macros
;;

!macro GCOMPRIS_MACRO_DEFAULT_STRING LABEL VALUE
  !ifndef "${LABEL}"
    !define "${LABEL}" "${VALUE}"
    !ifdef INSERT_DEFAULT
      !warning "${LANG} lang file missing ${LABEL}, using default..."
    !endif
  !endif
!macroend

!macro GCOMPRIS_MACRO_LANGSTRING_INSERT LABEL LANG
  LangString "${LABEL}" "${LANG_${LANG}}" "${${LABEL}}"
  !undef "${LABEL}"
!macroend

!macro GCOMPRIS_MACRO_LANGUAGEFILE_BEGIN LANG
  !define CUR_LANG "${LANG}"
!macroend
';


# GCOMPRIS_MACRO_LANGUAGEFILE_END
$gcomprisLanguages .= '
!macro GCOMPRIS_MACRO_LANGUAGEFILE_END
  !define INSERT_DEFAULT
  !include "${GCOMPRIS_DEFAULT_LANGFILE}"
  !undef INSERT_DEFAULT

  ; String labels should match those from the default language file.
';

my $text_en = $result{"en"};
foreach my $keyEn (keys(%$text_en)) {
    $gcomprisLanguages .= "  !insertmacro GCOMPRIS_MACRO_LANGSTRING_INSERT $keyEn \${CUR_LANG}";
    $gcomprisLanguages .= "\n";
}

$gcomprisLanguages .= '
  !undef CUR_LANG
!macroend
';

$gcomprisLanguages .= '
!macro GCOMPRIS_MACRO_INCLUDE_LANGFILE LANG FILE
  !insertmacro GCOMPRIS_MACRO_LANGUAGEFILE_BEGIN "${LANG}"
  !include "${FILE}"
  !insertmacro GCOMPRIS_MACRO_LANGUAGEFILE_END
!macroend
';

#
# Create each nsh translation file
#

print "Creating the nsh default file\n";
open (DESC, ">$tmp_dir/en.nsh");
print DESC ";; Auto generated file by create_nsis_translations.pl\n";
foreach my $keyEn (keys(%$text_en)) {
    my $line = $result{'en'}{$keyEn};
    $line =~ s/!define /!insertmacro GCOMPRIS_MACRO_DEFAULT_STRING /;
    print DESC $line;
}
close DESC;

$gcomprisLanguages .= "  !insertmacro GCOMPRIS_MACRO_INCLUDE_LANGFILE".
    " \"ENGLISH\"".
   " \"\${GCOMPRIS_NSIS_INCLUDE_PATH}\\translations\\en.nsh\"\n";

#
# Two pass are needed:
# - create the utf8 file
# - transform it to the proper windows locale
#
print "Creating the nsh locale files\n";
foreach my $lang (@localeKeys) {
    if ( $lang eq "en" ) { next; }
    open (DESC, ">$tmp_dir/$lang.nsh.utf8");
    print DESC ";; Auto generated file by create_nsis_translations.pl\n";
    print DESC ";; Code Page: $localeNames{$lang}[1]\n";

    my $text_locale = $result{"$lang"};
    my $total_key_count = 0;
    my $found_key_count = 0;
    foreach my $keyEn (keys(%$text_en)) {
	my $found = 0;
	$total_key_count++;
	foreach my $keyLocale (keys(%$text_locale)) {
	    # Fine, we found a translation
	    if ( $keyLocale eq $keyEn )
	    {
		print DESC "$result{$lang}{$keyLocale}";
		$found = 1;
		$found_key_count++;
		last;
	    }
	}
	# English keys are the reference.
	# If not found they are inserted
	#if ( ! $found )
	#{
	#    print DESC "$result{'en'}{$keyEn}";
	#}
    }
    close DESC;

    # If we have at least 50% of the keys found, include the language
    if (($found_key_count * 1.0 / $total_key_count) >= 0.5) {
        $muiLanguages .= "  !insertmacro MUI_LANGUAGE \"$localeNames{$lang}[0]\"\n";
        $gcomprisLanguages .= "  !insertmacro GCOMPRIS_MACRO_INCLUDE_LANGFILE".
            " \"". uc($localeNames{$lang}[0]) . "\"".
            " \"\${GCOMPRIS_NSIS_INCLUDE_PATH}\\translations\\$lang.nsh\"\n";
    } else {
        print "Ignoring language $lang because it is less than 50% translated ($found_key_count of $total_key_count).\n";
        continue;
    }


    # iconv conversion
    system("iconv -f UTF-8 -t $localeNames{$lang}[1] $tmp_dir/$lang.nsh.utf8 > $tmp_dir/$lang.nsh");
    if ($? ne 0)
    {
	die("ERROR: Failed to run: iconv -f UTF-8 -t $localeNames{$lang}[1] $lang.nsh.utf8 > $lang.nsh\n");
    }
    #`rm $tmp_dir/$lang.nsh.utf8`;

}

# We have all the data, let's replace it
my $gcomprisInstaller;
open (MYFILE, $installer);
while (<MYFILE>) {
    if ($_ =~ /\@INSERT_TRANSLATIONS\@/)
    {
	print "Processing \@INSERT_TRANSLATIONS\@\n";
	$gcomprisInstaller .= $muiLanguages;
	$gcomprisInstaller .= $gcomprisLanguages;
    }
    else
    {
	$gcomprisInstaller .= "$_";
    }
}
close (MYFILE);

# Rewrite the file with the replaced data
open (MYFILE, ">$installer");
print MYFILE "$gcomprisInstaller";
close (MYFILE);
