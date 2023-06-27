SHARED_OBJECT_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

SHARED_OBJECTS := \
	  $(SHARED_OBJECT_DIR)/download.o \
	  $(SHARED_OBJECT_DIR)/gdb.o \
	  $(SHARED_OBJECT_DIR)/ip-syscalls.o \
	  $(SHARED_OBJECT_DIR)/ip-transport.o \
	  $(SHARED_OBJECT_DIR)/lzo.o \
	  $(SHARED_OBJECT_DIR)/mingw.o \
	  $(SHARED_OBJECT_DIR)/upload.o \
	  $(SHARED_OBJECT_DIR)/utils.o

