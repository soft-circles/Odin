/*
 * libdragon's rsp.ld for ld.lld, which the N64 build links RSP microcode with. It differs from rsp.ld in two places:
 * ld.lld rejects AT > after OVERLAY, so the overlay keeps only its runtime region (nothing extracts its load
 * addresses), and /DISCARD/ drops .reginfo and .got, which ld.lld would otherwise place in ram_data and grow DMEM.
 */

OUTPUT_FORMAT ("elf32-bigmips", "elf32-bigmips", "elf32-littlemips")
OUTPUT_ARCH (mips)
ENTRY (_start)

MEMORY
{
    rom_dmem : ORIGIN = 0x0000, LENGTH = 0x1000
    rom_imem : ORIGIN = 0x1000, LENGTH = 0x1000
    ram_data : ORIGIN = 0xA4000000, LENGTH = 0x1000
    ram_text : ORIGIN = 0xA4001000, LENGTH = 0x1000
}

SECTIONS
{
    .text : {
        _start = .;
        KEEP(*(.text))
        *(.text.*)
        _text_end = .;
    } > ram_text AT > rom_imem

    .data : {
        KEEP(*(.data))
        *(.data.*)
        _data_end = .;
    } > ram_data AT > rom_dmem

    . = ALIGN(8);

    OVERLAY : {
        .bss {
            KEEP(*(.bss))
            *(.bss.*)
        }
        .bssovl1 {
            KEEP(*(.bssovl1))
            *(.bssovl1.*)
        }
        .bssovl2 {
            KEEP(*(.bssovl2))
            *(.bssovl2.*)
        }
        .bssovl3 {
            KEEP(*(.bssovl3))
            *(.bssovl3.*)
        }
    } > ram_data

    .meta : {
        KEEP(*(.meta))
        *(.meta.*)
    }

    /DISCARD/ : { *(.MIPS.abiflags) *(.reginfo) *(.got) }
}
