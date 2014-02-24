# -*- Mode: makefile-gmake -*-

.PHONY: clean all debug release

# Required packages
PKGS = glib-2.0 libsoup-2.4 libwspcodec

#
# Default target
#

all: debug release

#
# Sources
#

SRC = mms_codec.c mms_connection.c mms_connman.c mms_dispatcher.c \
  mms_error.c mms_handler.c mms_lib_util.c mms_file_util.c mms_log.c \
  mms_message.c mms_task.c mms_task_ack.c mms_task_decode.c \
  mms_task_http.c mms_task_notification.c mms_task_notifyresp.c \
  mms_task_publish.c mms_task_read.c mms_task_retrieve.c mms_util.c

#
# Directories
#

SRC_DIR = src
INCLUDE_DIR = include
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
CFLAGS += --coverage
endif

#
# Tools and flags
#

CC = $(CROSS_COMPILE)gcc
LD = $(CC)
ARFLAGS = rc
DEBUG_FLAGS = -g
RELEASE_FLAGS = -O2
DEBUG_DEFS = -DDEBUG
RELEASE_DEFS =
WARNINGS = -Wall
INCLUDES = -I$(SRC_DIR) -I$(INCLUDE_DIR)
CFLAGS += $(WARNINGS) $(INCLUDES) $(shell pkg-config --cflags $(PKGS)) -MMD

ifndef KEEP_SYMBOLS
KEEP_SYMBOLS = 0
endif

ifneq ($(KEEP_SYMBOLS),0)
RELEASE_FLAGS += -g
endif

DEBUG_CFLAGS = $(DEBUG_FLAGS) $(DEBUG_DEFS) $(CFLAGS)
RELEASE_CFLAGS = $(RELEASE_FLAGS) $(RELEASE_DEFS) $(CFLAGS)

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

#
# Rules
#
LIB = libmms.a
DEBUG_LIB = $(DEBUG_BUILD_DIR)/$(LIB)
RELEASE_LIB = $(RELEASE_BUILD_DIR)/$(LIB)

debug: $(DEBUG_LIB)

release: $(RELEASE_LIB) 

clean:
	rm -fr $(BUILD_DIR) *~ $(SRC_DIR)/*~ $(INCLUDE_DIR)/*~

$(DEBUG_BUILD_DIR):
	mkdir -p $@

$(RELEASE_BUILD_DIR):
	mkdir -p $@

$(DEBUG_LIB): $(DEBUG_BUILD_DIR) $(DEBUG_OBJS)
	$(AR) $(ARFLAGS) $@ $(DEBUG_OBJS)

$(RELEASE_LIB): $(RELEASE_BUILD_DIR) $(RELEASE_OBJS)
	$(AR) $(ARFLAGS) $@ $(RELEASE_OBJS)

$(DEBUG_BUILD_DIR)/%.o : $(SRC_DIR)/%.c
	$(CC) -c $(DEBUG_CFLAGS) -MT"$@" -MF"$(@:%.o=%.d)" $< -o $@

$(RELEASE_BUILD_DIR)/%.o : $(SRC_DIR)/%.c
	$(CC) -c $(RELEASE_CFLAGS) -MT"$@" -MF"$(@:%.o=%.d)" $< -o $@
