MINILZO_DIR := $(dir $(lastword $(MAKEFILE_LIST)))/../minilzo.106

LZO_INCLUDES := -I$(MINILZO_DIR)
