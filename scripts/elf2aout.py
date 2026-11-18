#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0
"""Convert a static x86-64 ELF executable into a Plan 9 amd64 a.out.

usage: elf2aout.py <in.elf> <out.aout>

The ELF must have been linked with scripts/plan9-aout.ld: text (with
rodata) starting at 0x200028 and data at the next 2 MB boundary, which
is where the Plan 9 kernel puts them.  Used to build test programs
without a Plan 9 tool chain.
"""
import struct
import sys

UTZERO = 0x200000
HDRSZ = 40
S_MAGIC = 0x8a97


def main():
    elf = open(sys.argv[1], "rb").read()
    assert elf[:4] == b"\x7fELF" and elf[4] == 2, "need ELF64"
    (e_entry, e_phoff, e_shoff) = struct.unpack_from("<QQQ", elf, 24)
    (e_phentsize, e_phnum) = struct.unpack_from("<HH", elf, 54)
    segs = []
    for i in range(e_phnum):
        p_type, p_flags, p_off, p_vaddr, _, p_filesz, p_memsz, _ = \
            struct.unpack_from("<IIQQQQQQ", elf, e_phoff + i * e_phentsize)
        if p_type == 1:  # PT_LOAD
            segs.append((p_vaddr, p_off, p_filesz, p_memsz, p_flags))
    segs.sort()
    text = segs[0]
    assert text[0] == UTZERO + HDRSZ, "text must start at 0x200028"
    textbytes = elf[text[1]:text[1] + text[2]]
    textend = UTZERO + HDRSZ + len(textbytes)
    adata = (textend + 0x1fffff) & ~0x1fffff
    data, bss = b"", 0
    if len(segs) > 1:
        d = segs[1]
        assert d[0] == adata, "data at %#x, expected %#x" % (d[0], adata)
        data = elf[d[1]:d[1] + d[2]]
        bss = d[3] - d[2]
    hdr = struct.pack(">IIIIIIII", S_MAGIC, len(textbytes), len(data), bss,
                      0, e_entry & 0xffffffff, 0, 0)
    hdr += struct.pack(">Q", e_entry)
    with open(sys.argv[2], "wb") as f:
        f.write(hdr + textbytes + data)


if __name__ == "__main__":
    main()
