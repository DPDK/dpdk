#!/usr/bin/python

#   BSD LICENSE
# 
#   Copyright(c) 2010-2012 Intel Corporation. All rights reserved.
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
# 
#  version: DPDK.L.1.2.3-3

# Script that uses qemu controlled by python-pexpect to check that
# all autotests are working in the baremetal environment.

import sys, pexpect, time, os, re

directory = sys.argv[2]
target = sys.argv[3]
log_file = "%s.txt"%(target)

if "baremetal" in target:
    cmdline  = "qemu-system-x86_64 -cdrom %s.iso -boot d "%(sys.argv[1])
    cmdline	+= "-m 2000 -smp 4 -nographic -net nic,model=e1000"
    platform = "QEMU x86_64"
else:
    cmdline  = "%s -c f -n 4"%(sys.argv[1])
    try:
        platform = open("/root/rte_platform_model.txt").read()
    except:
        platform = "unknown"

print cmdline

report_hdr=""".. <COPYRIGHT_TAG>

"""

test_whitelist=None
test_blacklist=None

class SubTest:
    "Defines a subtest"
    def __init__(self, title, function, command=None, timeout=10, genreport=None):
        self.title = title
        self.function = function
        self.command = command
        self.timeout = timeout
        self.genreport = genreport

class AutoTest:
    """This class contains all methods needed to launch several
    automatic tests, archive test results, log, and generate a nice
    test report in restructured text"""

    title = "new"
    mainlog = None
    logbuf = None
    literal = 0
    test_list = []
    report_list = []
    child = None

    def __init__(self, pexpectchild, filename, mode):
        "Init the Autotest class"
        self.mainlog = file(filename, mode)
        self.child = pexpectchild
        pexpectchild.logfile = self
    def register(self, filename, title, subtest_list):
        "Register a test with a list of subtests"
        test = {}
        test["filename"] = filename
        test["title"] = title
        test["subtest_list"] = subtest_list
        self.test_list.append(test)

    def start(self):
        "start the tests, and fill the internal report_list field"
        for t in self.test_list:
            report = {}
            report["date"] = time.asctime()
            report["title"] = t["title"]
            report["filename"] = t["filename"]
            report["subreport_list"] = []
            report["fails"] = 0
            report["success"] = 0
            report["subreport_list"] = []
            for st in t["subtest_list"]:
                if test_whitelist is not None and st.title not in test_whitelist:
                    continue
                if test_blacklist is not None and st.title in test_blacklist:
                    continue
                subreport = {}
                self.reportbuf = ""
                subreport["title"] = st.title
                subreport["func"] = st.function
                subreport["command"] = st.command
                subreport["timeout"] = st.timeout
                subreport["genreport"] = st.genreport

                # launch subtest
                print "%s (%s): "%(subreport["title"], subreport["command"]),
                sys.stdout.flush()
                start = time.time()
                res = subreport["func"](self.child,
                                        command = subreport["command"],
                                        timeout = subreport["timeout"])
                t = int(time.time() - start)

                subreport["time"] = "%dmn%d"%(t/60, t%60)
                subreport["result"] = res[0] # 0 or -1
                subreport["result_str"] = res[1] # cause of fail
                subreport["logs"] = self.reportbuf
                print "%s [%s]"%(subreport["result_str"], subreport["time"])
                if subreport["result"] == 0:
                    report["success"] += 1
                else:
                    report["fails"] += 1
                report["subreport_list"].append(subreport)
            self.report_list.append(report)

    def gen_report(self):
        for report in self.report_list:
            # main report header and stats
            self.literal = 0
            reportlog = file(report["filename"], "w")
            reportlog.write(report_hdr)
            reportlog.write(report["title"] + "\n")
            reportlog.write(re.sub(".", "=", report["title"]) + "\n\n")
            reportlog.write("Autogenerated test report:\n\n" )
            reportlog.write("- date: **%s**\n"%(report["date"]))
            reportlog.write("- target: **%s**\n"%(target))
            reportlog.write("- success: **%d**\n"%(report["success"]))
            reportlog.write("- fails: **%d**\n"%(report["fails"]))
            reportlog.write("- platform: **%s**\n\n"%(platform))

            # summary
            reportlog.write(".. csv-table:: Test results summary\n")
            reportlog.write('   :header: "Name", "Result"\n\n')
            for subreport in report["subreport_list"]:
                if subreport["result"] == 0:
                    res_str = "Success"
                else:
                    res_str = "Failure"
                reportlog.write('   "%s", "%s"\n'%(subreport["title"], res_str))
            reportlog.write('\n')

            # subreports
            for subreport in report["subreport_list"]:
                # print subtitle
                reportlog.write(subreport["title"] + "\n")
                reportlog.write(re.sub(".", "-", subreport["title"]) + "\n\n")
                # print logs
                reportlog.write("::\n  \n  ")
                s = subreport["logs"].replace("\n", "\n  ")
                reportlog.write(s)
                # print result
                reportlog.write("\n\n")
                reportlog.write("**" + subreport["result_str"] + "**\n\n")
                # custom genreport
                if subreport["genreport"] != None:
                    s = subreport["genreport"]()
                    reportlog.write(s)

            reportlog.close()

        # displayed on console
        print
        print "-------------------------"
        print
        if report["fails"] == 0:
            print "All test OK"
        else:
            print "%s test(s) failed"%(report["fails"])

    # file API, to store logs from pexpect
    def write(self, buf):
        s = buf[:]
        s = s.replace("\r", "")
        self.mainlog.write(s)
        self.reportbuf += s
    def flush(self):
        self.mainlog.flush()
    def close(self):
        self.mainlog.close()


# Try to match prompt: return 0 on success, else return -1
def wait_prompt(child):
    for i in range(3):
        index = child.expect(["RTE>>", pexpect.TIMEOUT], timeout = 1)
        child.sendline("")
        if index == 0:
            return 0
    print "Cannot find prompt"
    return -1

# Try to match prompt after boot: return 0 on success, else return -1
def wait_boot(child):
    index = child.expect(["RTE>>", pexpect.TIMEOUT],
                         timeout = 120)
    if index == 0:
        return 0
    if (wait_prompt(child) == -1):
        print "Target did not boot, failed"
        return -1
    return 0

# quit RTE
def quit(child):
    if wait_boot(child) != 0:
        return -1, "Cannot find prompt"
    child.sendline("quit")
    return 0, "Success"

# Default function to launch an autotest that does not need to
# interact with the user. Basically, this function calls the autotest
# function through command line interface, then check that it displays
# "Test OK" or "Test Failed".
def default_autotest(child, command, timeout=10):
    if wait_prompt(child) != 0:
        return -1, "Failed: cannot find prompt"
    child.sendline(command)
    index = child.expect(["Test OK", "Test Failed",
                          pexpect.TIMEOUT], timeout = timeout)
    if index == 1:
        return -1, "Failed"
    elif index == 2:
        return -1, "Failed [Timeout]"
    return 0, "Success"

# wait boot
def boot_autotest(child, **kargs):
    if wait_boot(child) != 0:
        return -1, "Cannot find prompt"
    return 0, "Success"

# Test memory dump. We need to check that at least one memory zone is
# displayed.
def memory_autotest(child, command, **kargs):
    if wait_prompt(child) != 0:
        return -1, "Failed: cannot find prompt"
    child.sendline(command)
    regexp = "phys:0x[0-9a-f]*, len:0x([0-9a-f]*), virt:0x[0-9a-f]*, socket_id:[0-9]*"
    index = child.expect([regexp, pexpect.TIMEOUT], timeout = 180)
    if index != 0:
        return -1, "Failed: timeout"
    size = int(child.match.groups()[0], 16)
    if size <= 0:
        return -1, "Failed: bad size"
    index = child.expect(["Test OK", "Test Failed",
                          pexpect.TIMEOUT], timeout = 10)
    if index == 1:
        return -1, "Failed: C code returned an error"
    elif index == 2:
        return -1, "Failed: timeout"
    return 0, "Success"

# Test some libc functions including scanf. This requires a
# interaction with the user (simulated in expect), so we cannot use
# default_autotest() here.
def string_autotest(child, command, **kargs):
    if wait_prompt(child) != 0:
        return -1, "Failed: cannot find prompt"
    child.sendline(command)
    index = child.expect(["Now, test scanf, enter this number",
                          pexpect.TIMEOUT], timeout = 10)
    if index != 0:
        return -1, "Failed: timeout"
    child.sendline("123456")
    index = child.expect(["number=123456", pexpect.TIMEOUT], timeout = 10)
    if index != 0:
        return -1, "Failed: timeout (2)"
    index = child.expect(["Test OK", "Test Failed",
                          pexpect.TIMEOUT], timeout = 10)
    if index != 0:
        return -1, "Failed: C code returned an error"
    return 0, "Success"

# Test spinlock. This requires to check the order of displayed lines:
# we cannot use default_autotest() here.
def spinlock_autotest(child, command, **kargs):
    i = 0
    ir = 0
    if wait_prompt(child) != 0:
        return -1, "Failed: cannot find prompt"
    child.sendline(command)
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
                return -1, "Failed: bad order"
            i = int(child.match.groups()[0])
        elif index == 3:
            if int(child.match.groups()[0]) < ir:
                return -1, "Failed: bad order"
            ir = int(child.match.groups()[0])

        # fail
        else:
            return -1, "Failed: timeout or error"

    return 0, "Success"


# Test rwlock. This requires to check the order of displayed lines:
# we cannot use default_autotest() here.
def rwlock_autotest(child, command, **kargs):
    i = 0
    if wait_prompt(child) != 0:
        return -1, "Failed: cannot find prompt"
    child.sendline(command)
    while True:
        index = child.expect(["Test OK",
                              "Test Failed",
                              "Hello from core ([0-9]*) !",
                              "Global write lock taken on master core ([0-9]*)",
                              pexpect.TIMEOUT], timeout = 10)
        # ok
        if index == 0:
            if i != 0xffff:
                return -1, "Failed: a message is missing"
            break

        # message, check ordering
        elif index == 2:
            if int(child.match.groups()[0]) < i:
                return -1, "Failed: bad order"
            i = int(child.match.groups()[0])

        # must be the last message, check ordering
        elif index == 3:
            i = 0xffff

        # fail
        else:
            return -1, "Failed: timeout or error"

    return 0, "Success"

# Test logs. This requires to check the order of displayed lines:
# we cannot use default_autotest() here.
def logs_autotest(child, command, **kargs):
    i = 0
    if wait_prompt(child) != 0:
        return -1, "Failed: cannot find prompt"
    child.sendline(command)

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

        # not ok
        if index != 0:
            return -1, "Failed: timeout or error"

    index = child.expect(["Test OK",
                          "Test Failed",
                          pexpect.TIMEOUT], timeout = 10)

    return 0, "Success"

# Test timers. This requires to check the order of displayed lines:
# we cannot use default_autotest() here.
def timer_autotest(child, command, **kargs):
    i = 0
    if wait_prompt(child) != 0:
        return -1, "Failed: cannot find prompt"
    child.sendline(command)

    index = child.expect(["Start timer stress tests \(30 seconds\)",
                          "Test Failed",
                          pexpect.TIMEOUT], timeout = 10)

    # not ok
    if index != 0:
        return -1, "Failed: timeout or error"

    index = child.expect(["Start timer basic tests \(30 seconds\)",
                          "Test Failed",
                          pexpect.TIMEOUT], timeout = 40)

    # not ok
    if index != 0:
        return -1, "Failed: timeout or error (2)"

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

        if index != 0:
            return -1, "Failed: timeout or error (3)"

        try:
            t = int(child.match.groups()[0])
            id = int(child.match.groups()[1])
            cnt = int(child.match.groups()[2])
            lcore = int(child.match.groups()[3])
        except:
            return -1, "Failed: cannot parse output"

        # timer0 always expires on the same core when cnt < 20
        if id == 0:
            if lcore_tim0 == -1:
                lcore_tim0 = lcore
            elif lcore != lcore_tim0 and cnt < 20:
                return -1, "Failed: lcore != lcore_tim0 (%d, %d)"%(lcore, lcore_tim0)
            if cnt > 21:
                return -1, "Failed: tim0 cnt > 21"

        # timer1 each time expires on a different core
        if id == 1:
            if lcore == lcore_tim1:
                return -1, "Failed: lcore == lcore_tim1 (%d, %d)"%(lcore, lcore_tim1)
            lcore_tim1 = lcore
            if cnt > 10:
                return -1, "Failed: tim1 cnt > 30"

        # timer0 always expires on the same core
        if id == 2:
            if lcore_tim2 == -1:
                lcore_tim2 = lcore
            elif lcore != lcore_tim2:
                return -1, "Failed: lcore != lcore_tim2 (%d, %d)"%(lcore, lcore_tim2)
            if cnt > 30:
                return -1, "Failed: tim2 cnt > 30"

        # timer0 always expires on the same core
        if id == 3:
            if lcore_tim3 == -1:
                lcore_tim3 = lcore
            elif lcore != lcore_tim3:
                return -1, "Failed: lcore_tim3 changed (%d -> %d)"%(lcore, lcore_tim3)
            if cnt > 30:
                return -1, "Failed: tim3 cnt > 30"

    # must be 2 different cores
    if lcore_tim0 == lcore_tim3:
        return -1, "Failed: lcore_tim0 (%d) == lcore_tim3 (%d)"%(lcore_tim0, lcore_tim3)

    return 0, "Success"

# Ring autotest
def ring_autotest(child, command, timeout=10):
    if wait_prompt(child) != 0:
        return -1, "Failed: cannot find prompt"
    child.sendline(command)
    index = child.expect(["Test OK", "Test Failed",
                          pexpect.TIMEOUT], timeout = timeout)
    if index != 0:
        return -1, "Failed"

    child.sendline("set_watermark test 100")
    child.sendline("set_quota test 16")
    child.sendline("dump_ring test")
    index = child.expect(["  watermark=100",
                          pexpect.TIMEOUT], timeout = 1)
    if index != 0:
        return -1, "Failed: bad watermark"

    index = child.expect(["  bulk_default=16",
                          pexpect.TIMEOUT], timeout = 1)
    if index != 0:
        return -1, "Failed: bad quota"

    return 0, "Success"

def ring_genreport():
    s  = "Performance curves\n"
    s += "------------------\n\n"
    sdk = os.getenv("RTE_SDK")
    script = os.path.join(sdk, "app/test/graph_ring.py")
    title ='"Autotest %s %s"'%(target, time.asctime())
    filename = target + ".txt"
    os.system("/usr/bin/python %s %s %s"%(script, filename, title))
    for f in os.listdir("."):
        if not f.startswith("ring"):
            continue
        if not f.endswith(".svg"):
            continue
        # skip single producer/consumer
        if "_sc" in f:
            continue
        if "_sp" in f:
            continue
        f = f[:-4] + ".png"
        s += ".. figure:: ../../images/autotests/%s/%s\n"%(target, f)
        s += "   :width: 50%\n\n"
        s += "   %s\n\n"%(f)
    return s

def mempool_genreport():
    s  = "Performance curves\n"
    s += "------------------\n\n"
    sdk = os.getenv("RTE_SDK")
    script = os.path.join(sdk, "app/test/graph_mempool.py")
    title ='"Autotest %s %s"'%(target, time.asctime())
    filename = target + ".txt"
    os.system("/usr/bin/python %s %s %s"%(script, filename, title))
    for f in os.listdir("."):
        if not f.startswith("mempool"):
            continue
        if not f.endswith(".svg"):
            continue
        # skip when n_keep = 128
        if "_128." in f:
            continue
        f = f[:-4] + ".png"
        s += ".. figure:: ../../images/autotests/%s/%s\n"%(target, f)
        s += "   :width: 50%\n\n"
        s += "   %s\n\n"%(f)
    return s

#
# main
#

if len(sys.argv) > 4:
    testlist=sys.argv[4].split(',')
    if testlist[0].startswith('-'):
        testlist[0]=testlist[0].lstrip('-')
        test_blacklist=testlist
    else:
        test_whitelist=testlist

child = pexpect.spawn(cmdline)
autotest = AutoTest(child, log_file,'w')

# timeout for memcpy and hash test
if "baremetal" in target:
    timeout = 60*180
else:
    timeout = 180

autotest.register("eal_report.rst", "EAL-%s"%(target),
                  [ SubTest("Boot", boot_autotest, "boot_autotest"),
                    SubTest("EAL Flags", default_autotest, "eal_flags_autotest"),
                    SubTest("Version", default_autotest, "version_autotest"),
                    SubTest("PCI", default_autotest, "pci_autotest"),
                    SubTest("Memory", memory_autotest, "memory_autotest"),
                    SubTest("Lcore launch", default_autotest, "per_lcore_autotest"),
                    SubTest("Spinlock", spinlock_autotest, "spinlock_autotest"),
                    SubTest("Rwlock", rwlock_autotest, "rwlock_autotest"),
                    SubTest("Atomic", default_autotest, "atomic_autotest"),
                    SubTest("Byte order", default_autotest, "byteorder_autotest"),
                    SubTest("Prefetch", default_autotest, "prefetch_autotest"),
                    SubTest("Debug", default_autotest, "debug_autotest"),
                    SubTest("Cycles", default_autotest, "cycles_autotest"),
                    SubTest("Logs", logs_autotest, "logs_autotest"),
                    SubTest("Memzone", default_autotest, "memzone_autotest"),
                    SubTest("Cpu flags", default_autotest, "cpuflags_autotest"),
                    SubTest("Memcpy", default_autotest, "memcpy_autotest", timeout),
                    SubTest("String Functions", default_autotest, "string_autotest"),
                    SubTest("Alarm", default_autotest, "alarm_autotest", 30),
                    SubTest("Interrupt", default_autotest, "interrupt_autotest"),
                    ])

autotest.register("ring_report.rst", "Ring-%s"%(target),
                  [ SubTest("Ring", ring_autotest, "ring_autotest", 30*60,
                            ring_genreport)
                    ])

if "baremetal" in target:
    timeout = 60*60*3
else:
    timeout = 60*30

autotest.register("mempool_report.rst", "Mempool-%s"%(target),
                  [ SubTest("Mempool", default_autotest, "mempool_autotest",
                            timeout, mempool_genreport)
                    ])
autotest.register("mbuf_report.rst", "Mbuf-%s"%(target),
                  [ SubTest("Mbuf", default_autotest, "mbuf_autotest", timeout=120)
                    ])
autotest.register("timer_report.rst", "Timer-%s"%(target),
                  [ SubTest("Timer", timer_autotest, "timer_autotest")
                    ])
autotest.register("malloc_report.rst", "Malloc-%s"%(target),
                  [ SubTest("Malloc", default_autotest, "malloc_autotest")
                    ])

# only do the hash autotest if supported by the platform
if not (platform.startswith("Intel(R) Core(TM)2 Quad CPU") or
        platform.startswith("QEMU")):
    autotest.register("hash_report.rst", "Hash-%s"%(target),
                      [ SubTest("Hash", default_autotest, "hash_autotest", timeout)
                        ])

autotest.register("lpm_report.rst", "LPM-%s"%(target),
                  [ SubTest("Lpm", default_autotest, "lpm_autotest", timeout)
                    ])
autotest.register("eal2_report.rst", "EAL2-%s"%(target),
                  [ SubTest("TailQ", default_autotest, "tailq_autotest"),
                   SubTest("Errno", default_autotest, "errno_autotest"),
                   SubTest("Multiprocess", default_autotest, "multiprocess_autotest")
                    ])

autotest.start()
autotest.gen_report()

quit(child)
child.terminate()
sys.exit(0)
