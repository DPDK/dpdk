# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

# Test functions

import pexpect

# default autotest, used to run most tests
# waits for "Test OK"


def default_autotest(child, test_name):
    child.sendline(test_name)
    result = child.expect(["Test OK", "Test Failed",
                           "Command not found", pexpect.TIMEOUT], timeout=900)
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
    lines = 0
    error = ''
    child.sendline(test_name)
    while True:
        regexp = "IOVA:0x[0-9a-f]*, len:([0-9]*), virt:0x[0-9a-f]*, " \
                 "socket_id:[0-9]*"
        index = child.expect([regexp, "Test OK", "Test Failed",
                              pexpect.TIMEOUT], timeout=10)
        if index == 3:
            return -1, "Fail [Timeout]"
        elif index == 1:
            break
        elif index == 2:
            return -1, "Fail"
        else:
            lines = lines + 1
            size = int(child.match.groups()[0], 10)
            if size <= 0:
                error = 'Bad size'

    if lines <= 0:
        return -1, "Fail [No entries]"
    if error != '':
        return -1, "Fail [{}]".format(error)
    return 0, "Success"


def spinlock_autotest(child, test_name):
    i = 0
    ir = 0
    child.sendline(test_name)
    while True:
        index = child.expect(["Test OK",
                              "Test Failed",
                              "Hello from core ([0-9]*) !",
                              "Hello from within recursive locks "
                              "from ([0-9]*) !",
                              pexpect.TIMEOUT], timeout=5)
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
                              "Global write lock taken on master "
                              "core ([0-9]*)",
                              pexpect.TIMEOUT], timeout=10)
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
    child.sendline(test_name)

    log_list = [
        "TESTAPP1: error message",
        "TESTAPP1: critical message",
        "TESTAPP2: critical message",
        "TESTAPP1: error message",
    ]

    for log_msg in log_list:
        index = child.expect([log_msg,
                              "Test OK",
                              "Test Failed",
                              pexpect.TIMEOUT], timeout=10)

        if index == 3:
            return -1, "Fail [Timeout]"
        # not ok
        elif index != 0:
            return -1, "Fail"

    index = child.expect(["Test OK",
                          "Test Failed",
                          pexpect.TIMEOUT], timeout=10)

    return 0, "Success"


def timer_autotest(child, test_name):
    child.sendline(test_name)

    index = child.expect(["Start timer stress tests",
                          "Test Failed",
                          pexpect.TIMEOUT], timeout=5)

    if index == 1:
        return -1, "Fail"
    elif index == 2:
        return -1, "Fail [Timeout]"

    index = child.expect(["Start timer stress tests 2",
                          "Test Failed",
                          pexpect.TIMEOUT], timeout=5)

    if index == 1:
        return -1, "Fail"
    elif index == 2:
        return -1, "Fail [Timeout]"

    index = child.expect(["Start timer basic tests",
                          "Test Failed",
                          pexpect.TIMEOUT], timeout=5)

    if index == 1:
        return -1, "Fail"
    elif index == 2:
        return -1, "Fail [Timeout]"

    lcore_tim0 = -1
    lcore_tim1 = -1
    lcore_tim2 = -1
    lcore_tim3 = -1

    while True:
        index = child.expect(["TESTTIMER: ([0-9]*): callback id=([0-9]*) "
                              "count=([0-9]*) on core ([0-9]*)",
                              "Test OK",
                              "Test Failed",
                              pexpect.TIMEOUT], timeout=10)

        if index == 1:
            break

        if index == 2:
            return -1, "Fail"
        elif index == 3:
            return -1, "Fail [Timeout]"

        try:
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
                return -1, "Fail [lcore != lcore_tim0 (%d, %d)]" \
                    % (lcore, lcore_tim0)
            if cnt > 21:
                return -1, "Fail [tim0 cnt > 21]"

        # timer1 each time expires on a different core
        if id == 1:
            if lcore == lcore_tim1:
                return -1, "Fail [lcore == lcore_tim1 (%d, %d)]" \
                    % (lcore, lcore_tim1)
            lcore_tim1 = lcore
            if cnt > 10:
                return -1, "Fail [tim1 cnt > 30]"

        # timer0 always expires on the same core
        if id == 2:
            if lcore_tim2 == -1:
                lcore_tim2 = lcore
            elif lcore != lcore_tim2:
                return -1, "Fail [lcore != lcore_tim2 (%d, %d)]" \
                    % (lcore, lcore_tim2)
            if cnt > 30:
                return -1, "Fail [tim2 cnt > 30]"

        # timer0 always expires on the same core
        if id == 3:
            if lcore_tim3 == -1:
                lcore_tim3 = lcore
            elif lcore != lcore_tim3:
                return -1, "Fail [lcore_tim3 changed (%d -> %d)]" \
                    % (lcore, lcore_tim3)
            if cnt > 30:
                return -1, "Fail [tim3 cnt > 30]"

    # must be 2 different cores
    if lcore_tim0 == lcore_tim3:
        return -1, "Fail [lcore_tim0 (%d) == lcore_tim3 (%d)]" \
            % (lcore_tim0, lcore_tim3)

    return 0, "Success"


def ring_autotest(child, test_name):
    child.sendline(test_name)
    index = child.expect(["Test OK", "Test Failed",
                          pexpect.TIMEOUT], timeout=2)
    if index == 1:
        return -1, "Fail"
    elif index == 2:
        return -1, "Fail [Timeout]"

    return 0, "Success"
