#!/usr/bin/env bash
# gcc -g main.c -o term -lGLU -lGL -lX11 -lfreetype -I/usr/include/freetype2 && vblank_mode=0 ./term
if $(gcc -g main.c -o term -lGLU -lGL -lX11 -lfreetype -I/usr/include/freetype2); then
	[[ -z ${@} ]] && ./term base64 /dev/urandom || ./term $@
fi
