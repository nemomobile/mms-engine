TEMPLATE = lib
CONFIG += staticlib
CONFIG -= qt
CONFIG += link_pkgconfig
PKGCONFIG += glib-2.0 gio-2.0 gio-unix-2.0
DBUS_SPEC_DIR = $$_PRO_FILE_PWD_/spec
INCLUDEPATH += include
INCLUDEPATH += ../mms-lib/include
QMAKE_CFLAGS += -Wno-unused

CONFIG(debug, debug|release) {
  DEFINES += DEBUG
  DESTDIR = $$_PRO_FILE_PWD_/build/debug
} else {
  DESTDIR = $$_PRO_FILE_PWD_/build/release
}

SOURCES += src/mms_handler_dbus.c
HEADERS += include/mms_handler_dbus.h
OTHER_FILES += spec/org.nemomobile.MmsHandler.xml

# org.nemomobile.MmsHandler
COMMHISTORYIF_XML = $$DBUS_SPEC_DIR/org.nemomobile.MmsHandler.xml
COMMHISTORYIF_GENERATE = gdbus-codegen --generate-c-code \
  org.nemomobile.MmsHandler $$COMMHISTORYIF_XML
COMMHISTORYIF_H = org.nemomobile.MmsHandler.h
org_nemomobile_MmsHandler_h.input = COMMHISTORYIF_XML
org_nemomobile_MmsHandler_h.output = $$COMMHISTORYIF_H
org_nemomobile_MmsHandler_h.commands = $$COMMHISTORYIF_GENERATE
org_nemomobile_MmsHandler_h.CONFIG = no_link
QMAKE_EXTRA_COMPILERS += org_nemomobile_MmsHandler_h

COMMHISTORYIF_C = org.nemomobile.MmsHandler.c
org_nemomobile_MmsHandler_c.input = COMMHISTORYIF_XML
org_nemomobile_MmsHandler_c.output = $$COMMHISTORYIF_C
org_nemomobile_MmsHandler_c.commands = $$COMMHISTORYIF_GENERATE
org_nemomobile_MmsHandler_c.CONFIG = no_link
QMAKE_EXTRA_COMPILERS += org_nemomobile_MmsHandler_c
GENERATED_SOURCES += $$COMMHISTORYIF_C
