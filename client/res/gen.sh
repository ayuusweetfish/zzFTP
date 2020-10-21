#!/bin/sh

rm res/*.h

for i in res/*.png; do
  convert $i \
    -background black \
    -alpha Remove $i \
    -compose Copy_Opacity \
    -composite rgba:- \
  | xxd -i > $i.h
done
