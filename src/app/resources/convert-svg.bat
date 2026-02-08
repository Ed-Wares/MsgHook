@echo off
echo dependencies can be installed in MinGW with: pacman -S --needed mingw-w64-ucrt-x86_64-imagemagick
echo running: magick mogrify -format ico -density 600 -background none -define icon:auto-resize=16,32,48,64 *.svg
magick mogrify -format ico -density 600 -background none -define icon:auto-resize=16,32,48,64 *.svg
dir /b *.ico