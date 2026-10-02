# mmi_engine
#   make              optimised build (ARCH=native by default)
#   make debug        AddressSanitizer + UndefinedBehaviorSanitizer build
#   make tsan         ThreadSanitizer build
#   make test         perft suite, UCI smoke test and bench determinism
#   make format       format sources with clang-format
#   make format-check fail if any source is not formatted

CC ?= cc
CLANG_FORMAT ?= clang-format
EXE = mmi_engine
SRCS := $(wildcard src/*.c src/*/*.c)
HDRS := $(wildcard src/*.h src/*/*.h)
GIT_HASH := $(shell git rev-parse --short HEAD 2>/dev/null || echo unknown)

ARCH ?= native
ifeq ($(ARCH),native)
  ifneq ($(filter arm64 aarch64,$(shell uname -m)),)
    ARCH_FLAGS ?= -mcpu=native
  else
    ARCH_FLAGS ?= -march=native
  endif
else
  ARCH_FLAGS ?= -march=$(ARCH)
endif

ifeq ($(OS),Windows_NT)
  LIBS =
else
  LIBS = -lpthread
endif

WARN = -Wall -Wextra -Wshadow -Wpedantic -Werror
CFLAGS_COMMON = -std=c17 $(WARN) -Isrc -DMMI_GIT_HASH='"$(GIT_HASH)"'
CFLAGS_RELEASE = $(CFLAGS_COMMON) -O3 -DNDEBUG -flto $(ARCH_FLAGS)
CFLAGS_DEBUG = $(CFLAGS_COMMON) -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=all
CFLAGS_TSAN = $(CFLAGS_COMMON) -O1 -g -fsanitize=thread

.PHONY: all debug tsan test format format-check clean

all: $(EXE)

$(EXE): $(SRCS) $(HDRS)
	$(CC) $(CFLAGS_RELEASE) $(SRCS) -o $@ $(LIBS)

debug: $(SRCS) $(HDRS)
	$(CC) $(CFLAGS_DEBUG) $(SRCS) -o $(EXE)-debug $(LIBS)

tsan: $(SRCS) $(HDRS)
	$(CC) $(CFLAGS_TSAN) $(SRCS) -o $(EXE)-tsan $(LIBS)

test: $(EXE)
	sh tests/run_tests.sh ./$(EXE)

format:
	$(CLANG_FORMAT) -i $(SRCS) $(HDRS)

format-check:
	$(CLANG_FORMAT) --dry-run --Werror $(SRCS) $(HDRS)

clean:
	rm -rf $(EXE) $(EXE)-debug $(EXE)-tsan $(EXE).exe *.dSYM
