#!/usr/bin/python
#
#       Copyright (C) 2014 Stephen M. Cameron
#       Author: Stephen M. Cameron
#
#       This file is part of planetificator.
#
#       planetificator is free software; you can redistribute it and/or modify
#       it under the terms of the GNU General Public License as published by
#       the Free Software Foundation; either version 2 of the License, or
#       (at your option) any later version.
#
#       planetificator is distributed in the hope that it will be useful,
#       but WITHOUT ANY WARRANTY; without even the implied warranty of
#       MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#       GNU General Public License for more details.
#
#       You should have received a copy of the GNU General Public License
#       along with planetificator; if not, write to the Free Software
#       Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#

import sys
import Image
import math

imagename = "image.jpg";
if (len(sys.argv) >= 2):
  imagename = sys.argv[1];

im = Image.open(imagename)
im.load();

image_width = 0.0 + im.size[0];
image_height = 0.0 + im.size[1];

outputimg = Image.new("RGB", im.size);

def sampleimg(row, col):
   irow = row;
   angle = (3.1415927 / 2.0) * (abs(row - image_height / 2.0) / (image_height / 2.0));
   circum = math.cos(angle) * image_width;
   icol = ((image_width - circum) / 2.0) + (col / image_width) * circum;
   return im.getpixel((icol, irow));

for row in range(0, int(image_height)):
   for col in range(0, int(image_width)):
      outputimg.putpixel((col, row), sampleimg(row, col));

outputimg.save("output.jpg");

