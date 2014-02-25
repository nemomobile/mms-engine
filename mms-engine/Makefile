# -*- Mode: makefile-gmake -*-

.PHONY: all debug release clean cleaner
.PHONY: mms_lib_debug_lib mms_lib_release_lib
.PHONY: mms_ofono_debug_lib mms_ofono_release_lib
.PHONY: mms_handler_debug_lib mms_handler_release_lib

# Required packages
PKGS = gio-unix-2.0 gio-2.0 glib-2.0
LIB_PKGS = libwspcodec libsoup-2.4 $(PKGS)

#
# Default target
#

all: debug release

#
# Sources
#

SRC = main.c mms_engine.c
GEN_SRC = org.nemomobile.MmsEngine.c

#
# Directories
#

SRC_DIR = .
BUILD_DIR = build
SPEC_DIR = .
GEN_DIR = $(BUILD_DIR)
DEBUG_BUILD_DIR = $(BUILD_DIR)/debug
RELEASE_BUILD_DIR = $(BUILD_DIR)/release

#
# mms-lib
#

MMS_LIB_LIB = libmms.a
MMS_LIB_DIR = ../mms-lib
MMS_LIB_BUILD_DIR = $(MMS_LIB_DIR)/build
MMS_LIB_DEBUG_LIB = $(MMS_LIB_BUILD_DIR)/debug/$(MMS_LIB_LIB)
MMS_LIB_RELEASE_LIB = $(MMS_LIB_BUILD_DIR)/release/$(MMS_LIB_LIB)

#
# mms-ofono
#

MMS_OFONO_LIB = libmms-ofono.a
MMS_OFONO_DIR = ../mms-ofono
MMS_OFONO_BUILD_DIR = $(MMS_OFONO_DIR)/build
MMS_OFONO_DEBUG_LIB = $(MMS_OFONO_BUILD_DIR)/debug/$(MMS_OFONO_LIB)
MMS_OFONO_RELEASE_LIB = $(MMS_OFONO_BUILD_DIR)/release/$(MMS_OFONO_LIB)

#
# mms-handler-dbus
#

MMS_HANDLER_LIB = libmms-handler-dbus.a
MMS_HANDLER_DIR = ../mms-handler-dbus
MMS_HANDLER_BUILD_DIR = $(MMS_HANDLER_DIR)/build
MMS_HANDLER_DEBUG_LIB = $(MMS_HANDLER_BUILD_DIR)/debug/$(MMS_HANDLER_LIB)
MMS_HANDLER_RELEASE_LIB = $(MMS_HANDLER_BUILD_DIR)/release/$(MMS_HANDLER_LIB)

#
# Tools and flags
#

CC = $(CROSS_COMPILE)gcc
LD = $(CC)
DEBUG_FLAGS = -g
RELEASE_FLAGS = -O2
DEBUG_DEFS = -DDEBUG
RELEASE_DEFS =
WARN = -Wall
CFLAGS = $(shell pkg-config --cflags $(PKGS)) -I. -I$(GEN_DIR) \
 -I$(MMS_LIB_DIR)/include -I$(MMS_OFONO_DIR)/include \
 -I$(MMS_HANDLER_DIR)/include -MMD

ifndef KEEP_SYMBOLS
KEEP_SYMBOLS = 0
endif

ifneq ($(KEEP_SYMBOLS),0)
RELEASE_FLAGS += -g
SUBMAKE_OPTS += KEEP_SYMBOLS=1
endif

DEBUG_CFLAGS = $(DEBUG_FLAGS) $(DEBUG_DEFS) $(CFLAGS)
RELEASE_CFLAGS = $(RELEASE_FLAGS) $(RELEASE_DEFS) $(CFLAGS)

LIBS = $(shell pkg-config --libs $(LIB_PKGS)) -lmagic
DEBUG_LIBS = \
  $(MMS_OFONO_DEBUG_LIB) \
  $(MMS_HANDLER_DEBUG_LIB) \
  $(MMS_LIB_DEBUG_LIB) \
  $(LIBS)
RELEASE_LIBS = \
  $(MMS_OFONO_RELEASE_LIB) \
  $(MMS_HANDLER_RELEASE_LIB) \
  $(MMS_LIB_RELEASE_LIB) \
  $(LIBS)

#
# Files
#

.PRECIOUS: $(GEN_SRC:%=$(GEN_DIR)/%)

DEBUG_OBJS = \
  $(GEN_SRC:%.c=$(DEBUG_BUILD_DIR)/%.o) \
  $(SRC:%.c=$(DEBUG_BUILD_DIR)/%.o)
RELEASE_OBJS = \
  $(GEN_SRC:%.c=$(RELEASE_BUILD_DIR)/%.o) \
  $(SRC:%.c=$(RELEASE_BUILD_DIR)/%.o)

#
# Dependencies
#

DEBUG_DEPS = \
  mms_lib_debug_lib \
  mms_ofono_debug_lib \
  mms_handler_debug_lib
RELEASE_DEPS = \
  mms_lib_release_lib \
  mms_ofono_release_lib \
  mms_handler_release_lib
DEBUG_EXE_DEPS = \
  $(MMS_LIB_DEBUG_LIB) \
  $(MMS_OFONO_DEBUG_LIB) \
  $(MMS_HANDLER_DEBUG_LIB) \
  $(DEBUG_BUILD_DIR)
RELEASE_EXE_DEPS = \
  $(MMS_LIB_RELEASE_LIB) \
  $(MMS_OFONO_RELEASE_LIB) \
  $(MMS_HANDLER_RELEASE_LIB) \
  $(RELEASE_BUILD_DIR)
DEPS = $(DEBUG_OBJS:%.o=%.d) $(RELEASE_OBJS:%.o=%.d)
ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(DEPS)),)
-include $(DEPS)
endif
endif

#
# Rules
#

EXE = mms-engine
DEBUG_EXE = $(DEBUG_BUILD_DIR)/$(EXE)
RELEASE_EXE = $(RELEASE_BUILD_DIR)/$(EXE)

debug: $(DEBUG_DEPS) $(DEBUG_EXE)

release: $(RELEASE_DEPS) $(RELEASE_EXE)

clean:
	rm -fr $(BUILD_DIR) $(SRC_DIR)/*~

cleaner: clean
	make -C $(MMS_LIB_DIR) $(SUBMAKE_OPTS) clean
	make -C $(MMS_OFONO_DIR) $(SUBMAKE_OPTS) clean
	make -C $(MMS_HANDLER_DIR) $(SUBMAKE_OPTS) clean

$(GEN_DIR):
	mkdir -p $@

$(DEBUG_BUILD_DIR):
	mkdir -p $@

$(RELEASE_BUILD_DIR):
	mkdir -p $@

mms_lib_debug_lib:
	make -C $(MMS_LIB_DIR) $(SUBMAKE_OPTS) debug

mms_lib_release_lib:
	make -C $(MMS_LIB_DIR) $(SUBMAKE_OPTS) release

mms_ofono_debug_lib:
	make -C $(MMS_OFONO_DIR) $(SUBMAKE_OPTS) debug

mms_ofono_release_lib:
	make -C $(MMS_OFONO_DIR) $(SUBMAKE_OPTS) release

mms_handler_debug_lib:
	make -C $(MMS_HANDLER_DIR) $(SUBMAKE_OPTS) debug

mms_handler_release_lib:
	make -C $(MMS_HANDLER_DIR) $(SUBMAKE_OPTS) release

$(MMS_LIB_DEBUG_LIB): mms_lib_debug_lib

$(MMS_LIB_RELEASE_LIB): mms_lib_release_lib

$(MMS_OFONO_DEBUG_LIB): mms_ofono_debug_lib

$(MMS_OFONO_RELEASE_LIB): mms_ofono_release_lib

$(MMS_HANDLER_DEBUG_LIB): mms_handler_debug_lib

$(MMS_HANDLER_RELEASE_LIB): mms_handler_release_lib

$(GEN_DIR)/%.c: $(SPEC_DIR)/%.xml
	gdbus-codegen --generate-c-code $(@:%.c=%) $<

$(DEBUG_BUILD_DIR)/%.o : $(SRC_DIR)/%.c
	$(CC) -c $(WARN) $(DEBUG_CFLAGS) -MT"$@" -MF"$(@:%.o=%.d)" $< -o $@

$(RELEASE_BUILD_DIR)/%.o : $(SRC_DIR)/%.c
	$(CC) -c $(WARN) $(RELEASE_CFLAGS) -MT"$@" -MF"$(@:%.o=%.d)" $< -o $@

$(DEBUG_BUILD_DIR)/%.o : $(GEN_DIR)/%.c
	$(CC) -c $(DEBUG_CFLAGS) -MT"$@" -MF"$(@:%.o=%.d)" $< -o $@

$(RELEASE_BUILD_DIR)/%.o : $(GEN_DIR)/%.c
	$(CC) -c $(RELEASE_CFLAGS) -MT"$@" -MF"$(@:%.o=%.d)" $< -o $@

$(DEBUG_EXE): $(DEBUG_EXE_DEPS) $(DEBUG_OBJS)
	$(LD) $(DEBUG_FLAGS) $(DEBUG_OBJS) $(DEBUG_LIBS) -o $@

$(RELEASE_EXE): $(RELEASE_EXE_DEPS) $(RELEASE_OBJS)
	$(LD) $(RELEASE_FLAGS) $(RELEASE_OBJS) $(RELEASE_LIBS) -o $@
ifeq ($(KEEP_SYMBOLS),0)
	strip $@
endif
