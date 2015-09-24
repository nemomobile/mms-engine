TEMPLATE = lib
CONFIG += staticlib
CONFIG -= qt
CONFIG += link_pkgconfig
PKGCONFIG += glib-2.0 dconf
INCLUDEPATH += include
INCLUDEPATH += ../mms-lib/include
QMAKE_CFLAGS += -Wno-unused-parameter

CONFIG(debug, debug|release) {
  DEFINES += DEBUG
  DESTDIR = $$_PRO_FILE_PWD_/build/debug
} else {
  DESTDIR = $$_PRO_FILE_PWD_/build/release
}

SOURCES += src/mms_settings_dconf.c
HEADERS += include/mms_settings_dconf.h
OTHER_FILES += spec/org.nemomobile.mms.sim.gschema.xml
