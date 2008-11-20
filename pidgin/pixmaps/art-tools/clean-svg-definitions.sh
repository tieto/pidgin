#!/bin/bash

for f in `ls *.svg`
do
  echo "Processing $f file..."
  inkscape --vacuum-defs $f
done
