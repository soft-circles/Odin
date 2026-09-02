BUILD_DIR := build
SOURCE_DIR := .
ELF := $(BUILD_DIR)/libdragon_bindings.elf
ROM := libdragon_bindings.z64
ODIN ?= ../../../odin
SDK_CHECK := ../validate_sdk.py

all: $(ROM)

ifndef N64_INST
$(error N64_INST must point to the installed libdragon toolchain)
endif

include $(N64_INST)/include/n64.mk

validate-sdk:
	python3 $(SDK_CHECK) "$(N64_INST)"

check-layout: CC = $(N64_CC)
check-layout: CXX = $(N64_CXX)
check-layout: CFLAGS += $(N64_CFLAGS)
check-layout: CXXFLAGS += $(N64_CXXFLAGS)
check-layout: $(BUILD_DIR)/probe.o $(BUILD_DIR)/odin_side.o

$(ELF): $(BUILD_DIR)/probe.o $(BUILD_DIR)/odin_side.o

$(BUILD_DIR)/probe.o $(BUILD_DIR)/odin_side.o: | validate-sdk

$(ROM): N64_ROM_TITLE = "Odin libdragon ABI"

$(BUILD_DIR)/odin_side.o: probe.odin $(ODIN)
	@mkdir -p $(dir $@)
	$(ODIN) build $< -file -target:freestanding_mips32be -build-mode:obj \
		-o:speed -no-entry-point -no-thread-local -default-to-nil-allocator \
		-reloc-mode:static -out:$@

check: $(ROM)
	python3 run.py $(ROM)

clean:
	rm -rf $(BUILD_DIR) *.z64

.PHONY: all check check-layout clean validate-sdk
