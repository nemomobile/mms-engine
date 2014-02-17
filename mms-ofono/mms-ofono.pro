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

SOURCES += \
  src/mms_ofono_connection.c \
  src/mms_ofono_connman.c \
  src/mms_ofono_context.c \
  src/mms_ofono_manager.c \
  src/mms_ofono_modem.c

HEADERS += \
  src/mms_ofono_connection.h \
  src/mms_ofono_context.h \
  src/mms_ofono_manager.h \
  src/mms_ofono_modem.h \
  src/mms_ofono_names.h \
  src/mms_ofono_types.h

HEADERS += \
  include/mms_ofono_connman.h \
  include/mms_ofono_log.h

# org.ofono.Manager
MANAGER_XML = $$DBUS_SPEC_DIR/org.ofono.Manager.xml
MANAGER_GENERATE = gdbus-codegen --generate-c-code \
  org.ofono.Manager $$MANAGER_XML
MANAGER_H = org.ofono.Manager.h
org_ofono_Manager_h.input = MANAGER_XML
org_ofono_Manager_h.output = $$MANAGER_H
org_ofono_Manager_h.commands = $$MANAGER_GENERATE
org_ofono_Manager_h.CONFIG = no_link
QMAKE_EXTRA_COMPILERS += org_ofono_Manager_h

MANAGER_C = org.ofono.Manager.c
org_ofono_Manager_c.input = MANAGER_XML
org_ofono_Manager_c.output = $$MANAGER_C
org_ofono_Manager_c.commands = $$MANAGER_GENERATE
org_ofono_Manager_c.CONFIG = no_link
QMAKE_EXTRA_COMPILERS += org_ofono_Manager_c
GENERATED_SOURCES += $$MANAGER_C

# org.ofono.ConnectionManager
CONNECTION_MANAGER_XML = $$DBUS_SPEC_DIR/org.ofono.ConnectionManager.xml
CONNECTION_MANAGER_GENERATE = gdbus-codegen --generate-c-code \
  org.ofono.ConnectionManager $$CONNECTION_MANAGER_XML
CONNECTION_MANAGER_H = org.ofono.ConnectionManager.h
org_ofono_ConnectionManager_h.input = CONNECTION_MANAGER_XML
org_ofono_ConnectionManager_h.output = $$CONNECTION_MANAGER_H
org_ofono_ConnectionManager_h.commands = $$CONNECTION_MANAGER_GENERATE
org_ofono_ConnectionManager_h.CONFIG = no_link
QMAKE_EXTRA_COMPILERS += org_ofono_ConnectionManager_h

CONNECTION_MANAGER_C = org.ofono.ConnectionManager.c
org_ofono_ConnectionManager_c.input = CONNECTION_MANAGER_XML
org_ofono_ConnectionManager_c.output = $$CONNECTION_MANAGER_C
org_ofono_ConnectionManager_c.commands = $$CONNECTION_MANAGER_GENERATE
org_ofono_ConnectionManager_c.CONFIG = no_link
QMAKE_EXTRA_COMPILERS += org_ofono_ConnectionManager_c
GENERATED_SOURCES += $$CONNECTION_MANAGER_C

# org.ofono.ConnectionContext
CONNECTION_CONTEXT_XML = $$DBUS_SPEC_DIR/org.ofono.ConnectionContext.xml
CONNECTION_CONTEXT_GENERATE = gdbus-codegen --generate-c-code \
  org.ofono.ConnectionContext $$CONNECTION_CONTEXT_XML
CONNECTION_CONTEXT_H = org.ofono.ConnectionContext.h
org_ofono_ConnectionContext_h.input = CONNECTION_CONTEXT_XML
org_ofono_ConnectionContext_h.output = $$CONNECTION_CONTEXT_H
org_ofono_ConnectionContext_h.commands = $$CONNECTION_CONTEXT_GENERATE
org_ofono_ConnectionContext_h.CONFIG = no_link
QMAKE_EXTRA_COMPILERS += org_ofono_ConnectionContext_h

CONNECTION_CONTEXT_C = org.ofono.ConnectionContext.c
org_ofono_ConnectionContext_c.input = CONNECTION_CONTEXT_XML
org_ofono_ConnectionContext_c.output = $$CONNECTION_CONTEXT_C
org_ofono_ConnectionContext_c.commands = $$CONNECTION_CONTEXT_GENERATE
org_ofono_ConnectionContext_c.CONFIG = no_link
QMAKE_EXTRA_COMPILERS += org_ofono_ConnectionContext_c
GENERATED_SOURCES += $$CONNECTION_CONTEXT_C

# org.ofono.SimManager
SIM_MANAGER_XML = $$DBUS_SPEC_DIR/org.ofono.SimManager.xml
SIM_MANAGER_GENERATE = gdbus-codegen --generate-c-code \
  org.ofono.SimManager $$SIM_MANAGER_XML
SIM_MANAGER_H = org.ofono.SimManager.h
org_ofono_SimManager_h.input = SIM_MANAGER_XML
org_ofono_SimManager_h.output = $$SIM_MANAGER_H
org_ofono_SimManager_h.commands = $$SIM_MANAGER_GENERATE
org_ofono_SimManager_h.CONFIG = no_link
QMAKE_EXTRA_COMPILERS += org_ofono_SimManager_h

SIM_MANAGER_C = org.ofono.SimManager.c
org_ofono_SimManager_c.input = SIM_MANAGER_XML
org_ofono_SimManager_c.output = $$SIM_MANAGER_C
org_ofono_SimManager_c.commands = $$SIM_MANAGER_GENERATE
org_ofono_SimManager_c.CONFIG = no_link
QMAKE_EXTRA_COMPILERS += org_ofono_SimManager_c
GENERATED_SOURCES += $$SIM_MANAGER_C

# org.ofono.Modem
MODEM_XML = $$DBUS_SPEC_DIR/org.ofono.Modem.xml
MODEM_GENERATE = gdbus-codegen --generate-c-code \
  org.ofono.Modem $$MODEM_XML
MODEM_H = org.ofono.Modem.h
org_ofono_Modem_h.input = MODEM_XML
org_ofono_Modem_h.output = $$MODEM_H
org_ofono_Modem_h.commands = $$MODEM_GENERATE
org_ofono_Modem_h.CONFIG = no_link
QMAKE_EXTRA_COMPILERS += org_ofono_Modem_h

MODEM_C = org.ofono.Modem.c
org_ofono_Modem_c.input = MODEM_XML
org_ofono_Modem_c.output = $$MODEM_C
org_ofono_Modem_c.commands = $$MODEM_GENERATE
org_ofono_Modem_c.CONFIG = no_link
QMAKE_EXTRA_COMPILERS += org_ofono_Modem_c
GENERATED_SOURCES += $$MODEM_C
