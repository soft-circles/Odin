BUILD_DIR := build
SOURCE_DIR := .
ELF := $(BUILD_DIR)/interop.elf
ODIN ?= ../../../odin

all: $(ELF)

ifndef N64_INST
$(error N64_INST must point to the installed libdragon toolchain)
endif

include $(N64_INST)/include/n64.mk

$(ELF): CC = $(N64_CC)
$(ELF): CXX = $(N64_CXX)
$(ELF): CFLAGS += $(N64_CFLAGS)
$(ELF): CXXFLAGS += $(N64_CXXFLAGS)
$(ELF): LDFLAGS += $(N64_LDFLAGS)
$(ELF): $(BUILD_DIR)/interop.o $(BUILD_DIR)/odin_side.o

$(BUILD_DIR)/odin_side.o: interop.odin $(ODIN)
	@mkdir -p $(dir $@)
	$(ODIN) build $< -file -target:freestanding_mips32be -build-mode:obj \
		-o:speed -no-entry-point -no-thread-local -default-to-nil-allocator \
		-reloc-mode:static -out:$@

check: $(ELF)
	python3 run.py $(ELF)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all check clean
