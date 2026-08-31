BUILD_DIR := build
SOURCE_DIR := .
ELF := $(BUILD_DIR)/interop.elf
ROM := interop.z64
ODIN ?= ../../../odin

all: $(ROM)

ifndef N64_INST
$(error N64_INST must point to the installed libdragon toolchain)
endif

include $(N64_INST)/include/n64.mk

$(ELF): $(BUILD_DIR)/interop.o $(BUILD_DIR)/odin_side.o

$(ROM): N64_ROM_TITLE = "Odin O64 ABI"

$(BUILD_DIR)/odin_side.o: interop.odin $(ODIN)
	@mkdir -p $(dir $@)
	$(ODIN) build $< -file -target:freestanding_mips32be -build-mode:obj \
		-o:speed -no-entry-point -no-thread-local -default-to-nil-allocator \
		-reloc-mode:static -out:$@

check: CC = $(N64_CC)
check: CXX = $(N64_CXX)
check: CFLAGS += $(N64_CFLAGS)
check: CXXFLAGS += $(N64_CXXFLAGS)
check: LDFLAGS += $(N64_LDFLAGS)
check: $(ELF)
	python3 run.py $(ELF)

clean:
	rm -rf $(BUILD_DIR) *.z64

.PHONY: all check clean
