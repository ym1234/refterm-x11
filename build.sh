#!/usr/bin/env bash
gcc -g *.c -o term -lGLU -lGL -lX11 -lfreetype -I/usr/include/freetype2 && ./term
