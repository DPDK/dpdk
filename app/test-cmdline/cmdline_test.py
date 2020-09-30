#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

# Script that runs cmdline_test app and feeds keystrokes into it.
import cmdline_test_data
import os
import pexpect
import sys


#
# function to run test
#
def runTest(child, test):
    child.send(test["Sequence"])
    if test["Result"] is None:
        return 0
    child.expect(test["Result"], 1)

#
# history test is a special case
#
# This test does the following:
# 1) fills the history with garbage up to its full capacity
#    (just enough to remove last entry)
# 2) scrolls back history to the very beginning
# 3) checks if the output is as expected, that is, the first
#    number in the sequence (not the last entry before it)
#
# This is a self-contained test, it needs only a pexpect child
#
def runHistoryTest(child):
    # find out history size
    child.sendline(cmdline_test_data.CMD_GET_BUFSIZE)
    child.expect("History buffer size: \\d+", timeout=1)
    history_size = int(child.after[len(cmdline_test_data.BUFSIZE_TEMPLATE):])
    i = 0

    # fill the history with numbers
    while i < history_size // 10:
        # add 1 to prevent from parsing as octals
        child.send("1" + str(i).zfill(8) + cmdline_test_data.ENTER)
        # the app will simply print out the number
        child.expect(str(i + 100000000), timeout=1)
        i += 1
    # scroll back history
    child.send(cmdline_test_data.UP * (i + 2) + cmdline_test_data.ENTER)
    child.expect("100000000", timeout=1)

# the path to cmdline_test executable is supplied via command-line.
if len(sys.argv) < 2:
    print("Error: please supply cmdline_test app path")
    sys.exit(1)

test_app_path = sys.argv[1]

if not os.path.exists(test_app_path):
    print("Error: please supply cmdline_test app path")
    sys.exit(1)

child = pexpect.spawn(test_app_path)

print("Running command-line tests...")
for test in cmdline_test_data.tests:
    testname = (test["Name"] + ":").ljust(30)
    try:
        runTest(child, test)
        print(testname, "PASS")
    except:
        print(testname, "FAIL")
        print(child)
        sys.exit(1)

# since last test quits the app, run new instance
child = pexpect.spawn(test_app_path)

testname = ("History fill test:").ljust(30)
try:
    runHistoryTest(child)
    print(testname, "PASS")
except:
    print(testname, "FAIL")
    print(child)
    sys.exit(1)
child.close()
sys.exit(0)
