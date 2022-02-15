#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2021 Intel Corporation
#

import argparse
import fileinput
import sys
from os.path import abspath, relpath, join
from pathlib import Path
from mesonbuild import mesonmain


def args_parse():
    "parse arguments and return the argument object back to main"
    parser = argparse.ArgumentParser(description="This script can be used to remove includes which are not in use\n")
    parser.add_argument('-b', '--build_dir', type=str, default='build',
                        help="The path to the build directory in which the IWYU tool was used in.")
    parser.add_argument('-d', '--sub_dir', type=str, default='',
                        help="The sub-directory to remove headers from.")
    parser.add_argument('file', type=Path,
                        help="The path to the IWYU log file or output from stdin.")

    return parser.parse_args()


def run_meson(args):
    "Runs a meson command logging output to process-iwyu.log"
    with open('process-iwyu.log', 'a') as sys.stdout:
        ret = mesonmain.run(args, abspath('meson'))
    sys.stdout = sys.__stdout__
    return ret


def remove_includes(filepath, include, build_dir):
    "Attempts to remove include, if it fails then revert to original state"
    with open(filepath) as f:
        lines = f.readlines()  # Read lines when file is opened

    with open(filepath, 'w') as f:
        for ln in lines:  # Removes the include passed in
            if not ln.startswith(include):
                f.write(ln)

    # run test build -> call meson on the build folder, meson compile -C build
    ret = run_meson(['compile', '-C', build_dir])
    if (ret == 0):  # Include is not needed -> build is successful
        print('SUCCESS')
    else:
        # failed, catch the error
        # return file to original state
        with open(filepath, 'w') as f:
            f.writelines(lines)
        print('FAILED')


def get_build_config(builddir, condition):
    "returns contents of rte_build_config.h"
    with open(join(builddir, 'rte_build_config.h')) as f:
        return [ln for ln in f.readlines() if condition(ln)]


def uses_libbsd(builddir):
    "return whether the build uses libbsd or not"
    return bool(get_build_config(builddir, lambda ln: 'RTE_USE_LIBBSD' in ln))


def process(args):
    "process the iwyu output on a set of files"
    filepath = None
    build_dir = abspath(args.build_dir)
    directory = args.sub_dir

    print("Warning: The results of this script may include false positives which are required for different systems",
          file=sys.stderr)

    keep_str_fns = uses_libbsd(build_dir)  # check for libbsd
    if keep_str_fns:
        print("Warning: libbsd is present, build will fail to detect incorrect removal of rte_string_fns.h",
              file=sys.stderr)
    # turn on werror
    run_meson(['configure', build_dir, '-Dwerror=true'])
    # Use stdin if no iwyu_tool out file given
    for line in fileinput.input(args.file):
        if 'should remove' in line:
            # If the file path in the iwyu_tool output is an absolute path it
            # means the file is outside of the dpdk directory, therefore ignore it.
            # Also check to see if the file is within the specified sub directory.
            filename = line.split()[0]
            if (filename != abspath(filename) and
                    directory in filename):
                filepath = relpath(join(build_dir, filename))
        elif line.startswith('-') and filepath:
            include = '#include ' + line.split()[2]
            print(f"Remove {include} from {filepath} ... ", end='', flush=True)
            if keep_str_fns and '<rte_string_fns.h>' in include:
                print('skipped')
                continue
            remove_includes(filepath, include, build_dir)
        else:
            filepath = None


def main():
    process(args_parse())


if __name__ == '__main__':
    main()
