TEMPLATE = app
CONFIG += link_pkgconfig
PKGCONFIG += gio-unix-2.0 gio-2.0 glib-2.0 libsoup-2.4 libwspcodec ImageMagick
DBUS_INTERFACE_DIR = $$_PRO_FILE_PWD_
MMS_LIB_DIR = $$_PRO_FILE_PWD_/../mms-lib
MMS_OFONO_DIR = $$_PRO_FILE_PWD_/../mms-ofono
MMS_HANDLER_DIR = $$_PRO_FILE_PWD_/../mms-handler-dbus
INCLUDEPATH += $$MMS_OFONO_DIR/include
INCLUDEPATH += $$MMS_LIB_DIR/include
INCLUDEPATH += $$MMS_HANDLER_DIR/include
QMAKE_CFLAGS += -Wno-unused

include(../mms-lib/mms-lib-config.pri)

ResizeImageMagick {
  CONFIG -= qt
  PKGCONFIG += ImageMagick
}

SOURCES += \
  main.c \
  mms_engine.c
HEADERS += \
  mms_engine.h
OTHER_FILES += \
  org.nemomobile.MmsEngine.push.conf \
  org.nemomobile.MmsEngine.dbus.conf \
  org.nemomobile.MmsEngine.service \
  org.nemomobile.MmsEngine.xml

CONFIG(debug, debug|release) {
    DEFINES += DEBUG
    DESTDIR = $$_PRO_FILE_PWD_/build/debug
    LIBS += $$MMS_OFONO_DIR/build/debug/libmms-ofono.a
    LIBS += $$MMS_HANDLER_DIR/build/debug/libmms-handler-dbus.a
    LIBS += $$MMS_LIB_DIR/build/debug/libmms-lib.a
} else {
    DESTDIR = $$_PRO_FILE_PWD_/build/release
    LIBS += $$MMS_OFONO_DIR/build/release/libmms-ofono.a
    LIBS += $$MMS_HANDLER_DIR/build/release/libmms-handler-dbus.a
    LIBS += $$MMS_LIB_DIR/build/release/libmms-lib.a
}

LIBS += -lmagic -ljpeg

MMS_ENGINE_DBUS_XML = $$DBUS_INTERFACE_DIR/org.nemomobile.MmsEngine.xml
MMS_ENGINE_DBUS_H = org.nemomobile.MmsEngine.h
org_nemomobile_mmsengine_h.input = MMS_ENGINE_DBUS_XML
org_nemomobile_mmsengine_h.output = $$MMS_ENGINE_DBUS_H
org_nemomobile_mmsengine_h.commands = gdbus-codegen --generate-c-code \
  org.nemomobile.MmsEngine $$MMS_ENGINE_DBUS_XML
org_nemomobile_mmsengine_h.CONFIG = no_link
QMAKE_EXTRA_COMPILERS += org_nemomobile_mmsengine_h

MMS_ENGINE_DBUS_C = org.nemomobile.MmsEngine.c
org_nemomobile_mmsengine_c.input = MMS_ENGINE_DBUS_XML
org_nemomobile_mmsengine_c.output = $$MMS_ENGINE_DBUS_C
org_nemomobile_mmsengine_c.commands = gdbus-codegen --generate-c-code \
  org.nemomobile.MmsEngine $$MMS_ENGINE_DBUS_XML
org_nemomobile_mmsengine_c.CONFIG = no_link
QMAKE_EXTRA_COMPILERS += org_nemomobile_mmsengine_c
GENERATED_SOURCES += $$MMS_ENGINE_DBUS_C
