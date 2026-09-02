BUILD_DIR := build
ELF := $(BUILD_DIR)/n64_console.elf
ROM := n64_console.z64
ODIN ?= ../../odin
ARES_TEST ?= ares-test
SDK_VALIDATOR := ../o64_abi/validate_sdk.py
LIBDRAGON_BINDING := $(wildcard ../../vendor/libdragon/*.odin)

all: $(ROM)

ifndef N64_INST
$(error N64_INST must point to the installed libdragon toolchain)
endif

include $(N64_INST)/include/n64.mk

validate-sdk:
	python3 $(SDK_VALIDATOR) "$(N64_INST)"

$(ELF): $(BUILD_DIR)/entry.o $(BUILD_DIR)/console.o

$(BUILD_DIR)/entry.o $(BUILD_DIR)/console.o: | validate-sdk

$(ROM): N64_ROM_TITLE = "Odin N64 Console"

$(BUILD_DIR)/console.o: console.odin $(LIBDRAGON_BINDING) $(ODIN)
	@mkdir -p $(dir $@)
	$(ODIN) build $< -file -target:freestanding_mips32be -build-mode:obj \
		-o:speed -no-entry-point -no-thread-local \
		-default-to-nil-allocator -reloc-mode:static -out:$@

check: $(ROM)
	$(ARES_TEST) console.test.js $(ROM) --timeout 30

clean:
	rm -rf $(BUILD_DIR) *.z64

.PHONY: all check clean validate-sdk
