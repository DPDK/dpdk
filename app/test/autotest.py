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

# Script that uses either test app or qemu controlled by python-pexpect

import sys, autotest_data, autotest_runner



def usage():
	print"Usage: autotest.py [test app|test iso image]",
	print "[target] [whitelist|-blacklist]"

if len(sys.argv) < 3:
	usage()
	sys.exit(1)

target = sys.argv[2]

test_whitelist=None
test_blacklist=None

# get blacklist/whitelist
if len(sys.argv) > 3:
	testlist = sys.argv[3].split(',')
	testlist = [test.lower() for test in testlist]
	if testlist[0].startswith('-'):
		testlist[0] = testlist[0].lstrip('-')
		test_blacklist = testlist
	else:
		test_whitelist = testlist

cmdline  = "%s -c f -n 4"%(sys.argv[1])

print cmdline

runner = autotest_runner.AutotestRunner(cmdline, target, test_blacklist, test_whitelist)

for test_group in autotest_data.parallel_test_group_list:
	runner.add_parallel_test_group(test_group)

for test_group in autotest_data.non_parallel_test_group_list:
	runner.add_non_parallel_test_group(test_group)

num_fails = runner.run_all_tests()

sys.exit(num_fails)
