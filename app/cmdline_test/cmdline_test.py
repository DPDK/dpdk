#!/usr/bin/python

#   BSD LICENSE
#
#   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
#   All rights reserved.
#
#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted provided that the following conditions
#   are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#     * Neither the name of Intel Corporation nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
#   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Script that runs cmdline_test app and feeds keystrokes into it.

import sys, pexpect, string, os, cmdline_test_data

#
# function to run test
#
def runTest(child,test):
	child.send(test["Sequence"])
	if test["Result"] == None:
		return 0
	child.expect(test["Result"],1)

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
	while i < history_size / 10:
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
	print "Error: please supply cmdline_test app path"
	sys.exit(1)

test_app_path = sys.argv[1]

if not os.path.exists(test_app_path):
	print "Error: please supply cmdline_test app path"
	sys.exit(1)

child = pexpect.spawn(test_app_path)

print "Running command-line tests..."
for test in cmdline_test_data.tests:
	print (test["Name"] + ":").ljust(30),
	try:
		runTest(child,test)
		print "PASS"
	except:
		print "FAIL"
		print child
		sys.exit(1)

# since last test quits the app, run new instance
child = pexpect.spawn(test_app_path)

print ("History fill test:").ljust(30),
try:
	runHistoryTest(child)
	print "PASS"
except:
	print "FAIL"
	print child
	sys.exit(1)
child.close()
sys.exit(0)

