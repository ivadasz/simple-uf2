/*

The MIT License (MIT)

Copyright (c) 2025 Imre Vadász

All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

// This tool can either be used with explicit offset, length, target-address
// options, or you can use the -s option to point it at an ELF section.
// The expectation is that the compiler output will all go into a single
// ELF section, since the Microcontroller target will usually have a single,
// contiguous FLASH or SRAM storage anyways, hence no support for separate
// .text and .data sections is implemented.

// The ELF section or explicit range from the input file will be converted
// to the Microsoft UF2 format, using a maximum 256 bytes payload.
// The payload will be less than 256 bytes when starting at an unaligned
// target-address, and when the remainder in the last block is < 256 bytes.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <fcntl.h>
#include <err.h>
#include <libelf.h>

#include "uf2.h"

void
usage(void)
{
	fprintf(stderr, "usage: mkuf2 -h -O <offset> -l <length> -a <address>"
	    " [-F <family_id>] -f <file> -o <output>\n");
	fprintf(stderr, "       mkuf2 -h -s <section>"
	    " [-F <family_id>] -f <file> -o <output>\n");
	exit(0);
}

int
main(int argc, char *argv[])
{
	uint32_t address, family = 0, length, offset = 0;
	int gotaddress = 0, gotlength = 0;
	char *input = NULL, *output = NULL;
	char *section = NULL;
	int ch, e;
	int infd, outfd;
	uint32_t pos;
	UF2_Block uf2data = {
		magicStart0: UF2_MAGIC_START0,
		magicStart1: UF2_MAGIC_START1,
		flags: UF2_FLAG_FAMILY_ID,
		blockNo: 0,
		magicEnd: UF2_MAGIC_END,
	};
	extern char *optarg;
	extern int optind;

	while ((ch = getopt(argc, argv, "a:f:hF:l:o:O:s:")) != -1) {
		switch (ch) {
		case 'a':
			address = (uint32_t)strtou(optarg, NULL, 0,
			    0, 0xFFFFFFFFU, &e);
			if (e) {
				errx(e, "conversion of %s to a number failed",
				    optarg);
			}
			gotaddress = 1;
			break;
		case 'f':
			input = optarg;
			break;
		case 'F':
			family = (uint32_t)strtou(optarg, NULL, 0,
			    0, 0xFFFFFFFFU, &e);
			if (e) {
				errx(e, "conversion of %s to a number failed",
				    optarg);
			}
			break;
		case 'h':
			usage();
		case 'l':
			length = (uint32_t)strtou(optarg, NULL, 0,
			    0, 0xFFFFFFFFU, &e);
			if (e) {
				errx(e, "conversion of %s to a number failed",
				    optarg);
			}
			gotlength = 1;
			break;
		case 'o':
			output = optarg;
			break;
		case 'O':
			offset = (uint32_t)strtou(optarg, NULL, 0,
			    0, 0xFFFFFFFFU, &e);
			if (e) {
				errx(e, "conversion of %s to a number failed",
				    optarg);
			}
			break;
		case 's':
			section = optarg;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (!input)
		errx(1, "No input file specified");
	if (!output)
		errx(1, "No output file specified");
	if (section != NULL && (gotaddress || gotlength || offset != 0))
		errx(1, "Only one of -s and -a or -l may be specified");
	if (section == NULL) {
		if (!gotaddress)
			errx(1, "No target address specified");
		if (!gotlength)
			errx(1, "No length specified");
	}

	infd = open(input, O_RDONLY);
	if (infd < 0)
		err(1, "%s", input);

	if (section != NULL) {
		size_t shstrndx;
		if (elf_version(EV_CURRENT) == EV_NONE) {
			errx(1, "ELF library initialization failed: %s",
			    elf_errmsg(-1));
		}
		Elf *e = elf_begin(infd, ELF_C_READ, NULL);
		if (e == NULL)
			errx(1, "elf_begin: %s", elf_errmsg(-1));
		if (elf_getshdrstrndx(e, &shstrndx) != 0)
			errx(1, "elf_getshdrstrndx: %s", elf_errmsg(-1));
		Elf_Scn *scn = NULL;
		char *name;
		while ((scn = elf_nextscn(e, scn)) != NULL) {
			Elf32_Shdr *shdr = elf32_getshdr(scn);
			if (shdr == NULL)
				errx(1, "elf_getshdr: %s", elf_errmsg(-1));
			name = elf_strptr(e, shstrndx, shdr->sh_name);
			if (name == NULL)
				errx(1, "elf_strptr: %s", elf_errmsg(-1));
			if (strcmp(name, section) == 0) {
				address = shdr->sh_addr;
				offset = shdr->sh_offset;
				length = shdr->sh_size;
				goto found;
				break;
			}
		}
		errx(1, "Couldn't find section %s in %s", section, input);
found:
		elf_end(e);
		if (address == 0) {
			errx(1, "Got unusable address 0 for section %s",
			    section);
		}
		if (offset == 0) {
			errx(1, "Got unusable offset 0 for section %s",
			    section);
		}
		if (length == 0) {
			errx(1, "Got unusable length 0 for section %s",
			    section);
		}
	}

	outfd = open(output, O_CREAT | O_WRONLY, 0600);
	if (outfd < 0)
		err(1, "%s", output);

	pos = 0;
	uf2data.numBlocks = (length - (address & 0xFF) + 0xFF) >> 8;
	uf2data.reserved = family;
	while (pos < length) {
		uint32_t amount = 256 - ((offset + pos) & 0xFF);
		uint32_t remaining = length - pos;
		ssize_t nr;
		if (remaining < amount)
			amount = remaining;
		uf2data.targetAddr = address + pos;
		uf2data.payloadSize = amount;
		nr = pread(infd, &uf2data.data[0], amount, offset + pos);
		if (nr == -1)
			err(1, "pread");
		if (nr == 0)
			errx(1, "unexpected EOF");
		if (nr != amount)
			errx(1, "unexpected short read: %u", nr);
		memset(&uf2data.data[nr], 0, 256 - nr);
		nr = write(outfd, &uf2data, sizeof(uf2data));
		if (nr == -1)
			err(1, "write");
		if (nr != sizeof(uf2data))
			errx(1, "wrong length for write: %u", nr);
		pos += amount;
		uf2data.blockNo++;
	}

	close(outfd);
	close(infd);
	printf("Wrote %u blocks\n", uf2data.numBlocks);

	exit(0);
}
