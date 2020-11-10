#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

# Script that uses either test app or qemu controlled by python-pexpect
import autotest_data
import autotest_runner
import sys


def usage():
    print("Usage: autotest.py [test app|test iso image] ",
          "[target] [allow|-block]")

if len(sys.argv) < 3:
    usage()
    sys.exit(1)

target = sys.argv[2]

test_allowlist = None
test_blocklist = None

# get blocklist/allowlist
if len(sys.argv) > 3:
    testlist = sys.argv[3].split(',')
    testlist = [test.lower() for test in testlist]
    if testlist[0].startswith('-'):
        testlist[0] = testlist[0].lstrip('-')
        test_blocklist = testlist
    else:
        test_allowlist = testlist

cmdline = "%s -c f" % (sys.argv[1])

print(cmdline)

# how many workers to run tests with. FreeBSD doesn't support multiple primary
# processes, so make it 1, otherwise make it 4. ignored for non-parallel tests
n_processes = 1 if "bsd" in target else 4

runner = autotest_runner.AutotestRunner(cmdline, target, test_blocklist,
                                        test_allowlist, n_processes)

runner.parallel_tests = autotest_data.parallel_test_list[:]
runner.non_parallel_tests = autotest_data.non_parallel_test_list[:]

num_fails = runner.run_all_tests()

sys.exit(num_fails)
