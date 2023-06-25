# Determine on what platform we are running

# This is much more simpler than using the "config.guess" mechanism, as we 
# don't need the full host triplet, but it will be sufficient here.

# For a more complete mechanism, based on host triplet and "config.guess", 
# check the "dc-chain/Makefile".

HOST := $(shell uname -s)

# BSD
ifneq ($(shell echo $(HOST) | grep -i 'BSD$$'),)
    BSD := 1
endif

# macOS
ifeq ($(shell echo $(HOST)),Darwin)
    MACOS := 1
endif

HOMEBREW_PREFIX := $(shell brew --prefix 2>/dev/null)

# MinGW/MSYS
ifeq ($(shell echo $(HOST) | cut -c-5),MINGW)
	# Both MinGW/MSYS legacy environment and MinGW-w64/MSYS2 environment
    MINGW32 := 1
    mingw_w64_checker = $(shell echo $$MSYSTEM_CHOST)
    ifneq ($(mingw_w64_checker),)
	# Only MinGW-w64/MSYS2 environment, both for x86 / x64
        MINGW64 := 1
    else
	# Only original and legacy MinGW/MSYS environment
        MINGW := 1
    endif
    WINDOWS := 1
endif

# Cygwin
ifeq ($(shell echo $(HOST) | cut -c-6),CYGWIN)
    CYGWIN := 1
    WINDOWS := 1
endif
