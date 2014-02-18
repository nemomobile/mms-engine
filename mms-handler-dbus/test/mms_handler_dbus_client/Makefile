# -*- Mode: makefile-gmake -*-

.PHONY: all debug release clean
.PHONY: mms_lib_debug_lib mms_lib_release_lib

# Required packages
LIB_PKGS = libsoup-2.4 glib-2.0 gio-2.0 gio-unix-2.0
PKGS = $(LIB_PKGS)

#
# Default target
#

all: debug release

#
# Sources
#

SRC = test_mms_handler_dbus_client.c
SRC1 = mms_handler_dbus.c
GEN_SRC = org.nemomobile.MmsHandler.c

#
# Directories
#

SRC_DIR = .
SRC1_DIR = ../../src
BUILD_DIR = build
GEN_DIR = $(BUILD_DIR)
SPEC_DIR = ../../spec
DEBUG_BUILD_DIR = $(BUILD_DIR)/debug
RELEASE_BUILD_DIR = $(BUILD_DIR)/release

#
# mms-lib
#

MMS_LIB_LIB = libmms.a
MMS_LIB_DIR = ../../../mms-lib
MMS_LIB_BUILD_DIR = $(MMS_LIB_DIR)/build
MMS_LIB_DEBUG_LIB = $(MMS_LIB_BUILD_DIR)/debug/$(MMS_LIB_LIB)
MMS_LIB_RELEASE_LIB = $(MMS_LIB_BUILD_DIR)/release/$(MMS_LIB_LIB)

#
# Tools and flags
#

CC = $(CROSS_COMPILE)gcc
LD = $(CC)
DEBUG_FLAGS = -g
RELEASE_FLAGS = -O2
DEBUG_DEFS = -DDEBUG
RELEASE_DEFS =
WARNINGS = -Wall
LIBS = $(shell pkg-config --libs $(LIB_PKGS))
CFLAGS = $(shell pkg-config --cflags $(PKGS)) -I. -I../../include \
  -I$(MMS_LIB_DIR)/include -I$(SRC1_DIR) -I$(GEN_DIR) -MMD

DEBUG_CFLAGS = $(WARNINGS) $(DEBUG_FLAGS) $(DEBUG_DEFS) $(CFLAGS)
RELEASE_CFLAGS = $(WARNINGS) $(RELEASE_FLAGS) $(RELEASE_DEFS) $(CFLAGS)
DEBUG_LIBS = $(MMS_LIB_DEBUG_LIB) $(LIBS)
RELEASE_LIBS = $(MMS_LIB_RELEASE_LIB) $(LIBS)

#
# Files
#

.PRECIOUS: $(GEN_SRC:%=$(GEN_DIR)/%)

DEBUG_OBJS = \
  $(GEN_SRC:%.c=$(DEBUG_BUILD_DIR)/%.o) \
  $(SRC:%.c=$(DEBUG_BUILD_DIR)/%.o) \
  $(SRC1:%.c=$(DEBUG_BUILD_DIR)/%.o)
RELEASE_OBJS = \
  $(GEN_SRC:%.c=$(RELEASE_BUILD_DIR)/%.o) \
  $(SRC:%.c=$(RELEASE_BUILD_DIR)/%.o) \
  $(SRC1:%.c=$(RELEASE_BUILD_DIR)/%.o)

#
# Dependencies
#

DEBUG_EXE_DEPS = $(MMS_LIB_DEBUG_LIB) $(DEBUG_BUILD_DIR)
RELEASE_EXE_DEPS = $(MMS_LIB_RELEASE_LIB) $(RELEASE_BUILD_DIR)
DEPS = $(DEBUG_OBJS:%.o=%.d) $(RELEASE_OBJS:%.o=%.d)
ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(DEPS)),)
-include $(DEPS)
endif
endif

#
# Rules
#

EXE = test_mms_handler_dbus_client
DEBUG_EXE = $(DEBUG_BUILD_DIR)/$(EXE)
RELEASE_EXE = $(RELEASE_BUILD_DIR)/$(EXE)

debug: $(DEBUG_EXE)

release: $(RELEASE_EXE) 

clean:
	make -C $(MMS_LIB_DIR) clean
	rm -fr $(BUILD_DIR) $(SRC_DIR)/*~

mms_lib_debug_lib:
	make -C $(MMS_LIB_DIR) debug

mms_lib_release_lib:
	make -C $(MMS_LIB_DIR) release

$(MMS_LIB_DEBUG_LIB): mms_lib_debug_lib

$(MMS_LIB_RELEASE_LIB): mms_lib_release_lib

$(GEN_DIR):
	mkdir -p $@

$(DEBUG_BUILD_DIR):
	mkdir -p $@

$(RELEASE_BUILD_DIR):
	mkdir -p $@

$(GEN_DIR)/%.c: $(SPEC_DIR)/%.xml
	gdbus-codegen --generate-c-code $(@:%.c=%) $<

$(DEBUG_BUILD_DIR)/%.o : $(SRC_DIR)/%.c
	$(CC) -c $(DEBUG_CFLAGS) -MT"$@" -MF"$(@:%.o=%.d)" $< -o $@

$(RELEASE_BUILD_DIR)/%.o : $(SRC_DIR)/%.c
	$(CC) -c $(RELEASE_CFLAGS) -MT"$@" -MF"$(@:%.o=%.d)" $< -o $@

$(DEBUG_BUILD_DIR)/%.o : $(SRC1_DIR)/%.c
	$(CC) -c $(DEBUG_CFLAGS) -MT"$@" -MF"$(@:%.o=%.d)" $< -o $@

$(RELEASE_BUILD_DIR)/%.o : $(SRC1_DIR)/%.c
	$(CC) -c $(RELEASE_CFLAGS) -MT"$@" -MF"$(@:%.o=%.d)" $< -o $@

$(DEBUG_BUILD_DIR)/%.o : $(GEN_DIR)/%.c
	$(CC) -c $(DEBUG_CFLAGS) -MT"$@" -MF"$(@:%.o=%.d)" $< -o $@

$(RELEASE_BUILD_DIR)/%.o : $(GEN_DIR)/%.c
	$(CC) -c $(RELEASE_CFLAGS) -MT"$@" -MF"$(@:%.o=%.d)" $< -o $@

$(DEBUG_EXE): $(DEBUG_EXE_DEPS) $(DEBUG_OBJS)
	$(LD) $(DEBUG_FLAGS) $(DEBUG_OBJS) $(DEBUG_LIBS) -o $@

$(RELEASE_EXE): $(RELEASE_EXE_DEPS) $(RELEASE_OBJS)
	$(LD) $(RELEASE_FLAGS) $(RELEASE_OBJS) $(RELEASE_LIBS) -o $@
	strip $@
