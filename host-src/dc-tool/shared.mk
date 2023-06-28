SHARED_OBJECT_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

SHARED_OBJECTS := \
	  $(SHARED_OBJECT_DIR)/commands.o \
	  $(SHARED_OBJECT_DIR)/gdb.o \
	  $(SHARED_OBJECT_DIR)/ip-syscalls.o \
	  $(SHARED_OBJECT_DIR)/ip-transport.o \
	  $(SHARED_OBJECT_DIR)/lzo.o \
	  $(SHARED_OBJECT_DIR)/mingw.o \
	  $(SHARED_OBJECT_DIR)/serial-syscalls.o \
	  $(SHARED_OBJECT_DIR)/serial-transport.o \
	  $(SHARED_OBJECT_DIR)/utils.o

