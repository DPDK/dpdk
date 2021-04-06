#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Intel Corporation

import sys


def is_function_line(ln):
    return ln.startswith('\t') and ln.endswith(';\n') and ":" not in ln and "# WINDOWS_NO_EXPORT" not in ln

# MinGW keeps the original .map file but replaces per_lcore* to __emutls_v.per_lcore*
def create_mingw_map_file(input_map, output_map):
    with open(input_map) as f_in, open(output_map, 'w') as f_out:
        f_out.writelines([lines.replace('per_lcore', '__emutls_v.per_lcore') for lines in f_in.readlines()])

def main(args):
    if not args[1].endswith('version.map') or \
            not args[2].endswith('exports.def') and \
            not args[2].endswith('mingw.map'):
        return 1

    if args[2].endswith('mingw.map'):
        create_mingw_map_file(args[1], args[2])
        return 0

# generate def file from map file.
# This works taking indented lines only which end with a ";" and which don't
# have a colon in them, i.e. the lines defining functions only.
    else:
        with open(args[1]) as f_in:
            functions = [ln[:-2] + '\n' for ln in sorted(f_in.readlines())
                         if is_function_line(ln)]
            functions = ["EXPORTS\n"] + functions

    with open(args[2], 'w') as f_out:
        f_out.writelines(functions)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
