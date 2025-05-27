ifeq ($(LLVM),1)
	LLVM = 1
	CC = clang
	LD = ld.lld
else
	LLVM = 0
	CC = gcc
	LD = ld
endif

BUILD_DIR = build

CFLAGS = \
	-std=gnu23 \
	-Wall    \
	-Wextra  \
	-Werror

LDFLAGS = -pthread -lrt -lm

ifeq ($(DEBUG),1)
	CFLAGS += -O0 -g -fsanitize=address,leak,undefined
else
	CFLAGS  += -O2 -flto
	LDFLAGS += -flto
endif

ifeq ($(HARDENED),1)
	CFLAGS += \
		-pedantic
		-fwrapv \
		-fno-strict-aliasing \
		-fno-delete-null-pointer-checks \
		-D_FORTIFY_SOURCE=2 \
		-fstack-protector-strong \
		-fPIE -fPIC -fpic \
		-fno-builtin-fprintf -fno-builtin-fwprintf \
		-fno-builtin-printf -fno-builtin-snprintf \
		-fno-builtin-sprintf -fno-builtin-swprintf \
		-fno-builtin-wprintf \
		-fno-builtin-memcpy -fno-builtin-memmove \
		-fno-builtin-memset -fno-builtin-strcat \
		-fno-builtin-strcpy -fno-builtin-strncat \
		-fno-builtin-strncpy -fno-builtin-wcscat \
		-fno-builtin-scwcpy -fno-builtin-wcsncat \
		-fno-builtin-wcsncpy -fno-builtin-wmemcpy \
		-fno-builtin-wmemmove -fno-builtin-wmemset \
		-Warray-bounds \
		-Wdiv-by-zero \
		-Wshift-count-negative -Wshift-count-overflow \
		-fstack-protector
	ifeq ($(LLVM),0)
		CFLAGS += -fwrapv-pointer \
			-Wclobbered
	endif
endif

.PHONY: all clean controller worker

all: controller worker

$(BUILD_DIR):
	@mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)

controller: $(BUILD_DIR)/int_controller

worker: $(BUILD_DIR)/int_worker

HEADERS=$(shell find . -name '*.h' -print)
LIBSRC=$(wildcard lib/*.c)
LIBOBJ=$(patsubst %.c,$(BUILD_DIR)/%.o,$(LIBSRC))

$(BUILD_DIR)/int_controller: int_controller.c $(HEADERS) $(BUILD_DIR)/libdistcomp.a | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@ -ldistcomp -L$(BUILD_DIR)

$(BUILD_DIR)/int_worker: int_worker.c $(HEADERS) $(BUILD_DIR)/libdistcomp.a | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@ -ldistcomp -L$(BUILD_DIR)

$(BUILD_DIR)/libdistcomp.a: $(LIBOBJ) | $(BUILD_DIR)
	ar rcs $@ $^

$(BUILD_DIR)/lib/%.o: lib/%.c $(HEADERS) | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/lib
	$(CC) $(CFLAGS) $(LDFLAGS) -c $(filter %.c,$^) -o $@

