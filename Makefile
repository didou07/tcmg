# tcmg v4.8 — Makefile
# Targets: Linux, macOS, Windows (MinGW)
#
# Usage:
#   make                    → debug build          → build/tcmg
#   make RELEASE=1          → release build        → build/tcmg
#   make assets             → regenerate webif_assets.h from CSS+JS
#   make clean              → remove build/

CC      ?= gcc
STRIP   ?= strip
PYTHON  ?= python3
RELEASE ?= 0

BUILD_DIR := build
OBJ_DIR   := $(BUILD_DIR)/obj

# ── Source list (grouped by layer) ──────────────────────────────────────────
SRCS := \
	src/globals.c \
	src/main.c \
	src/client.c \
	src/core/log.c \
	src/core/conf.c \
	src/core/ban.c \
	src/core/emu.c \
	src/core/srvid.c \
	src/net/net.c \
	src/crypto/crypto.c \
	src/platform/platform.c \
	src/webif/webif.c \
	src/webif/webif_common.c \
	src/webif/webif_layout.c \
	src/webif/webif_page_login.c \
	src/webif/webif_page_status.c \
	src/webif/webif_page_users.c \
	src/webif/webif_page_system.c \
	src/webif/webif_api.c \
	src/webif/webif_tvcas.c

# Map src/sub/file.c → build/obj/sub__file.o  (avoids name collisions)
obj_name = $(OBJ_DIR)/$(subst /,__,$(patsubst src/%.c,%.o,$(1)))
OBJS := $(foreach s,$(SRCS),$(call obj_name,$(s)))

# ── Platform detection ───────────────────────────────────────────────────────
UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)

ifeq ($(findstring MINGW,$(UNAME_S)),MINGW)
  PLATFORM  := windows
  TARGET    := $(BUILD_DIR)/tcmg_x64.exe
  CFLAGS    += -DTCMG_OS_WINDOWS -DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0601
  LDFLAGS   += -lws2_32 -ladvapi32 -lbcrypt -static -static-libgcc -lpthread
else ifeq ($(UNAME_S),Darwin)
  PLATFORM  := macos
  TARGET    := $(BUILD_DIR)/tcmg
  LDFLAGS   += -lpthread
else ifneq ($(findstring mingw,$(CC)),)
  PLATFORM  := windows-cross
  TARGET    := $(BUILD_DIR)/tcmg_x64.exe
  CFLAGS    += -DTCMG_OS_WINDOWS -DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0601
  LDFLAGS   += -lws2_32 -ladvapi32 -lbcrypt -static -static-libgcc -lpthread
else
  PLATFORM  := linux
  TARGET    := $(BUILD_DIR)/tcmg
  LDFLAGS   += -lpthread -lm
endif

# ── Compile flags ─────────────────────────────────────────────────────────────
# -Isrc  : lets any .c file use #include "tcmg.h", #include "core/log.h", etc.
# Overlength-strings: CSS/JS literals exceed the ISO C99 minimum (4095 chars);
# this is intentional for an embedded web interface.
BASE_FLAGS := -std=c11 -Wall -Wextra -Wno-unused-parameter \
              -Wno-overlength-strings \
              -Wno-format \
              -Isrc -D_FORTIFY_SOURCE=2

ifeq ($(RELEASE),1)
  CFLAGS += $(BASE_FLAGS) -Os \
            -ffunction-sections -fdata-sections \
            -fmerge-all-constants -fno-ident \
            -fstack-protector-strong \
            -march=x86-64 -mtune=generic
  ifeq ($(PLATFORM),linux)
    LDFLAGS += -Wl,--gc-sections -Wl,--strip-all -Wl,--build-id=none
  endif
else
  CFLAGS += $(BASE_FLAGS) -O2 -g
endif

# ── Rules ─────────────────────────────────────────────────────────────────────
.PHONY: all clean debug release assets

all: $(TARGET)

# Regenerate embedded CSS/JS header from source assets
assets:
	$(PYTHON) tools/gen_assets.py
	@echo "webif_assets.h regenerated — rebuild required (make clean && make)"

$(TARGET): $(OBJS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)
ifeq ($(RELEASE),1)
	$(STRIP) --strip-all $@ 2>/dev/null || true
endif
	@echo "Built: $@  (platform=$(PLATFORM))"

# Pattern rule: src/sub/file.c → build/obj/sub__file.o
define COMPILE_RULE
$(call obj_name,$(1)): $(1) | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $$< -o $$@
endef
$(foreach s,$(SRCS),$(eval $(call COMPILE_RULE,$(s))))

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

debug: CFLAGS  += -g -O0 -DDEBUG -fsanitize=address
debug: LDFLAGS += -fsanitize=address
debug: $(TARGET)

release:
	$(MAKE) RELEASE=1

clean:
	rm -rf $(BUILD_DIR)
