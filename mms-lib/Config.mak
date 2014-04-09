# -*- Mode: makefile-gmake -*-

#MMS_RESIZE = ImageMagick
MMS_RESIZE = Qt

#
# ImageMagick support
#

ifeq ($(MMS_RESIZE),ImageMagick)
RESIZE_PKG = ImageMagick
RESIZE_DEFINES = -DMMS_RESIZE_IMAGEMAGICK
RESIZE_CFLAGS = $(shell pkg-config --cflags $(RESIZE_PKG))
else
  ifeq ($(MMS_RESIZE),Qt)
    RESIZE_PKG = Qt5Gui
    RESIZE_LIBS = -lstdc++
    RESIZE_DEFINES = -DMMS_RESIZE_QT
    RESIZE_CPPFLAGS = $(shell pkg-config --cflags $(RESIZE_PKG))
  endif
endif

