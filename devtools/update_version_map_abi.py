#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Intel Corporation

"""
A Python program that updates and merges all available stable ABI versions into
one ABI version, while leaving experimental ABI exactly as it is. The intended
ABI version is supplied via command-line parameter. This script is to be called
from the devtools/update-abi.sh utility.
"""

import argparse
import sys
import re


def __parse_map_file(f_in):
    # match function name, followed by semicolon, followed by EOL or comments,
    # optionally with whitespace in between each item
    func_line_regex = re.compile(r"\s*"
                                 r"(?P<line>"
                                 r"(?P<func>[a-zA-Z_0-9]+)"
                                 r"\s*"
                                 r";"
                                 r"\s*"
                                 r"(?P<comment>#.+)?"
                                 r")"
                                 r"\s*"
                                 r"$")
    # match section name, followed by opening bracked, followed by EOL,
    # optionally with whitespace in between each item
    section_begin_regex = re.compile(r"\s*"
                                     r"(?P<version>[a-zA-Z0-9_\.]+)"
                                     r"\s*"
                                     r"{"
                                     r"\s*"
                                     r"$")
    # match closing bracket, optionally followed by section name (for when we
    # inherit from another ABI version), followed by semicolon, followed by
    # EOL, optionally with whitespace in between each item
    section_end_regex = re.compile(r"\s*"
                                   r"}"
                                   r"\s*"
                                   r"(?P<parent>[a-zA-Z0-9_\.]+)?"
                                   r"\s*"
                                   r";"
                                   r"\s*"
                                   r"$")

    # for stable ABI, we don't care about which version introduced which
    # function, we just flatten the list. there are dupes in certain files, so
    # use a set instead of a list
    stable_lines = set()
    # copy experimental section as is
    experimental_lines = []
    # copy internal section as is
    internal_lines = []
    in_experimental = False
    in_internal = False
    has_stable = False

    # gather all functions
    for line in f_in:
        # clean up the line
        line = line.strip('\n').strip()

        # is this an end of section?
        match = section_end_regex.match(line)
        if match:
            # whatever section this was, it's not active any more
            in_experimental = False
            in_internal = False
            continue

        # if we're in the middle of experimental section, we need to copy
        # the section verbatim, so just add the line
        if in_experimental:
            experimental_lines += [line]
            continue

        # if we're in the middle of internal section, we need to copy
        # the section verbatim, so just add the line
        if in_internal:
            internal_lines += [line]
            continue

        # skip empty lines
        if not line:
            continue

        # is this a beginning of a new section?
        match = section_begin_regex.match(line)
        if match:
            cur_section = match.group("version")
            # is it experimental?
            in_experimental = cur_section == "EXPERIMENTAL"
            # is it internal?
            in_internal = cur_section == "INTERNAL"
            if not in_experimental and not in_internal:
                has_stable = True
            continue

        # is this a function?
        match = func_line_regex.match(line)
        if match:
            stable_lines.add(match.group("line"))

    return has_stable, stable_lines, experimental_lines, internal_lines


def __generate_stable_abi(f_out, abi_major, lines):
    # print ABI version header
    print("DPDK_{} {{".format(abi_major), file=f_out)

    # print global section if it exists
    if lines:
        print("\tglobal:", file=f_out)
        # blank line
        print(file=f_out)

        # print all stable lines, alphabetically sorted
        for line in sorted(lines):
            print("\t{}".format(line), file=f_out)

        # another blank line
        print(file=f_out)

    # print local section
    print("\tlocal: *;", file=f_out)

    # end stable version
    print("};", file=f_out)


def __generate_experimental_abi(f_out, lines):
    # start experimental section
    print("EXPERIMENTAL {", file=f_out)

    # print all experimental lines as they were
    for line in lines:
        # don't print empty whitespace
        if not line:
            print("", file=f_out)
        else:
            print("\t{}".format(line), file=f_out)

    # end section
    print("};", file=f_out)

def __generate_internal_abi(f_out, lines):
    # start internal section
    print("INTERNAL {", file=f_out)

    # print all internal lines as they were
    for line in lines:
        # don't print empty whitespace
        if not line:
            print("", file=f_out)
        else:
            print("\t{}".format(line), file=f_out)

    # end section
    print("};", file=f_out)

def __main():
    arg_parser = argparse.ArgumentParser(
        description='Merge versions in linker version script.')

    arg_parser.add_argument("map_file", type=str,
                            help='path to linker version script file '
                                 '(pattern: version.map)')
    arg_parser.add_argument("abi_version", type=str,
                            help='target ABI version (pattern: MAJOR.MINOR)')

    parsed = arg_parser.parse_args()

    if not parsed.map_file.endswith('version.map'):
        print("Invalid input file: {}".format(parsed.map_file),
              file=sys.stderr)
        arg_parser.print_help()
        sys.exit(1)

    if not re.match(r"\d{1,2}\.\d{1,2}", parsed.abi_version):
        print("Invalid ABI version: {}".format(parsed.abi_version),
              file=sys.stderr)
        arg_parser.print_help()
        sys.exit(1)
    abi_major = parsed.abi_version.split('.')[0]

    with open(parsed.map_file) as f_in:
        has_stable, stable_lines, experimental_lines, internal_lines = __parse_map_file(f_in)

    with open(parsed.map_file, 'w') as f_out:
        need_newline = has_stable and experimental_lines
        if has_stable:
            __generate_stable_abi(f_out, abi_major, stable_lines)
        if need_newline:
            # separate sections with a newline
            print(file=f_out)
        if experimental_lines:
            __generate_experimental_abi(f_out, experimental_lines)
        if internal_lines:
            if has_stable or experimental_lines:
              # separate sections with a newline
              print(file=f_out)
            __generate_internal_abi(f_out, internal_lines)


if __name__ == "__main__":
    __main()
