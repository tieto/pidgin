#!/bin/bash
# Script to silence win32 build by displaying a neat one-line notice instead of
# full command contents when executing compilers.
#
# Written by Tomek Wasilczyk <tomkiewicz@cpw.pidgin.im>, licensed under GNU GPL

tag=$1
found=0
object=""
for arg in "$@"
do
	if [ "$found" == 1 ]; then
		object="$arg"
		break
	fi
	if [ "$arg" == "-o" ]; then
		found=1
	fi
	if [ "$tag" == "auto" ] && [ "$arg" == "-shared" ]; then
		tag="CCLD"
	fi
	if [ "$tag" == "PERL" ] && [ "$arg" == "-e" ]; then
		found=1
	fi
done

if [ "$tag" == "auto" ]; then
	tag="CC"
fi

if [ "$tag" == "PERL" ]; then
	object=`echo "$object" | sed 's|.*output *=> *"\([^"]*\)".*|\1|'`
fi

echo -e "  $tag\t$object"
shift 1
"$@"
