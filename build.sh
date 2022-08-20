#!/usr/bin/env bash
# for some reason sanitization crashes the whole prog?
# if $(gcc -D _DEBUG -O0 -fsanitize=undefined -fsanitize=leak -fsanitize=address -g main.c -o term -lstdc++ /home/ym/fun/tracy/library/unix/obj/release/o/public/TracyClient.o -lGLU -lGL -lX11 -lfreetype -Ibuild/include/ ./build/src/glad.c -I/usr/include/freetype2); then
if $(gcc -D TRACY_ENABLE -D TRACY_DEBUGINFOD -D _DEBUG -O0 -g main.c -o term -fno-omit-frame-pointer -rdynamic -lstdc++ -ldebuginfod /home/ym/fun/tracy/library/unix/obj/release/o/public/TracyClient.o -lGLU -lGL -lX11 -lfreetype -Ibuild/include/ ./build/src/glad.c -I/usr/include/freetype2); then
	if [[ -z ${@} ]]; then
		./term base64 /dev/urandom
	else
		./term "$@"
	fi
fi
