TEMPLATE = app
TARGET = mms-send
CONFIG -= qt

CONFIG += link_pkgconfig
PKGCONFIG +=  gio-unix-2.0 gio-2.0 glib-2.0
QMAKE_CFLAGS += -Wno-unused-parameter

SOURCES += mms-send.c

DBUS_SPEC_DIR = $$_PRO_FILE_PWD_/../mms-engine

# org.nemomobile.MmsEngine
MMS_ENGINE_XML = $$DBUS_SPEC_DIR/org.nemomobile.MmsEngine.xml
MMS_ENGINE_COMMAND = gdbus-codegen --generate-c-code \
  org.nemomobile.MmsEngine $$MMS_ENGINE_XML
MMS_ENGINE_DBUS_H = org.nemomobile.MmsEngine.h
org_nemomobile_mmsengine_h.input = MMS_ENGINE_XML
org_nemomobile_mmsengine_h.output = $$MMS_ENGINE_DBUS_H
org_nemomobile_mmsengine_h.commands = $$MMS_ENGINE_COMMAND
org_nemomobile_mmsengine_h.CONFIG = no_link
QMAKE_EXTRA_COMPILERS += org_nemomobile_mmsengine_h

MMS_ENGINE_DBUS_C = org.nemomobile.MmsEngine.c
org_nemomobile_mmsengine_c.input = MMS_ENGINE_XML
org_nemomobile_mmsengine_c.output = $$MMS_ENGINE_DBUS_C
org_nemomobile_mmsengine_c.commands = $$MMS_ENGINE_COMMAND
org_nemomobile_mmsengine_c.CONFIG = no_link
QMAKE_EXTRA_COMPILERS += org_nemomobile_mmsengine_c
GENERATED_SOURCES += $$MMS_ENGINE_DBUS_C
