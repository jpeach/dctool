include ../../build/hostdetect.mk
include ../../build/hostconfig.mk
include ../../build/minilzo.mk

include shared.mk

CC	= $(HOSTCC)
CFLAGS	= $(HOSTCFLAGS)
LDFLAGS = $(HOSTLDFLAGS)
INCLUDE = $(LZO_INCLUDES) -I/usr/local/include

LIBS 	=

DEFS	= -DDCLOAD_VERSION=\"$(VERSION)\" \
	  -DDREAMCAST_IP=\"$(DREAMCAST_IP)\" \
	  -DDEFAULT_SPEED=$(TOOL_DEFAULT_SPEED) \
	  -DSERIALDEVICE="\"$(SERIALDEVICE)\"" \
	  -DB1500000 \
	  -DB500000

# Adding static flag if asked.
ifeq ($(STANDALONE_BINARY),1)
  LDFLAGS += -static
endif

# Determine if we need zlib or not in that context for building dc-tool
# The purpose of that is just to have '-lz' once at the end of the command line
ZLIB_REQUIRED := 0

# Linking with 'libelf' or 'libbfd' (sh-elf), depending of 'Makefile.cfg'
ifeq ($(WITH_BFD),1)
  # Starting from Binutils 2.40, SFrame is required when using BFD
  LIBSFRAME	=
  ifneq ("$(wildcard $(TARGETPREFIX)/lib/libsframe.a)","")
    LIBSFRAME = -lsframe
  endif
  
  DEFS		+= -DWITH_BFD
  LDFLAGS	+= -L$(BFDLIB)
  INCLUDE	+= -I$(BFDINCLUDE)
  LIBS		+= -lbfd $(LIBSFRAME) -liberty -lintl
  
  ZLIB_REQUIRED := 1
else
  LDFLAGS	+= -L$(ELFLIB)
  INCLUDE	+= -I$(ELFINCLUDE)
  LIBS		+= -lelf
endif

# Additional libraries for MinGW/MSYS or MinGW-w64/MSYS2
ifdef MINGW32
  LIBS		+= -lws2_32 -lwsock32 -liconv
  ZLIB_REQUIRED := 1
endif

# Add zlib to the command line end... if required
ifeq ($(ZLIB_REQUIRED),1)
  LIBS		+= -lz
endif

DCTOOL 	:= dc-tool$(EXECUTABLEEXTENSION)

OBJECTS	:= dc-tool.o $(SHARED_OBJECTS)

%.o: %.c
	$(CC) $(CFLAGS) $(DEFS) $(INCLUDE) -o $@ -c $< 

all: $(DCTOOL)

$(DCTOOL): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

.PHONY : install
install: $(DCTOOL) | $(TOOLINSTALLDIR)	
	cp $(DCTOOL) $(TOOLINSTALLDIR)
$(TOOLINSTALLDIR):
	-mkdir $(TOOLINSTALLDIR)

.PHONY : clean
clean:
	rm -f $(OBJECTS)

.PHONY : distclean
distclean: clean 
	rm -f $(DCTOOL)
