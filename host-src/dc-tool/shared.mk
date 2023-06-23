SHARED_OBJECT_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

SHARED_OBJECTS := \
	  $(SHARED_OBJECT_DIR)/gdb.o \
	  $(SHARED_OBJECT_DIR)/mingw.o \
	  $(SHARED_OBJECT_DIR)/utils.o

