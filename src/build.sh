#!/bin/bash

cd ~/project/sp-plus/src || { echo "Failed to change directory"; exit 1; }

# Clean up the build
clean() {
	rm -f ../bin/sp-plus && echo "Cleaned"
}

# pass c to clean
if [ "$1" == "c" ]; then
	clean 
	exit 0
fi

TARGET="../bin/sp-plus"
SRC="platform/linux_platform.c sp_plus.c sp_raster.c"

# pass 'r' for release mode
if [ "$1" == "r" ]; then
	CFLAGS="-O3"
else
	CFLAGS="-g -Wextra -D DEBUG"
fi

CFLAGS="$CFLAGS -I./external"

LINKFLAGS="-lm -lasound -lX11 -L../lib -lsmarc"

# Create the target directory if it doesn't exist
mkdir -p ../bin

# Compile and link
gcc $CFLAGS -o $TARGET $SRC $LINKFLAGS && echo "Compiled" 
