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

# Test functions

import sys, pexpect, time, os, re

# default autotest, used to run most tests
# waits for "Test OK"
def default_autotest(child, test_name):
	child.sendline(test_name)
	result = child.expect(["Test OK", "Test Failed",
		"Command not found", pexpect.TIMEOUT], timeout = 900)
	if result == 1:
		return -1, "Fail"
	elif result == 2:
		return -1, "Fail [Not found]"
	elif result == 3:
		return -1, "Fail [Timeout]"
	return 0, "Success"

# autotest used to run dump commands
# just fires the command
def dump_autotest(child, test_name):
	child.sendline(test_name)
	return 0, "Success"

# memory autotest
# reads output and waits for Test OK
def memory_autotest(child, test_name):
	child.sendline(test_name)
	regexp = "phys:0x[0-9a-f]*, len:([0-9]*), virt:0x[0-9a-f]*, socket_id:[0-9]*"
	index = child.expect([regexp, pexpect.TIMEOUT], timeout = 180)
	if index != 0:
		return -1, "Fail [Timeout]"
	size = int(child.match.groups()[0], 16)
	if size <= 0:
		return -1, "Fail [Bad size]"
	index = child.expect(["Test OK", "Test Failed",
		          pexpect.TIMEOUT], timeout = 10)
	if index == 1:
		return -1, "Fail"
	elif index == 2:
		return -1, "Fail [Timeout]"
	return 0, "Success"

def spinlock_autotest(child, test_name):
	i = 0
	ir = 0
	child.sendline(test_name)
	while True:
		index = child.expect(["Test OK",
			"Test Failed",
			"Hello from core ([0-9]*) !",
			"Hello from within recursive locks from ([0-9]*) !",
		pexpect.TIMEOUT], timeout = 20)
		# ok
		if index == 0:
			break

		# message, check ordering
		elif index == 2:
			if int(child.match.groups()[0]) < i:
				return -1, "Fail [Bad order]"
			i = int(child.match.groups()[0])
		elif index == 3:
			if int(child.match.groups()[0]) < ir:
				return -1, "Fail [Bad order]"
			ir = int(child.match.groups()[0])

		# fail
		elif index == 4:
			return -1, "Fail [Timeout]"
		elif index == 1:
			return -1, "Fail"

	return 0, "Success"

def rwlock_autotest(child, test_name):
	i = 0
	child.sendline(test_name)
	while True:
		index = child.expect(["Test OK",
			"Test Failed",
			"Hello from core ([0-9]*) !",
			"Global write lock taken on master core ([0-9]*)",
		pexpect.TIMEOUT], timeout = 10)
		# ok
		if index == 0:
			if i != 0xffff:
				return -1, "Fail [Message is missing]"
			break

		# message, check ordering
		elif index == 2:
			if int(child.match.groups()[0]) < i:
				return -1, "Fail [Bad order]"
			i = int(child.match.groups()[0])

		# must be the last message, check ordering
		elif index == 3:
			i = 0xffff

		elif index == 4:
			return -1, "Fail [Timeout]"

		# fail
		else:
			return -1, "Fail"

	return 0, "Success"

def logs_autotest(child, test_name):
	i = 0
	child.sendline(test_name)

	log_list = [
		"TESTAPP1: this is a debug level message",
		"TESTAPP1: this is a info level message",
		"TESTAPP1: this is a warning level message",
		"TESTAPP2: this is a info level message",
		"TESTAPP2: this is a warning level message",
		"TESTAPP1: this is a debug level message",
		"TESTAPP1: this is a debug level message",
		"TESTAPP1: this is a info level message",
		"TESTAPP1: this is a warning level message",
		"TESTAPP2: this is a info level message",
		"TESTAPP2: this is a warning level message",
		"TESTAPP1: this is a debug level message",
	]

	for log_msg in log_list:
		index = child.expect([log_msg,
				      "Test OK",
				      "Test Failed",
				      pexpect.TIMEOUT], timeout = 10)

		if index == 3:
			return -1, "Fail [Timeout]"
		# not ok
		elif index != 0:
			return -1, "Fail"

	index = child.expect(["Test OK",
		"Test Failed",
		pexpect.TIMEOUT], timeout = 10)

	return 0, "Success"

def timer_autotest(child, test_name):
	i = 0
	child.sendline(test_name)

	index = child.expect(["Start timer stress tests \(20 seconds\)",
		"Test Failed",
		pexpect.TIMEOUT], timeout = 10)

	if index == 1:
		return -1, "Fail"
	elif index == 2:
		return -1, "Fail [Timeout]"

	index = child.expect(["Start timer stress tests 2",
		"Test Failed",
		pexpect.TIMEOUT], timeout = 40)

	if index == 1:
		return -1, "Fail"
	elif index == 2:
		return -1, "Fail [Timeout]"

	index = child.expect(["Start timer basic tests \(20 seconds\)",
		"Test Failed",
		pexpect.TIMEOUT], timeout = 20)

	if index == 1:
		return -1, "Fail"
	elif index == 2:
		return -1, "Fail [Timeout]"

	prev_lcore_timer1 = -1

	lcore_tim0 = -1
	lcore_tim1 = -1
	lcore_tim2 = -1
	lcore_tim3 = -1

	while True:
		index = child.expect(["TESTTIMER: ([0-9]*): callback id=([0-9]*) count=([0-9]*) on core ([0-9]*)",
			"Test OK",
			"Test Failed",
			pexpect.TIMEOUT], timeout = 10)

		if index == 1:
			break

		if index == 2:
			return -1, "Fail"
		elif index == 3:
			return -1, "Fail [Timeout]"

		try:
			t = int(child.match.groups()[0])
			id = int(child.match.groups()[1])
			cnt = int(child.match.groups()[2])
			lcore = int(child.match.groups()[3])
		except:
			return -1, "Fail [Cannot parse]"

		# timer0 always expires on the same core when cnt < 20
		if id == 0:
			if lcore_tim0 == -1:
				lcore_tim0 = lcore
			elif lcore != lcore_tim0 and cnt < 20:
				return -1, "Fail [lcore != lcore_tim0 (%d, %d)]"%(lcore, lcore_tim0)
			if cnt > 21:
				return -1, "Fail [tim0 cnt > 21]"

		# timer1 each time expires on a different core
		if id == 1:
			if lcore == lcore_tim1:
				return -1, "Fail [lcore == lcore_tim1 (%d, %d)]"%(lcore, lcore_tim1)
			lcore_tim1 = lcore
			if cnt > 10:
				return -1, "Fail [tim1 cnt > 30]"

		# timer0 always expires on the same core
		if id == 2:
			if lcore_tim2 == -1:
				lcore_tim2 = lcore
			elif lcore != lcore_tim2:
				return -1, "Fail [lcore != lcore_tim2 (%d, %d)]"%(lcore, lcore_tim2)
			if cnt > 30:
				return -1, "Fail [tim2 cnt > 30]"

		# timer0 always expires on the same core
		if id == 3:
			if lcore_tim3 == -1:
				lcore_tim3 = lcore
			elif lcore != lcore_tim3:
				return -1, "Fail [lcore_tim3 changed (%d -> %d)]"%(lcore, lcore_tim3)
			if cnt > 30:
				return -1, "Fail [tim3 cnt > 30]"

	# must be 2 different cores
	if lcore_tim0 == lcore_tim3:
		return -1, "Fail [lcore_tim0 (%d) == lcore_tim3 (%d)]"%(lcore_tim0, lcore_tim3)

	return 0, "Success"

def ring_autotest(child, test_name):
	child.sendline(test_name)
	index = child.expect(["Test OK", "Test Failed",
		pexpect.TIMEOUT], timeout = 15)
	if index == 1:
		return -1, "Fail"
	elif index == 2:
		return -1, "Fail [Timeout]"

	child.sendline("set_watermark test 100")
	child.sendline("dump_ring test")
	index = child.expect(["  watermark=100",
		pexpect.TIMEOUT], timeout = 1)
	if index != 0:
		return -1, "Fail [Bad watermark]"

	return 0, "Success"
