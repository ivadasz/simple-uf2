# simple-uf2
A basic C utility to create a UF2 flashing payload from an ELF binary.
Mainly focused on supporting Raspberry Pi Pico RP2350 for now, but should
support arbitrary targets.

## Flags

* `-a` Target address to write on device
* `-F` Family
* `-f` Source file
* `-l` Length to read from source file
* `-O` Offset to read from source file
* `-o` Output file
* `-s` Retrieve `Target address`, `Length` and `Offset` parameters from an ELF section header.

The flags `-O`, `-l` and `-a` can't be used together with `-s`.

## Example Usage

### Create UF2 for RP2350 in ARM mode using ELF section:

This assumes that you used a custom linker script to write your full binary
into a single ELF section. Adding support for specifying multiple ELF sections
should be easy to add. Also, this assumes that your .text section already
specifies the correct target address for your binary. Additionally, this
assumes that your binary's .text section contains the correct header for
booting your code on the Microcontroller,
```
./mkuf2 -s .text -F 0xE48BFF59 -f mybinary -o mybinary.uf2
```

An example linker script to write your .text section with target address 0x20000000 could be:
```
ENTRY(_start)

SECTIONS
{
  .text (0x20000000) : {
    rp2350_arm_header.o(.text)
    *(.text)
    *(.data)
  }
}
```

### Create UF2 for RP2350 in ARM mode, extracting explicitly from a binary:

As an alternative, you can explicitly state the offset, length and target
address for your code (`-O`, `-l`, and `-a` flags respectively), instead of
specifying the ELF section. For example this copies 0xE7 bytes from offset 0x10000, which will be written to the target address 0x20000000 on your device:
```
./mkuf2 -O 0x10000 -l 0xE7 -a 0x20000000 -F 0xE48BFF59 -f mybinary -o mybinary.uf2
```

