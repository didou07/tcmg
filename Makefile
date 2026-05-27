# =============================================================================
# Makefile — tcmg
# Source tree: all C files live in src/
#
# Targets:
#   make                    native debug build → tcmg
#   make RELEASE=1          optimised + stripped
#   make CC=arm-linux-gnueabihf-gcc   cross-compile
#   make clean
#
# Override variables:
#   SRC      source directory  (default: src)
#   OUT      output binary     (default: tcmg[.exe])
# =============================================================================

PROG   = tcmg
CC    ?= gcc
STRIP ?= strip
SRC   ?= src

# ── Source & object lists ─────────────────────────────────────────────────────
SRCS := \
    $(SRC)/tcmg.c        \
    $(SRC)/tcmg-log.c    \
    $(SRC)/tcmg-crypto.c \
    $(SRC)/tcmg-net.c    \
    $(SRC)/tcmg-ban.c    \
    $(SRC)/tcmg-conf.c   \
    $(SRC)/tcmg-emu.c    \
    $(SRC)/tcmg-srvid2.c \
    $(SRC)/tcmg-webif.c         \
    $(SRC)/tcmg-webif-common.c  \
    $(SRC)/tcmg-webif-layout.c  \
    $(SRC)/tcmg-webif-pages.c   \
    $(SRC)/tcmg-webif-tvcas.c

OBJS := $(SRCS:.c=.o)

# ── Compiler flags ────────────────────────────────────────────────────────────
WARN := \
    -Wall -Wextra -Wshadow      \
    -Wno-unused-parameter       \
    -Wmissing-prototypes        \
    -Wstrict-prototypes

STD  := -std=c11 -D_GNU_SOURCE

ifdef RELEASE
  OPT  := -Os -ffunction-sections -fdata-sections \
           -fomit-frame-pointer -fstack-protector-strong \
           -fmerge-all-constants -fno-ident \
           -D_FORTIFY_SOURCE=2
  LDFL := -Wl,--gc-sections -Wl,--strip-all
  # -z relro / -z now are ELF-only hardening flags.
  # MinGW ld.exe (Windows) rejects them with "unrecognized option '-z'".
  # Skip them when building on/for Windows.
  ifneq ($(OS),Windows_NT)
    ifeq (,$(findstring mingw,$(CC)))
      LDFL += -Wl,-z,relro -Wl,-z,now
    endif
  endif
  DO_STRIP := 1
else
  OPT  := -Og -g
  LDFL :=
endif

CFLAGS  := $(STD) $(WARN) $(OPT) -I$(SRC) $(EXTRA_CFLAGS)
LDFLAGS := $(LDFL) -lpthread $(EXTRA_LDFLAGS)

# ── Platform detection ────────────────────────────────────────────────────────
ifeq ($(OS),Windows_NT)
  LDFLAGS += -lbcrypt -lws2_32 -ladvapi32 -static -static-libgcc
  PROG    := $(PROG).exe
else
  ifneq (,$(findstring mingw,$(CC)))
    LDFLAGS += -lbcrypt -lws2_32 -ladvapi32 -static -static-libgcc
    PROG    := $(PROG).exe
  endif
endif

# ── Rules ─────────────────────────────────────────────────────────────────────
.PHONY: all clean

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
ifdef DO_STRIP
	$(STRIP) --strip-all $@
endif
	@echo "  LINK  $@  ($(shell ls -lh $@ | awk '{print $$5}'))"

$(SRC)/%.o: $(SRC)/%.c
	@echo "  CC    $<"
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(PROG) tcmg.exe

# ── Header dependencies ───────────────────────────────────────────────────────
D := $(SRC)
$(D)/tcmg.o:        $(D)/tcmg-globals.h $(D)/tcmg-log.h $(D)/tcmg-net.h   \
                    $(D)/tcmg-crypto.h  $(D)/tcmg-conf.h $(D)/tcmg-emu.h   \
                    $(D)/tcmg-ban.h     $(D)/tcmg-webif.h $(D)/tcmg-srvid2.h
$(D)/tcmg-log.o:    $(D)/tcmg-globals.h $(D)/tcmg-log.h   $(D)/tcmg-srvid2.h
$(D)/tcmg-srvid2.o: $(D)/tcmg-srvid2.h
$(D)/tcmg-crypto.o: $(D)/tcmg-globals.h $(D)/tcmg-crypto.h $(D)/tcmg-net.h
$(D)/tcmg-net.o:    $(D)/tcmg-globals.h $(D)/tcmg-net.h    $(D)/tcmg-crypto.h
$(D)/tcmg-ban.o:    $(D)/tcmg-globals.h $(D)/tcmg-ban.h
$(D)/tcmg-conf.o:   $(D)/tcmg-globals.h $(D)/tcmg-conf.h
$(D)/tcmg-emu.o:    $(D)/tcmg-globals.h $(D)/tcmg-emu.h    $(D)/tcmg-crypto.h
$(D)/tcmg-webif.o:         $(D)/tcmg-globals.h $(D)/tcmg-webif.h  $(D)/tcmg-log.h $(D)/tcmg-webif-internal.h
$(D)/tcmg-webif-common.o:  $(D)/tcmg-globals.h $(D)/tcmg-webif-internal.h
$(D)/tcmg-webif-layout.o:  $(D)/tcmg-globals.h $(D)/tcmg-webif-internal.h
$(D)/tcmg-webif-pages.o:   $(D)/tcmg-globals.h $(D)/tcmg-webif-internal.h
$(D)/tcmg-webif-tvcas.o:   $(D)/tcmg-globals.h $(D)/tcmg-webif-internal.h
