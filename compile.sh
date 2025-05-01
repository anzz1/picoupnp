#!/bin/sh
#

echo "Compiling picoupnp ..."
gcc -O2 -w -m32 -march=i686 -s \
	-fno-strict-aliasing -fno-strict-overflow -fvisibility=hidden \
	-fomit-frame-pointer -fno-stack-protector -fno-pie -no-pie \
    -Wl,--no-export-dynamic \
	main.c -o picoupnp
