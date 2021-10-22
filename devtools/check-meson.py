#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2021 Intel Corporation

'''
A Python script to run some checks on meson.build files in DPDK
'''

import sys
import os
import re
from os.path import relpath, join
from argparse import ArgumentParser

VERBOSE = False


def scan_dir(path):
    '''return meson.build files found in path'''
    for root, dirs, files in os.walk(path):
        if 'meson.build' in files:
            yield(relpath(join(root, 'meson.build')))


def split_code_comments(line):
    'splits a line into a code part and a comment part, returns (code, comment) tuple'
    if line.lstrip().startswith('#'):
        return ('', line)
    elif '#' in line and '#include' not in line:  # catch 99% of cases, not 100%
        idx = line.index('#')
        while (line[idx - 1].isspace()):
            idx -= 1
        return line[:idx], line[idx:]
    else:
        return (line, '')


def setline(contents, index, value):
    'sets the contents[index] to value. Returns the line, along with code and comments part'
    line = contents[index] = value
    code, comments = split_code_comments(line)
    return line, code, comments


def check_indentation(filename, contents):
    '''check that a list or files() is correctly indented'''
    infiles = False
    inlist = False
    edit_count = 0
    for lineno, line in enumerate(contents):
        code, comments = split_code_comments(line)
        if not code.strip():
            continue
        if re.match('^ *\t', code):
            print(f'Error parsing {filename}:{lineno}, got some tabulation')
        if code.endswith('files('):
            if infiles:
                raise(f'Error parsing {filename}:{lineno}, got "files(" when already parsing files list')
            if inlist:
                print(f'Error parsing {filename}:{lineno}, got "files(" when already parsing array list')
            infiles = True
            indent_count = len(code) - len(code.lstrip(' '))
            indent = ' ' * (indent_count + 8)  # double indent required
        elif code.endswith('= ['):
            if infiles:
                raise(f'Error parsing {filename}:{lineno}, got start of array when already parsing files list')
            if inlist:
                print(f'Error parsing {filename}:{lineno}, got start of array when already parsing array list')
            inlist = True
            indent_count = len(code) - len(code.lstrip(' '))
            indent = ' ' * (indent_count + 8)  # double indent required
        elif infiles and (code.endswith(')') or code.strip().startswith(')')):
            infiles = False
            continue
        elif inlist and (code.endswith(']') or code.strip().startswith(']')):
            inlist = False
            continue
        elif inlist or infiles:
            # skip further subarrays or lists
            if '[' in code or ']' in code:
                continue
            if not code.startswith(indent) or code[len(indent)] == ' ':
                print(f'Error: Incorrect indent at {filename}:{lineno + 1}')
                line, code, comments = setline(contents, lineno, indent + line.strip())
                edit_count += 1
            if not code.endswith(','):
                print(f'Error: Missing trailing "," in list at {filename}:{lineno + 1}')
                line, code, comments = setline(contents, lineno, code + ',' + comments)
                edit_count += 1
            if len(code.split(',')) > 2:  # only one comma per line
                print(f'Error: multiple entries per line in list at {filename}:{lineno +1}')
                entries = [e.strip() for e in code.split(',') if e.strip()]
                line, code, comments = setline(contents, lineno,
                                               indent + (',\n' + indent).join(entries) +
                                               ',' + comments)
                edit_count += 1
    return edit_count


def process_file(filename, fix):
    '''run checks on file "filename"'''
    if VERBOSE:
        print(f'Processing {filename}')
    with open(filename) as f:
        contents = [ln.rstrip() for ln in f.readlines()]

    if check_indentation(filename, contents) > 0 and fix:
        print(f"Fixing {filename}")
        with open(filename, 'w') as f:
            f.writelines([f'{ln}\n' for ln in contents])


def main():
    '''parse arguments and then call other functions to do work'''
    global VERBOSE
    parser = ArgumentParser(description='Run syntax checks on DPDK meson.build files')
    parser.add_argument('-d', metavar='directory', default='.', help='Directory to process')
    parser.add_argument('--fix', action='store_true', help='Attempt to fix errors')
    parser.add_argument('-v', action='store_true', help='Verbose output')
    args = parser.parse_args()

    VERBOSE = args.v
    for f in scan_dir(args.d):
        process_file(f, args.fix)


if __name__ == "__main__":
    main()
