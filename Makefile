# Makefile — tcmg
#
# Usage:
#   make              — native debug build
#   make RELEASE=1    — optimised + stripped
#   make CC=arm-linux-gnueabihf-gcc  — cross-compile
#   make clean

PROG    = tcmg
CC     ?= gcc
STRIP  ?= strip

SRCS = \
	tcmg.c \
	tcmg-log.c \
	tcmg-crypto.c \
	tcmg-net.c \
	tcmg-ban.c \
	tcmg-conf.c \
	tcmg-emu.c \
	tcmg-srvid2.c \
	tcmg-webif.c

OBJS = $(SRCS:.c=.o)

WARN = \
	-Wall -Wextra -Wshadow \
	-Wno-unused-parameter \
	-Wmissing-prototypes \
	-Wstrict-prototypes

STD  = -std=c11 -D_GNU_SOURCE

ifdef RELEASE
  OPT  = -Os -ffunction-sections -fdata-sections -fomit-frame-pointer \
         -fstack-protector-strong -D_FORTIFY_SOURCE=2
  LDFL = -Wl,--gc-sections -Wl,--strip-all
  DO_STRIP = 1
else
  OPT  = -Og -g
  LDFL =
endif

CFLAGS  = $(STD) $(WARN) $(OPT) $(EXTRA_CFLAGS)
LDFLAGS = $(LDFL) -lpthread $(EXTRA_LDFLAGS)

# ── Windows (MinGW) detection ─────────────────────────────────────────────────
ifeq ($(OS),Windows_NT)
  LDFLAGS += -lbcrypt -lws2_32
  PROG    := $(PROG).exe
else
  # Also detect MinGW cross-compiler on Linux/macOS
  ifneq (,$(findstring mingw,$(CC)))
    LDFLAGS += -lbcrypt -lws2_32
    PROG    := $(PROG).exe
  endif
endif

.PHONY: all clean

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
ifdef DO_STRIP
	$(STRIP) --strip-all $@
endif
	@echo "  LINK  $@"
	@ls -lh $@

%.o: %.c
	@echo "  CC    $<"
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(PROG) *.zip

# Header dependencies
tcmg.o:        tcmg-globals.h tcmg-log.h tcmg-net.h tcmg-crypto.h tcmg-conf.h tcmg-emu.h tcmg-ban.h tcmg-webif.h tcmg-srvid2.h
tcmg-log.o:    tcmg-globals.h tcmg-log.h tcmg-srvid2.h
tcmg-srvid2.o:  tcmg-srvid2.h
tcmg-crypto.o: tcmg-globals.h tcmg-crypto.h tcmg-net.h
tcmg-net.o:    tcmg-globals.h tcmg-net.h tcmg-crypto.h
tcmg-ban.o:    tcmg-globals.h tcmg-ban.h
tcmg-conf.o:   tcmg-globals.h tcmg-conf.h
tcmg-emu.o:    tcmg-globals.h tcmg-emu.h tcmg-crypto.h
tcmg-webif.o:  tcmg-globals.h tcmg-webif.h tcmg-log.h
