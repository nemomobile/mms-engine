# -*- Mode: makefile-gmake -*-

.PHONY: all debug release clean

# Required packages
PKGS = gio-unix-2.0 gio-2.0 glib-2.0
LIB_PKGS = $(PKGS)

#
# Default target
#

all: debug release

#
# Sources
#

SRC = mms-send.c
GEN_SRC = org.nemomobile.MmsEngine.c

#
# Directories
#

SRC_DIR = .
SPEC_DIR = ../mms-engine
BUILD_DIR = build
GEN_DIR = $(BUILD_DIR)
DEBUG_BUILD_DIR = $(BUILD_DIR)/debug
RELEASE_BUILD_DIR = $(BUILD_DIR)/release

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
CFLAGS = $(shell pkg-config --cflags $(PKGS)) -I. -I$(GEN_DIR) -MMD
LIBS = $(shell pkg-config --libs $(LIB_PKGS))

ifndef KEEP_SYMBOLS
KEEP_SYMBOLS = 0
endif

ifneq ($(KEEP_SYMBOLS),0)
RELEASE_FLAGS += -g
endif

DEBUG_CFLAGS = $(DEBUG_FLAGS) $(DEBUG_DEFS) $(CFLAGS)
RELEASE_CFLAGS = $(RELEASE_FLAGS) $(RELEASE_DEFS) $(CFLAGS)
DEBUG_LIBS = $(LIBS)
RELEASE_LIBS = $(LIBS)

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

DEBUG_EXE_DEPS = $(DEBUG_BUILD_DIR)
RELEASE_EXE_DEPS = $(RELEASE_BUILD_DIR)
DEPS = $(DEBUG_OBJS:%.o=%.d) $(RELEASE_OBJS:%.o=%.d)
ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(DEPS)),)
-include $(DEPS)
endif
endif

#
# Rules
#

EXE = mms-send
DEBUG_EXE = $(DEBUG_BUILD_DIR)/$(EXE)
RELEASE_EXE = $(RELEASE_BUILD_DIR)/$(EXE)

debug: $(DEBUG_EXE)

release: $(RELEASE_EXE) 

clean:
	rm -fr $(BUILD_DIR) $(SRC_DIR)/*~

$(DEBUG_BUILD_DIR):
	mkdir -p $@

$(RELEASE_BUILD_DIR):
	mkdir -p $@

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
