CC      ?= gcc
.DEFAULT_GOAL := all
STRIP   ?= strip
RELEASE ?= 0

BUILD_DIR := build
OBJ_DIR   := $(BUILD_DIR)/obj

# Single source of truth: every production C translation unit under src/ and webif/
# is compiled. Test programs live under tests/ and are linked separately below.
SRCS := $(shell find src webif -type f -name '*.c' -print | sort)

obj_name = $(OBJ_DIR)/$(patsubst %.c,%.o,$(1))
OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(SRCS))

UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)

# TCMG_TARGET_OS lets a cross build force the right branch below instead of
# relying on `uname` of the BUILD machine -- essential when cross-compiling
# for an embedded Linux box (Vu+, Dreambox, ...) from a Windows/MSYS2 host,
# where `uname` would otherwise report MINGW and wrongly pull in Windows
# flags/libs. Values: windows | linux | macos. Leave unset for native builds.
TCMG_TARGET_OS ?=

ifeq ($(TCMG_TARGET_OS),linux)
  PLATFORM  := linux
  TARGET    := $(BUILD_DIR)/tcmg
  LDFLAGS   += -lpthread -lm
else ifeq ($(TCMG_TARGET_OS),windows)
  PLATFORM  := windows
  TARGET    := $(BUILD_DIR)/tcmg_x64.exe
  CFLAGS    += -DTCMG_OS_WINDOWS -DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0601
  LDFLAGS   += -lws2_32 -ladvapi32 -lbcrypt -static -static-libgcc -lpthread
else ifeq ($(TCMG_TARGET_OS),macos)
  PLATFORM  := macos
  TARGET    := $(BUILD_DIR)/tcmg
  LDFLAGS   += -lpthread
else ifeq ($(findstring MINGW,$(UNAME_S)),MINGW)
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

# Optional: rename the output binary (useful when building several device
# profiles from the same tree so they don't overwrite one another).
ifneq ($(TCMG_TARGET_NAME),)
  TARGET := $(BUILD_DIR)/$(TCMG_TARGET_NAME)
endif

TCMG_ARCH_FLAGS ?=
TCMG_SANITIZE ?=
# Cross-toolchain linker extras: --sysroot=..., -static, a custom dynamic
# linker path, etc. Applied after every other LDFLAGS so it can override them.
TCMG_ARCH_LDFLAGS ?=
LDFLAGS += $(TCMG_ARCH_LDFLAGS)
# Keep sanitizer runtime flags on the final link as well as compilation.
LDFLAGS += $(TCMG_SANITIZE)

TCMG_PCSC ?= auto
# These are intentionally overrideable from build.sh. Cross builds prepare
# a target-side static libpcsclite and pass its include/lib directories here.
PCSC_CFLAGS ?=
PCSC_LIBS   ?=

ifeq ($(PLATFORM),linux)
  ifeq ($(TCMG_PCSC),auto)
    ifeq ($(strip $(PCSC_LIBS)),)
      PCSC_LIBS := $(shell pkg-config --libs libpcsclite 2>/dev/null)
    endif
    ifeq ($(strip $(PCSC_CFLAGS)),)
      PCSC_CFLAGS := $(shell pkg-config --cflags libpcsclite 2>/dev/null)
    endif
    ifeq ($(strip $(PCSC_LIBS)),)
      TCMG_PCSC := 0
    else
      TCMG_PCSC := 1
    endif
  endif
  ifeq ($(TCMG_PCSC),1)
    # Explicit native PC/SC builds use pkg-config when available and fall back
    # to the standard system linker name when it is not.
    ifeq ($(strip $(PCSC_LIBS)),)
      PCSC_LIBS := -lpcsclite
    endif
    CFLAGS += -DTCMG_PCSC=1 $(PCSC_CFLAGS)
    LDFLAGS += $(PCSC_LIBS)
  endif
else ifeq ($(PLATFORM),windows)
  ifeq ($(TCMG_PCSC),auto)
    TCMG_PCSC := 1
  endif
  ifeq ($(TCMG_PCSC),1)
    CFLAGS += -DTCMG_PCSC=1
    LDFLAGS += -lwinscard
  endif
else ifeq ($(PLATFORM),windows-cross)
  ifeq ($(TCMG_PCSC),auto)
    TCMG_PCSC := 1
  endif
  ifeq ($(TCMG_PCSC),1)
    CFLAGS += -DTCMG_PCSC=1
    LDFLAGS += -lwinscard
  endif
else ifeq ($(PLATFORM),macos)
  ifeq ($(TCMG_PCSC),auto)
    TCMG_PCSC := 1
  endif
  ifeq ($(TCMG_PCSC),1)
    CFLAGS += -DTCMG_PCSC=1
    LDFLAGS += -framework PCSC
  endif
endif

BASE_FLAGS := -std=c11 -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wno-unused-parameter \
              -Wno-overlength-strings \
              -I. -Isrc -D_FORTIFY_SOURCE=2 \
              $(TCMG_ARCH_FLAGS) \
              $(TCMG_SANITIZE)

TCMG_STRICT ?= 0
TCMG_CONF_DIR ?=

CFLAGS_EXTRA ?=
ifeq ($(TCMG_STRICT),1)
  BASE_FLAGS += -Werror -Wpedantic -Wshadow -Wstrict-prototypes -Wold-style-definition -Wredundant-decls
endif
BASE_FLAGS += $(CFLAGS_EXTRA)

ifneq ($(strip $(TCMG_CONF_DIR)),)
  BASE_FLAGS += -DCS_CONFDIR=\"$(TCMG_CONF_DIR)\"
endif

ifeq ($(RELEASE),1)
  CFLAGS += $(BASE_FLAGS) -Os \
            -ffunction-sections -fdata-sections \
            -fmerge-all-constants -fno-ident \
            -fstack-protector-strong \
            -flto
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

.PHONY: all clean debug release test-config test-network test-reader-registry test-proto-registry test-account-core test-session test-ecm-pipeline test-cache test-webif-service test-config-runtime-access test-account-state test-antishare check

# Browser assets (CSS / JS) are plain C headers in webif/assets/*.h -- edited by hand,
# no generator step.  Every object is rebuilt when one of them changes.
ASSET_HDRS := $(wildcard webif/assets/*.h)

TEST_COMMON_CFLAGS = -std=c11 -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror \
                    -Wno-unused-parameter -Wno-overlength-strings -I. -Isrc -D_FORTIFY_SOURCE=2 -O2 -g $(TCMG_SANITIZE)

test-config: $(TARGET)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(TEST_COMMON_CFLAGS) tests/config_smoke.c $(filter-out $(OBJ_DIR)/src/main.o,$(OBJS)) -o $(BUILD_DIR)/test_config_smoke $(LDFLAGS)
	$(BUILD_DIR)/test_config_smoke

test-network: $(TARGET)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(TEST_COMMON_CFLAGS) tests/reader_smoke.c $(filter-out $(OBJ_DIR)/src/main.o,$(OBJS)) -o $(BUILD_DIR)/test_reader_smoke $(LDFLAGS)
	TCMG_NETWORK_BUILD_DIR="$(abspath $(BUILD_DIR))" bash ./tests/network_matrix.sh

test-reader-registry: $(TARGET)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(TEST_COMMON_CFLAGS) tests/reader_registry_smoke.c $(filter-out $(OBJ_DIR)/src/main.o,$(OBJS)) -o $(BUILD_DIR)/test_reader_registry $(LDFLAGS)
	$(BUILD_DIR)/test_reader_registry

test-proto-registry: $(TARGET)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(TEST_COMMON_CFLAGS) tests/proto_registry_smoke.c $(filter-out $(OBJ_DIR)/src/main.o,$(OBJS)) -o $(BUILD_DIR)/test_proto_registry $(LDFLAGS)
	$(BUILD_DIR)/test_proto_registry

test-account-core: $(TARGET)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(TEST_COMMON_CFLAGS) tests/account_core_smoke.c $(filter-out $(OBJ_DIR)/src/main.o,$(OBJS)) -o $(BUILD_DIR)/test_account_core $(LDFLAGS)
	$(BUILD_DIR)/test_account_core

test-session: $(TARGET)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(TEST_COMMON_CFLAGS) tests/session_smoke.c $(filter-out $(OBJ_DIR)/src/main.o,$(OBJS)) -o $(BUILD_DIR)/test_session $(LDFLAGS)
	$(BUILD_DIR)/test_session

test-cache: $(TARGET)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(TEST_COMMON_CFLAGS) tests/cache_smoke.c $(filter-out $(OBJ_DIR)/src/main.o,$(OBJS)) -o $(BUILD_DIR)/test_cache -pthread $(LDFLAGS)
	$(BUILD_DIR)/test_cache

test-ecm-pipeline: $(TARGET)
	$(CC) $(TEST_COMMON_CFLAGS) tests/ecm_pipeline_smoke.c $(filter-out $(OBJ_DIR)/src/main.o,$(OBJS)) -o $(BUILD_DIR)/test_ecm_pipeline $(LDFLAGS)
	$(BUILD_DIR)/test_ecm_pipeline

test-webif-service: $(TARGET)
	$(CC) $(TEST_COMMON_CFLAGS) tests/webif_service_smoke.c $(filter-out $(OBJ_DIR)/src/main.o,$(OBJS)) -o $(BUILD_DIR)/test_webif_service $(LDFLAGS)
	$(BUILD_DIR)/test_webif_service

test-config-runtime-access: $(TARGET)
	$(CC) $(TEST_COMMON_CFLAGS) tests/config_runtime_access_smoke.c $(filter-out $(OBJ_DIR)/src/main.o,$(OBJS)) -o $(BUILD_DIR)/test_config_runtime_access $(LDFLAGS)
	$(BUILD_DIR)/test_config_runtime_access

test-account-state: $(TARGET)
	$(CC) $(TEST_COMMON_CFLAGS) tests/account_state_smoke.c $(filter-out $(OBJ_DIR)/src/main.o,$(OBJS)) -o $(BUILD_DIR)/test_account_state $(LDFLAGS)
	$(BUILD_DIR)/test_account_state

test-antishare: $(TARGET)
	$(CC) $(TEST_COMMON_CFLAGS) tests/antishare_smoke.c $(filter-out $(OBJ_DIR)/src/main.o,$(OBJS)) -o $(BUILD_DIR)/test_antishare $(LDFLAGS)
	$(BUILD_DIR)/test_antishare

test-internal: $(TARGET)
	$(CC) $(TEST_COMMON_CFLAGS) tests/internal_smoke.c $(filter-out $(OBJ_DIR)/src/main.o,$(OBJS)) -o $(BUILD_DIR)/test_internal $(LDFLAGS)
	$(BUILD_DIR)/test_internal

test-serial: $(TARGET)
	$(CC) $(TEST_COMMON_CFLAGS) tests/serial_smoke.c $(filter-out $(OBJ_DIR)/src/main.o,$(OBJS)) -o $(BUILD_DIR)/test_serial $(LDFLAGS)
	$(BUILD_DIR)/test_serial

test: test-config test-network test-reader-registry test-proto-registry test-account-core test-session test-ecm-pipeline test-cache test-webif-service test-config-runtime-access test-account-state test-antishare test-internal test-serial

check: $(ASSET_HDRS)
	@set -e; \
	bash -n build.sh; \
	missing=0; for f in $(SRCS); do test -f "$$f" || { echo "missing source: $$f" >&2; missing=1; }; done; \
	test "$$missing" -eq 0; \
	d=/tmp/tcmg-src-duplicates.$$; printf '%s\n' $(SRCS) | sort | uniq -d > "$$d"; \
	test ! -s "$$d"; rm -f "$$d"; \
	if grep -RInE '\b(g_cfg|g_clients|S_ACCOUNT|S_CLIENT|S_READER|g_ban_|g_active_conns)\b' webif/api webif/pages webif/core.c webif/server.c; then echo 'WEBIF direct state access detected' >&2; exit 1; fi; \
	if grep -nE '^#include "../core/config_state.h"|\bg_cfg\.(pcsc_)' src/pcsc/pcsc.c; then echo 'PCSC runtime config boundary violation detected' >&2; exit 1; fi; \
	if grep -nE '\bg_cfg\.(failban_)' src/security/failban.c; then echo 'Fail-Ban runtime config boundary violation detected' >&2; exit 1; fi; \
	if grep -RInE 'g_cfg\.(acc_lock|accounts|naccounts)' src/account src/client src/proto/cccam.c src/proto/camd35_server.c --exclude='account_state.c'; then echo 'ACCOUNT STATE boundary violation detected' >&2; exit 1; fi; \
	if grep -RInE '\bfetch\(' webif/pages webif/core.c | grep -v 'js_common.h'; then echo 'DIRECT FETCH IN PAGE DETECTED' >&2; exit 1; fi; \
	if grep -RInE '"../../src/(core/config_state|core/client_state|config/config|client/client|security/failban)' webif/api webif/pages webif/core.c webif/server.c; then echo 'WEBIF internal include detected' >&2; exit 1; fi; \
	if grep -RIn 'globals.h' src webif tests --include='*.c' --include='*.h' >/tmp/tcmg-globals.$$ 2>/dev/null && [ -s /tmp/tcmg-globals.$$ ]; then echo 'Umbrella globals.h include detected' >&2; rm -f /tmp/tcmg-globals.$$; exit 1; fi; rm -f /tmp/tcmg-globals.$$; \
	bash ./build.sh check >/dev/null; \
	bash ./build.sh self-test >/dev/null; \
	echo "CHECK: PASS"

all: $(TARGET)
$(OBJS): $(ASSET_HDRS)

$(TARGET): $(OBJS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)
ifeq ($(RELEASE),1)
	$(STRIP) --strip-all $@ 2>/dev/null || true
endif
	@echo "Built: $@  (platform=$(PLATFORM))"

# Portable pattern rule: preserve the source tree under $(OBJ_DIR).
# This avoids $(eval)/$(foreach) generated rules, which are fragile on older
# GNU make versions and were the source of "missing separator" failures.
$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

debug: CFLAGS  += -g -O0 -DDEBUG -fsanitize=address
debug: LDFLAGS += -fsanitize=address
debug: $(TARGET)

release:
	$(MAKE) RELEASE=1

clean:
	rm -rf $(BUILD_DIR)
