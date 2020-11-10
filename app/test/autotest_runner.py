#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

# The main logic behind running autotests in parallel

import io
import csv
from multiprocessing import Pool, Queue
import pexpect
import re
import subprocess
import sys
import time
import glob
import os

# wait for prompt
def wait_prompt(child):
    try:
        child.sendline()
        result = child.expect(["RTE>>", pexpect.TIMEOUT, pexpect.EOF],
                              timeout=120)
    except:
        return False
    if result == 0:
        return True
    else:
        return False


# get all valid NUMA nodes
def get_numa_nodes():
    return [
        int(
            re.match(r"node(\d+)", os.path.basename(node))
            .group(1)
        )
        for node in glob.glob("/sys/devices/system/node/node*")
    ]


# find first (or any, really) CPU on a particular node, will be used to spread
# processes around NUMA nodes to avoid exhausting memory on particular node
def first_cpu_on_node(node_nr):
    cpu_path = glob.glob("/sys/devices/system/node/node%d/cpu*" % node_nr)
    r = re.compile(r"cpu(\d+)")
    cpu_name = filter(None,
            map(r.match,
                map(os.path.basename, cpu_path)
            )
    )
    return int(next(cpu_name).group(1))

pool_child = None  # per-process child


# we initialize each worker with a queue because we need per-pool unique
# command-line arguments, but we cannot do different arguments in an initializer
# because the API doesn't allow per-worker initializer arguments. so, instead,
# we will initialize with a shared queue, and dequeue command-line arguments
# from this queue
def pool_init(queue, result_queue):
    global pool_child

    cmdline, prefix = queue.get()
    start_time = time.time()
    name = ("Start %s" % prefix) if prefix != "" else "Start"

    # use default prefix if no prefix was specified
    prefix_cmdline = "--file-prefix=%s" % prefix if prefix != "" else ""

    # append prefix to cmdline
    cmdline = "%s %s" % (cmdline, prefix_cmdline)

    # prepare logging of init
    startuplog = io.StringIO()

    # run test app
    try:

        print("\n%s %s\n" % ("=" * 20, prefix), file=startuplog)
        print("\ncmdline=%s" % cmdline, file=startuplog)

        pool_child = pexpect.spawn(cmdline, logfile=startuplog, encoding='utf-8')
        # wait for target to boot
        if not wait_prompt(pool_child):
            pool_child.close()

            result = tuple((-1,
                            "Fail [No prompt]",
                            name,
                            time.time() - start_time,
                            startuplog.getvalue(),
                            None))
            pool_child = None
        else:
            result = tuple((0,
                            "Success",
                            name,
                            time.time() - start_time,
                            startuplog.getvalue(),
                            None))
    except:
        result = tuple((-1,
                        "Fail [Can't run]",
                        name,
                        time.time() - start_time,
                        startuplog.getvalue(),
                        None))
        pool_child = None

    result_queue.put(result)


# run a test
# each result tuple in results list consists of:
#   result value (0 or -1)
#   result string
#   test name
#   total test run time (double)
#   raw test log
#   test report (if not available, should be None)
#
# this function needs to be outside AutotestRunner class because otherwise Pool
# won't work (or rather it will require quite a bit of effort to make it work).
def run_test(target, test):
    global pool_child

    if pool_child is None:
        return -1, "Fail [No test process]", test["Name"], 0, "", None

    # create log buffer for each test
    # in multiprocessing environment, the logging would be
    # interleaved and will create a mess, hence the buffering
    logfile = io.StringIO()
    pool_child.logfile = logfile

    # make a note when the test started
    start_time = time.time()

    try:
        # print test name to log buffer
        print("\n%s %s\n" % ("-" * 20, test["Name"]), file=logfile)

        # run test function associated with the test
        result = test["Func"](pool_child, test["Command"])

        # make a note when the test was finished
        end_time = time.time()

        log = logfile.getvalue()

        # append test data to the result tuple
        result += (test["Name"], end_time - start_time, log)

        # call report function, if any defined, and supply it with
        # target and complete log for test run
        if test["Report"]:
            report = test["Report"](target, log)

            # append report to results tuple
            result += (report,)
        else:
            # report is None
            result += (None,)
    except:
        # make a note when the test crashed
        end_time = time.time()

        # mark test as failed
        result = (-1, "Fail [Crash]", test["Name"],
                  end_time - start_time, logfile.getvalue(), None)

    # return test results
    return result


# class representing an instance of autotests run
class AutotestRunner:
    cmdline = ""
    parallel_test_groups = []
    non_parallel_test_groups = []
    logfile = None
    csvwriter = None
    target = ""
    start = None
    n_tests = 0
    fails = 0
    log_buffers = []
    blocklist = []
    allowlist = []

    def __init__(self, cmdline, target, blocklist, allowlist, n_processes):
        self.cmdline = cmdline
        self.target = target
        self.blocklist = blocklist
        self.allowlist = allowlist
        self.skipped = []
        self.parallel_tests = []
        self.non_parallel_tests = []
        self.n_processes = n_processes
        self.active_processes = 0

        # parse the binary for available test commands
        binary = cmdline.split()[0]
        stripped = 'not stripped' not in \
                   subprocess.check_output(['file', binary]).decode()
        if not stripped:
            symbols = subprocess.check_output(['nm', binary]).decode()
            self.avail_cmds = re.findall('test_register_(\w+)', symbols)
        else:
            self.avail_cmds = None

        # log file filename
        logfile = "%s.log" % target
        csvfile = "%s.csv" % target

        self.logfile = open(logfile, "w")
        csvfile = open(csvfile, "w")
        self.csvwriter = csv.writer(csvfile)

        # prepare results table
        self.csvwriter.writerow(["test_name", "test_result", "result_str"])

    # set up cmdline string
    def __get_cmdline(self, cpu_nr):
        cmdline = ("taskset -c %i " % cpu_nr) + self.cmdline

        return cmdline

    def __process_result(self, result):

        # unpack result tuple
        test_result, result_str, test_name, \
            test_time, log, report = result

        # get total run time
        cur_time = time.time()
        total_time = int(cur_time - self.start)

        # print results, test run time and total time since start
        result = ("%s:" % test_name).ljust(30)
        result += result_str.ljust(29)
        result += "[%02dm %02ds]" % (test_time / 60, test_time % 60)

        # don't print out total time every line, it's the same anyway
        print(result + "[%02dm %02ds]" % (total_time / 60, total_time % 60))

        # if test failed and it wasn't a "start" test
        if test_result < 0:
            self.fails += 1

        # collect logs
        self.log_buffers.append(log)

        # create report if it exists
        if report:
            try:
                f = open("%s_%s_report.rst" %
                         (self.target, test_name), "w")
            except IOError:
                print("Report for %s could not be created!" % test_name)
            else:
                with f:
                    f.write(report)

        # write test result to CSV file
        self.csvwriter.writerow([test_name, test_result, result_str])

    # this function checks individual test and decides if this test should be in
    # the group by comparing it against allowlist/blocklist. it also checks if
    # the test is compiled into the binary, and marks it as skipped if necessary
    def __filter_test(self, test):
        test_cmd = test["Command"]
        test_id = test_cmd

        # dump tests are specified in full e.g. "Dump_mempool"
        if "_autotest" in test_id:
            test_id = test_id[:-len("_autotest")]

        # filter out blocked/allowed tests
        if self.blocklist and test_id in self.blocklist:
            return False
        if self.allowlist and test_id not in self.allowlist:
            return False

        # if test wasn't compiled in, remove it as well
        if self.avail_cmds and test_cmd not in self.avail_cmds:
            result = 0, "Skipped [Not compiled]", test_id, 0, "", None
            self.skipped.append(tuple(result))
            return False

        return True

    def __run_test_group(self, test_group, worker_cmdlines):
        group_queue = Queue()
        init_result_queue = Queue()
        for proc, cmdline in enumerate(worker_cmdlines):
            prefix = "test%i" % proc if len(worker_cmdlines) > 1 else ""
            group_queue.put(tuple((cmdline, prefix)))

        # create a pool of worker threads
        # we will initialize child in the initializer, and we don't need to
        # close the child because when the pool worker gets destroyed, child
        # closes the process
        pool = Pool(processes=len(worker_cmdlines),
                    initializer=pool_init,
                    initargs=(group_queue, init_result_queue))

        results = []

        # process all initialization results
        for _ in range(len(worker_cmdlines)):
            self.__process_result(init_result_queue.get())

        # run all tests asynchronously
        for test in test_group:
            result = pool.apply_async(run_test, (self.target, test))
            results.append(result)

        # tell the pool to stop all processes once done
        pool.close()

        # iterate while we have group execution results to get
        while len(results) > 0:
            # iterate over a copy to be able to safely delete results
            # this iterates over a list of group results
            for async_result in results[:]:
                # if the thread hasn't finished yet, continue
                if not async_result.ready():
                    continue

                res = async_result.get()

                self.__process_result(res)

                # remove result from results list once we're done with it
                results.remove(async_result)

    # iterate over test groups and run tests associated with them
    def run_all_tests(self):
        # filter groups
        self.parallel_tests = list(
            filter(self.__filter_test,
                   self.parallel_tests)
        )
        self.non_parallel_tests = list(
            filter(self.__filter_test,
                   self.non_parallel_tests)
        )

        parallel_cmdlines = []
        # FreeBSD doesn't have NUMA support
        numa_nodes = get_numa_nodes()
        if len(numa_nodes) > 0:
            for proc in range(self.n_processes):
                # spread cpu affinity between NUMA nodes to have less chance of
                # running out of memory while running multiple test apps in
                # parallel. to do that, alternate between NUMA nodes in a round
                # robin fashion, and pick an arbitrary CPU from that node to
                # taskset our execution to
                numa_node = numa_nodes[self.active_processes % len(numa_nodes)]
                cpu_nr = first_cpu_on_node(numa_node)
                parallel_cmdlines += [self.__get_cmdline(cpu_nr)]
                # increase number of active processes so that the next cmdline
                # gets a different NUMA node
                self.active_processes += 1
        else:
            parallel_cmdlines = [self.cmdline] * self.n_processes

        print("Running tests with %d workers" % self.n_processes)

        # create table header
        print("")
        print("Test name".ljust(30) + "Test result".ljust(29) +
              "Test".center(9) + "Total".center(9))
        print("=" * 80)

        if len(self.skipped):
            print("Skipped autotests:")

            # print out any skipped tests
            for result in self.skipped:
                # unpack result tuple
                test_result, result_str, test_name, _, _, _ = result
                self.csvwriter.writerow([test_name, test_result, result_str])

                t = ("%s:" % test_name).ljust(30)
                t += result_str.ljust(29)
                t += "[00m 00s]"

                print(t)

        # make a note of tests start time
        self.start = time.time()

        # whatever happens, try to save as much logs as possible
        try:
            if len(self.parallel_tests) > 0:
                print("Parallel autotests:")
                self.__run_test_group(self.parallel_tests, parallel_cmdlines)

            if len(self.non_parallel_tests) > 0:
                print("Non-parallel autotests:")
                self.__run_test_group(self.non_parallel_tests, [self.cmdline])

            # get total run time
            cur_time = time.time()
            total_time = int(cur_time - self.start)

            # print out summary
            print("=" * 80)
            print("Total run time: %02dm %02ds" % (total_time / 60,
                                                   total_time % 60))
            if self.fails != 0:
                print("Number of failed tests: %s" % str(self.fails))

            # write summary to logfile
            self.logfile.write("Summary\n")
            self.logfile.write("Target: ".ljust(15) + "%s\n" % self.target)
            self.logfile.write("Tests: ".ljust(15) + "%i\n" % self.n_tests)
            self.logfile.write("Failed tests: ".ljust(
                15) + "%i\n" % self.fails)
        except:
            print("Exception occurred")
            print(sys.exc_info())
            self.fails = 1

        # drop logs from all executions to a logfile
        for buf in self.log_buffers:
            self.logfile.write(buf.replace("\r", ""))

        return self.fails
