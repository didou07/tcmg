CC      ?= gcc
.DEFAULT_GOAL := all
STRIP   ?= strip
RELEASE ?= 0
VERSION := $(strip $(shell cat VERSION 2>/dev/null))
ifeq ($(VERSION),)
$(error VERSION file missing or empty)
endif

BUILD_DIR := build
OBJ_DIR   := $(BUILD_DIR)/obj

SRCS := $(shell find src webif -type f -name '*.c' -print | sort)

obj_name = $(OBJ_DIR)/$(patsubst %.c,%.o,$(1))
OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(SRCS))

UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)

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

ifneq ($(TCMG_TARGET_NAME),)
  TARGET := $(BUILD_DIR)/$(TCMG_TARGET_NAME)
endif

TCMG_ARCH_FLAGS ?=
TCMG_SANITIZE ?=
TCMG_ASSET_REV ?= $(shell date +%Y%m%d%H%M%S)
TCMG_ARCH_LDFLAGS ?=
LDFLAGS += $(TCMG_ARCH_LDFLAGS)
LDFLAGS += $(TCMG_SANITIZE)

TCMG_PCSC ?= auto
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

BASE_FLAGS := -std=c11 -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L -Wall -Wextra \
              -I. -Isrc -D_FORTIFY_SOURCE=2 -DTCMG_ASSET_REV=\"$(TCMG_ASSET_REV)\" \
              $(TCMG_ARCH_FLAGS) \
              $(TCMG_SANITIZE) \
              -DTCMG_VERSION=\"$(VERSION)\"

TCMG_STRICT ?= 0
TCMG_CONF_DIR ?=

CFLAGS_EXTRA ?=
ifeq ($(TCMG_STRICT),1)
  BASE_FLAGS += -Werror -Wpedantic -Wshadow -Wstrict-prototypes -Wold-style-definition -Wredundant-decls -Wconversion -Wsign-conversion -Wformat=2
endif
BASE_FLAGS += $(CFLAGS_EXTRA)

ifneq ($(strip $(TCMG_CONF_DIR)),)
  BASE_FLAGS += -DCS_CONFDIR=\"$(TCMG_CONF_DIR)\"
endif

ifeq ($(RELEASE),1)
  CFLAGS += $(BASE_FLAGS) -O2 \
            -ffunction-sections -fdata-sections \
            -fmerge-all-constants -fno-ident \
            -fstack-protector-strong \
            -fno-unwind-tables -fno-asynchronous-unwind-tables \
            -flto
  ifeq ($(PLATFORM),linux)
    LDFLAGS += -flto -Wl,--gc-sections -Wl,--strip-all \
               -Wl,--build-id=none -Wl,--relax -Wl,-O1 -Wl,--hash-style=gnu
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

.PHONY: all clean debug release test-config test-network test-reader-rules test-reader-registry test-proto-registry test-account-core test-session test-ecm-pipeline test-cache test-webif-service test-webif-many-clients test-webif-concurrency test-config-runtime-access test-account-state test-antishare test-internal test-internal-t0 test-internal-ui test-serial test-log check

ASSET_HDRS := $(wildcard webif/assets/*.h)

TEST_COMMON_CFLAGS = -std=c11 -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -Wpedantic -Wshadow -Wstrict-prototypes -Wold-style-definition -Wredundant-decls -Wconversion -Wsign-conversion -Wformat=2 \
                    -I. -Isrc -D_FORTIFY_SOURCE=2 -O2 -g $(TCMG_SANITIZE)

test-log: $(TARGET)
	$(CC) $(TEST_COMMON_CFLAGS) tests/log_smoke.c $(filter-out $(OBJ_DIR)/src/main.o,$(OBJS)) -o $(BUILD_DIR)/test_log_smoke $(LDFLAGS)
	$(BUILD_DIR)/test_log_smoke

test-config: $(TARGET)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(TEST_COMMON_CFLAGS) tests/config_smoke.c $(filter-out $(OBJ_DIR)/src/main.o,$(OBJS)) -o $(BUILD_DIR)/test_config_smoke $(LDFLAGS)
	$(BUILD_DIR)/test_config_smoke

test-network: $(TARGET)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(TEST_COMMON_CFLAGS) tests/reader_smoke.c $(filter-out $(OBJ_DIR)/src/main.o,$(OBJS)) -o $(BUILD_DIR)/test_reader_smoke $(LDFLAGS)
	TCMG_NETWORK_BUILD_DIR="$(abspath $(BUILD_DIR))" bash ./tests/network_matrix.sh

test-reader-rules: $(TARGET)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(TEST_COMMON_CFLAGS) tests/reader_rules_smoke.c $(filter-out $(OBJ_DIR)/src/main.o,$(OBJS)) -o $(BUILD_DIR)/test_reader_rules $(LDFLAGS)
	$(BUILD_DIR)/test_reader_rules

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

test-webif-many-clients: $(TARGET)
	$(CC) $(TEST_COMMON_CFLAGS) tests/webif_many_clients_smoke.c $(filter-out $(OBJ_DIR)/src/main.o,$(OBJS)) -o $(BUILD_DIR)/test_webif_many_clients $(LDFLAGS)
	$(BUILD_DIR)/test_webif_many_clients

test-webif-concurrency: $(TARGET)
	TCMG_WEBIF_BIN="$(abspath $(TARGET))" python3 tests/webif/concurrency_503.py

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

test-internal-t0: $(TARGET)
	$(CC) $(TEST_COMMON_CFLAGS) tests/internal_t0_smoke.c $(OBJ_DIR)/src/internal/internal_t0.o -o $(BUILD_DIR)/test_internal_t0 $(LDFLAGS)
	$(BUILD_DIR)/test_internal_t0

test-internal-ui: $(TARGET)
	bash tests/internal_ui_smoke.sh

test-serial: $(TARGET)
	$(CC) $(TEST_COMMON_CFLAGS) tests/serial_smoke.c $(filter-out $(OBJ_DIR)/src/main.o,$(OBJS)) -o $(BUILD_DIR)/test_serial $(LDFLAGS)
	$(BUILD_DIR)/test_serial

test: test-config test-network test-reader-rules test-reader-registry test-proto-registry test-account-core test-session test-ecm-pipeline test-cache test-webif-service test-webif-many-clients test-webif-concurrency test-config-runtime-access test-account-state test-antishare test-internal test-internal-t0 test-internal-ui test-serial test-log

check: $(ASSET_HDRS)
	@set -e; \
	bash -n build.sh; \
	missing=0; for f in $(SRCS); do test -f "$$f" || { echo "missing source: $$f" >&2; missing=1; }; done; \
	test "$$missing" -eq 0; \
	d=/tmp/tcmg-src-duplicates.$$; printf '%s\n' $(SRCS) | sort | uniq -d > "$$d"; \
	test ! -s "$$d"; rm -f "$$d"; \
	if grep -RInE '\b(g_cfg|g_clients|S_ACCOUNT|S_CLIENT|S_READER|g_ban_|g_active_conns)\b' webif/api webif/pages webif/core.c webif/server.c; then echo 'WEBIF direct state access detected' >&2; exit 1; fi; \
	if grep -RInE 'AUTH (blocked|failed|rejected):[^\n]*reason=' src/proto webif/server.c --include='*.c'; then echo 'AUTH reason= label still present' >&2; exit 1; fi; \
	if grep -nE '^#include "../core/config_state.h"|\bg_cfg\.(pcsc_)' src/pcsc/pcsc.c; then echo 'PCSC runtime config boundary violation detected' >&2; exit 1; fi; \
	if grep -nE '\bg_cfg\.(failban_)' src/security/failban.c; then echo 'Fail-Ban runtime config boundary violation detected' >&2; exit 1; fi; \
	if grep -RInE 'g_cfg\.(acc_lock|accounts|naccounts)' src/account src/client src/proto/cccam.c src/proto/camd35_server.c --exclude='account_state.c'; then echo 'ACCOUNT STATE boundary violation detected' >&2; exit 1; fi; \
	if grep -RInE '\bfetch\(' webif/pages webif/core.c | grep -v 'js_common.h'; then echo 'DIRECT FETCH IN PAGE DETECTED' >&2; exit 1; fi; \
	if grep -RInE '"../../src/(core/config_state|core/client_state|config/config|client/client|security/failban)' webif/api webif/pages webif/core.c webif/server.c; then echo 'WEBIF internal include detected' >&2; exit 1; fi; \
	if grep -nE '@media[^\n]*(max-width|min-width)' webif/assets/css.h; then echo 'WIDTH-BASED RESPONSIVE MEDIA QUERY DETECTED' >&2; exit 1; fi; \
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
