# -*- Mode: makefile-gmake -*-

.PHONY: all debug release clean

# Required packages
PKGS = glib-2.0 libwspcodec
LIB_PKGS = $(PKGS)

#
# Default target
#

all: debug release

#
# Sources
#

SRC = mms-dump.c

#
# Directories
#

SRC_DIR = .
BUILD_DIR = build
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
WARNINGS = -Wall
CFLAGS = $(shell pkg-config --cflags $(PKGS)) -MMD
LIBS = $(shell pkg-config --libs $(LIB_PKGS))

ifndef KEEP_SYMBOLS
KEEP_SYMBOLS = 0
endif

ifneq ($(KEEP_SYMBOLS),0)
RELEASE_FLAGS += -g
SUBMAKE_OPTS += KEEP_SYMBOLS=1
endif

DEBUG_CFLAGS = $(WARNINGS) $(DEBUG_FLAGS) $(DEBUG_DEFS) $(CFLAGS)
RELEASE_CFLAGS = $(WARNINGS) $(RELEASE_FLAGS) $(RELEASE_DEFS) $(CFLAGS)

#
# Files
#

DEBUG_OBJS = $(SRC:%.c=$(DEBUG_BUILD_DIR)/%.o)
RELEASE_OBJS = $(SRC:%.c=$(RELEASE_BUILD_DIR)/%.o)

#
# Dependencies
#

DEPS = $(DEBUG_OBJS:%.o=%.d) $(RELEASE_OBJS:%.o=%.d)
ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(DEPS)),)
-include $(DEPS)
endif
endif

$(DEBUG_OBJS): | $(DEBUG_BUILD_DIR)
$(RELEASE_OBJS): | $(RELEASE_BUILD_DIR)

#
# Rules
#

EXE = mms-dump
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

$(DEBUG_BUILD_DIR)/%.o : $(SRC_DIR)/%.c
	$(CC) -c $(DEBUG_CFLAGS) -MT"$@" -MF"$(@:%.o=%.d)" $< -o $@

$(RELEASE_BUILD_DIR)/%.o : $(SRC_DIR)/%.c
	$(CC) -c $(RELEASE_CFLAGS) -MT"$@" -MF"$(@:%.o=%.d)" $< -o $@

$(DEBUG_EXE): $(DEBUG_OBJS)
	$(LD) $(DEBUG_FLAGS) $^ $(LIBS) -o $@

$(RELEASE_EXE): $(RELEASE_OBJS)
	$(LD) $(RELEASE_FLAGS) $^ $(LIBS) -o $@
ifeq ($(KEEP_SYMBOLS),0)
	strip $@
endif
