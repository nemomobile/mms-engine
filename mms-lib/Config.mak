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
    ifeq ($(shell pkg-config --exists Qt5Gui; echo $$?),0)
        RESIZE_PKG = Qt5Gui
    else
        RESIZE_PKG = QtGui
    endif
    RESIZE_LIBS = -lstdc++
    RESIZE_DEFINES = -DMMS_RESIZE_QT
    RESIZE_CPPFLAGS = $(shell pkg-config --cflags $(RESIZE_PKG))
  endif
endif

