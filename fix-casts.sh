#!/bin/sh

if [ $# -eq 0 ]; then
	echo "Usage: `basename "$0"` PurpleFoo..."
	echo
	echo "This script searches the *current working directory* and replaces casts"
	echo "with GObject-style type checking and casting macros."
	echo 'For example, "(PurpleBuddy *)b" becomes "PURPLE_BUDDY(b)".'
	exit 0
fi

for struct in $* ; do
	cast=`echo $struct | sed "s|[A-Z]|_\0|g" | tr "a-z" "A-Z" | sed "s|^_||"`
	for file in `grep -rl "([[:space:]]*$struct[[:space:]]*\*[[:space:]]*)" . --include=*.c --exclude=purple-client-bindings.c` ; do
		sed -i "s|([[:space:]]*$struct[[:space:]]*\*[[:space:]]*)[[:space:]]*(|$cast(|g" $file
		sed -i "s|([[:space:]]*$struct[[:space:]]*\*[[:space:]]*)[[:space:]]*\([^(][^,);]*\)|$cast(\1)|g" $file
	done
done
