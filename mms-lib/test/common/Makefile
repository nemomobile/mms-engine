# -*- Mode: makefile-gmake -*-

.PHONY: all debug release clean test test_banner
.PHONY: mms_lib_debug_lib mms_lib_release_lib

#
# Real test makefile defines EXE and SRC and includes this one.
#

ifndef SRC
${error SRC not defined}
endif

ifndef EXE
${error EXE not defined}
endif

#
# Pull in mms-lib configuration
#

include ../../Config.mak

#
# Required packages
#

LIB_PKGS += libwspcodec libsoup-2.4 glib-2.0 $(RESIZE_PKG)
PKGS += $(LIB_PKGS)

#
# Default target
#

all: debug release

#
# Directories
#

SRC_DIR = .
COMMON_DIR = ../common
BUILD_DIR = build
DEBUG_BUILD_DIR = $(BUILD_DIR)/debug
RELEASE_BUILD_DIR = $(BUILD_DIR)/release

#
# Code coverage
#

ifndef GCOV
GCOV = 0
endif

ifneq ($(GCOV),0)
BASE_FLAGS += --coverage
endif

#
# mms-lib
#

MMS_LIB_LIB = libmms.a
MMS_LIB_DIR = ../..
MMS_LIB_BUILD_DIR = $(MMS_LIB_DIR)/build
MMS_LIB_DEBUG_LIB = $(MMS_LIB_BUILD_DIR)/debug/$(MMS_LIB_LIB)
MMS_LIB_RELEASE_LIB = $(MMS_LIB_BUILD_DIR)/release/$(MMS_LIB_LIB)
MMS_LIB_MAKE = $(MAKE) --no-print-directory -C $(MMS_LIB_DIR) GCOV=$(GCOV)

#
# Tools and flags
#

CC = $(CROSS_COMPILE)gcc
LD = $(CC)
DEBUG_FLAGS = $(BASE_FLAGS) -g
RELEASE_FLAGS = $(BASE_FLAGS) -O2
DEBUG_DEFS = -DDEBUG
RELEASE_DEFS =
WARNINGS = -Wall
LIBS = $(shell pkg-config --libs $(LIB_PKGS)) -lmagic -ljpeg $(RESIZE_LIBS)
CFLAGS = -fPIE $(shell pkg-config --cflags $(PKGS)) -I$(MMS_LIB_DIR)/include \
  -I$(MMS_LIB_DIR)/src -I$(COMMON_DIR) -MMD

DEBUG_CFLAGS = $(WARNINGS) $(DEBUG_FLAGS) $(DEBUG_DEFS) $(CFLAGS)
RELEASE_CFLAGS = $(WARNINGS) $(RELEASE_FLAGS) $(RELEASE_DEFS) $(CFLAGS)
DEBUG_LIBS = $(MMS_LIB_DEBUG_LIB) $(LIBS)
RELEASE_LIBS = $(MMS_LIB_RELEASE_LIB) $(LIBS)

#
# Files
#

DEBUG_OBJS = \
  $(SRC:%.c=$(DEBUG_BUILD_DIR)/%.o) \
  $(COMMON_SRC:%.c=$(DEBUG_BUILD_DIR)/%.o)
RELEASE_OBJS = \
  $(SRC:%.c=$(RELEASE_BUILD_DIR)/%.o) \
  $(COMMON_SRC:%.c=$(RELEASE_BUILD_DIR)/%.o)

#
# Dependencies
#

DEBUG_DEPS = mms_lib_debug_lib
RELEASE_DEPS = mms_lib_release_lib
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

DEBUG_EXE = $(DEBUG_BUILD_DIR)/$(EXE)
RELEASE_EXE = $(RELEASE_BUILD_DIR)/$(EXE)

debug: $(DEBUG_DEPS) $(DEBUG_EXE)

release: $(RELEASE_DEPS) $(RELEASE_EXE) 

clean:
	$(MAKE) -w -C $(MMS_LIB_DIR) clean
	rm -fr $(BUILD_DIR) $(SRC_DIR)/*~

test_banner:
	@echo "===========" $(EXE) "=========== "

test: test_banner debug
	@$(DEBUG_EXE)

mms_lib_debug_lib:
	@$(MMS_LIB_MAKE) debug

mms_lib_release_lib:
	@$(MMS_LIB_MAKE) release

$(MMS_LIB_DEBUG_LIB): mms_lib_debug_lib

$(MMS_LIB_RELEASE_LIB): mms_lib_release_lib

$(DEBUG_BUILD_DIR):
	mkdir -p $@

$(RELEASE_BUILD_DIR):
	mkdir -p $@

$(DEBUG_BUILD_DIR)/%.o : $(SRC_DIR)/%.c
	$(CC) -c $(DEBUG_CFLAGS) -MT"$@" -MF"$(@:%.o=%.d)" $< -o $@

$(RELEASE_BUILD_DIR)/%.o : $(SRC_DIR)/%.c
	$(CC) -c $(RELEASE_CFLAGS) -MT"$@" -MF"$(@:%.o=%.d)" $< -o $@

$(DEBUG_BUILD_DIR)/%.o : $(COMMON_DIR)/%.c
	$(CC) -c $(DEBUG_CFLAGS) -MT"$@" -MF"$(@:%.o=%.d)" $< -o $@

$(RELEASE_BUILD_DIR)/%.o : $(COMMON_DIR)/%.c
	$(CC) -c $(RELEASE_CFLAGS) -MT"$@" -MF"$(@:%.o=%.d)" $< -o $@

$(DEBUG_EXE): $(DEBUG_EXE_DEPS) $(DEBUG_OBJS)
	$(LD) $(DEBUG_FLAGS) $(DEBUG_OBJS) $(DEBUG_LIBS) -o $@

$(RELEASE_EXE): $(RELEASE_EXE_DEPS) $(RELEASE_OBJS)
	$(LD) $(RELEASE_FLAGS) $(RELEASE_OBJS) $(RELEASE_LIBS) -o $@
	strip $@
