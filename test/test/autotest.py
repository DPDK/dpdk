#!/usr/bin/env python
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

# Script that uses either test app or qemu controlled by python-pexpect
from __future__ import print_function
import autotest_data
import autotest_runner
import sys


def usage():
    print("Usage: autotest.py [test app|test iso image] ",
          "[target] [whitelist|-blacklist]")

if len(sys.argv) < 3:
    usage()
    sys.exit(1)

target = sys.argv[2]

test_whitelist = None
test_blacklist = None

# get blacklist/whitelist
if len(sys.argv) > 3:
    testlist = sys.argv[3].split(',')
    testlist = [test.lower() for test in testlist]
    if testlist[0].startswith('-'):
        testlist[0] = testlist[0].lstrip('-')
        test_blacklist = testlist
    else:
        test_whitelist = testlist

cmdline = "%s -c f -n 4" % (sys.argv[1])

print(cmdline)

runner = autotest_runner.AutotestRunner(cmdline, target, test_blacklist,
                                        test_whitelist)

for test_group in autotest_data.parallel_test_group_list:
    runner.add_parallel_test_group(test_group)

for test_group in autotest_data.non_parallel_test_group_list:
    runner.add_non_parallel_test_group(test_group)

num_fails = runner.run_all_tests()

sys.exit(num_fails)
