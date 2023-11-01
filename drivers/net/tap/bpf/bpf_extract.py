#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2023 Stephen Hemminger <stephen@networkplumber.org>

import argparse
import sys
import struct
from tempfile import TemporaryFile
from elftools.elf.elffile import ELFFile


def load_sections(elffile):
    """Get sections of interest from ELF"""
    result = []
    parts = [("cls_q", "cls_q_insns"), ("l3_l4", "l3_l4_hash_insns")]
    for name, tag in parts:
        section = elffile.get_section_by_name(name)
        if section:
            insns = struct.iter_unpack('<BBhL', section.data())
            result.append([tag, insns])
    return result


def dump_section(name, insns, out):
    """Dump the array of BPF instructions"""
    print(f'\nstatic struct bpf_insn {name}[] = {{', file=out)
    for bpf in insns:
        code = bpf[0]
        src = bpf[1] >> 4
        dst = bpf[1] & 0xf
        off = bpf[2]
        imm = bpf[3]
        print(f'\t{{{code:#04x}, {dst:4d}, {src:4d}, {off:8d}, {imm:#010x}}},',
              file=out)
    print('};', file=out)


def parse_args():
    """Parse command line arguments"""
    parser = argparse.ArgumentParser()
    parser.add_argument('-s',
                        '--source',
                        type=str,
                        help="original source file")
    parser.add_argument('-o', '--out', type=str, help="output C file path")
    parser.add_argument("file",
                        nargs='+',
                        help="object file path or '-' for stdin")
    return parser.parse_args()


def open_input(path):
    """Open the file or stdin"""
    if path == "-":
        temp = TemporaryFile()
        temp.write(sys.stdin.buffer.read())
        return temp
    return open(path, 'rb')


def write_header(out, source):
    """Write file intro header"""
    print("/* SPDX-License-Identifier: BSD-3-Clause", file=out)
    if source:
        print(f' * Auto-generated from {source}', file=out)
    print(" * This not the original source file. Do NOT edit it.", file=out)
    print(" */\n", file=out)
    print("#include <tap_bpf.h>", file=out)


def main():
    '''program main function'''
    args = parse_args()

    with open(args.out, 'w',
              encoding="utf-8") if args.out else sys.stdout as out:
        write_header(out, args.source)
        for path in args.file:
            elffile = ELFFile(open_input(path))
            sections = load_sections(elffile)
            for name, insns in sections:
                dump_section(name, insns, out)


if __name__ == "__main__":
    main()
