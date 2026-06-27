CC      ?= gcc
STRIP   ?= strip
PYTHON  ?= python3
RELEASE ?= 0

BUILD_DIR := build
OBJ_DIR   := $(BUILD_DIR)/obj

SRCS := \
	src/core/globals.c          \
	src/main.c                  \
	src/client/client.c         \
	src/log/log.c               \
	src/config/config.c         \
	src/security/failban.c      \
	src/emu/emu.c               \
	src/srvid/srvid.c           \
	src/net/net.c               \
	src/cache/cw_cache.c        \
	src/platform/platform.c     \
	src/crypto/crypto.c         \
	src/crypto/sha1.c           \
	src/proto/newcamd.c         \
	src/proto/cccam.c           \
	webif/server.c              \
	webif/core.c                \
	webif/pages/login.c         \
	webif/pages/status.c        \
	webif/pages/users.c         \
	webif/pages/system.c        \
	webif/pages/config.c        \
	webif/pages/files.c         \
	webif/pages/power.c         \
	webif/pages/tvcas.c         \
	webif/api/status.c          \
	webif/api/users.c           \
	webif/api/config.c          \
	webif/api/system.c

obj_name = $(OBJ_DIR)/$(subst /,__,$(patsubst %.c,%.o,$(1)))
OBJS := $(foreach s,$(SRCS),$(call obj_name,$(s)))

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

BASE_FLAGS := -std=c11 -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wno-unused-parameter \
              -Wno-overlength-strings \
              -I. -D_FORTIFY_SOURCE=2

ifeq ($(RELEASE),1)
  CFLAGS += $(BASE_FLAGS) -Os \
            -ffunction-sections -fdata-sections \
            -fmerge-all-constants -fno-ident \
            -fstack-protector-strong \
            -flto \
            -march=x86-64 -mtune=generic
  ifeq ($(PLATFORM),linux)
    LDFLAGS += -flto -Wl,--gc-sections -Wl,--strip-all \
               -Wl,--build-id=none -Wl,--relax -Wl,-O1
  endif
  ifeq ($(PLATFORM),windows)
    LDFLAGS += -flto -Wl,--gc-sections -Wl,--strip-all \
               -Wl,--build-id=none -Wl,-O1
  endif
  ifeq ($(PLATFORM),windows-cross)
    LDFLAGS += -flto -Wl,--gc-sections -Wl,--strip-all \
               -Wl,--build-id=none -Wl,-O1
  endif
else
  ifeq ($(PLATFORM),windows)
    CFLAGS += $(BASE_FLAGS) -O2
  else ifeq ($(PLATFORM),windows-cross)
    CFLAGS += $(BASE_FLAGS) -O2
  else
    CFLAGS += $(BASE_FLAGS) -O2 -g
  endif
endif

.PHONY: all clean debug release assets

ASSETS_H := webif/assets/webif_assets.h

all: $(TARGET)

assets: $(ASSETS_H)

$(ASSETS_H): webif/assets/tcmg.css webif/assets/tcmg.js tools/gen_assets.py
	$(PYTHON) tools/gen_assets.py
	@echo "webif/assets/webif_assets.h regenerated"

$(OBJS): $(ASSETS_H)

$(TARGET): $(OBJS)
	@mkdir -p $(BUILD_DIR)
ifeq ($(RELEASE),1)
	$(STRIP) --strip-unneeded $(OBJS) 2>/dev/null || true
endif
	$(CC) $(OBJS) -o $@ $(LDFLAGS)
ifeq ($(RELEASE),1)
	$(STRIP) --strip-all $@ 2>/dev/null || true
endif
	@echo "Built: $@  (platform=$(PLATFORM))"

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
