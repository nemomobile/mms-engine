# -*- Mode: makefile-gmake -*-

#MMS_RESIZE = ImageMagick
MMS_RESIZE = Qt

#
# ImageMagick support
#

ifeq ($(MMS_RESIZE),ImageMagick)
RESIZE_DEFINES = -DMMS_RESIZE_IMAGEMAGICK
RESIZE_PKG = ImageMagick
else
  ifeq ($(MMS_RESIZE),Qt)
    RESIZE_LIBS = -lstdc++
    RESIZE_DEFINES = -DMMS_RESIZE_QT
    ifeq ($(shell qmake --version | grep "Using Qt version 5"),)
        RESIZE_PKG = QtGui
    else
        RESIZE_PKG = Qt5Gui
    endif
  endif
endif

