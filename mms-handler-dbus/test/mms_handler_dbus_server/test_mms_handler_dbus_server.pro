TARGET = test_mms_handler_dbus_server
CONFIG += console
CONFIG -= app_bundle

QT += core
QT -= gui
QT += dbus

QMAKE_CXXFLAGS *= -Werror -Wall -fno-exceptions -Wno-psabi -Wno-unused-parameter

TEMPLATE = app

SOURCES += \
  main.cpp \
  mmsadaptor.cpp \
  mmshandler.cpp \
  mmspart.cpp

HEADERS += \
  mmsadaptor.h \
  mmshandler.h \
  mmspart.h

OTHER_FILES += \
  org.nemomobile.MmsHandler.conf \
  ../../spec/org.nemomobile.MmsHandler.xml
