#!/usr/bin/env bash
# if $(gcc -D _DEBUG -Wall -O0 -fsanitize=undefined -fsanitize=leak -fsanitize=address -g main.c -o term -lm -lstdc++ /home/ym/fun/tracy/library/unix/obj/release/o/public/TracyClient.o -lGLU -lGL -lX11 -lfreetype -Ibuild/include/ ./build/src/glad.c -I/usr/include/freetype2); then
if $(gcc -Wall -lm -O0 -g main.c -march=native -o term -lstdc++ /home/ym/fun/tracy/library/unix/obj/release/o/public/TracyClient.o -lGLU -lGL -lX11 -lfreetype -Ibuild/include/ ./build/src/glad.c -I/usr/include/freetype2); then
# if $(gcc -O2  main.c -o term -lstdc++ /home/ym/fun/tracy/library/unix/obj/release/o/public/TracyClient.o -lGL -lX11 -lfreetype -Ibuild/include/ ./build/src/glad.c -I/usr/include/freetype2); then
	if [[ $1 == "-nl" ]]; then
		exit
	fi
	if [[ -z ${@} ]]; then
		./term base64 /dev/urandom
	else
		./term "$@"
	fi
fi
