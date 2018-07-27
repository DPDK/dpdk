# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

# The main logic behind running autotests in parallel

from __future__ import print_function
import StringIO
import csv
import multiprocessing
import pexpect
import re
import subprocess
import sys
import time

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

# run a test group
# each result tuple in results list consists of:
#   result value (0 or -1)
#   result string
#   test name
#   total test run time (double)
#   raw test log
#   test report (if not available, should be None)
#
# this function needs to be outside AutotestRunner class
# because otherwise Pool won't work (or rather it will require
# quite a bit of effort to make it work).


def run_test_group(cmdline, target, test_group):
    results = []
    child = None
    start_time = time.time()
    startuplog = None

    # run test app
    try:
        # prepare logging of init
        startuplog = StringIO.StringIO()

        print("\n%s %s\n" % ("=" * 20, test_group["Prefix"]), file=startuplog)
        print("\ncmdline=%s" % cmdline, file=startuplog)

        child = pexpect.spawn(cmdline, logfile=startuplog)

        # wait for target to boot
        if not wait_prompt(child):
            child.close()

            results.append((-1,
                            "Fail [No prompt]",
                            "Start %s" % test_group["Prefix"],
                            time.time() - start_time,
                            startuplog.getvalue(),
                            None))

            # mark all tests as failed
            for test in test_group["Tests"]:
                results.append((-1, "Fail [No prompt]", test["Name"],
                                time.time() - start_time, "", None))
            # exit test
            return results

    except:
        results.append((-1,
                        "Fail [Can't run]",
                        "Start %s" % test_group["Prefix"],
                        time.time() - start_time,
                        startuplog.getvalue(),
                        None))

        # mark all tests as failed
        for t in test_group["Tests"]:
            results.append((-1, "Fail [Can't run]", t["Name"],
                            time.time() - start_time, "", None))
        # exit test
        return results

    # startup was successful
    results.append((0, "Success", "Start %s" % test_group["Prefix"],
                    time.time() - start_time, startuplog.getvalue(), None))

    # run all tests in test group
    for test in test_group["Tests"]:

        # create log buffer for each test
        # in multiprocessing environment, the logging would be
        # interleaved and will create a mess, hence the buffering
        logfile = StringIO.StringIO()
        child.logfile = logfile

        result = ()

        # make a note when the test started
        start_time = time.time()

        try:
            # print test name to log buffer
            print("\n%s %s\n" % ("-" * 20, test["Name"]), file=logfile)

            # run test function associated with the test
            result = test["Func"](child, test["Command"])

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
        finally:
            # append the results to the results list
            results.append(result)

    # regardless of whether test has crashed, try quitting it
    try:
        child.sendline("quit")
        child.close()
    # if the test crashed, just do nothing instead
    except:
        # nop
        pass

    # return test results
    return results


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
    blacklist = []
    whitelist = []

    def __init__(self, cmdline, target, blacklist, whitelist):
        self.cmdline = cmdline
        self.target = target
        self.binary = cmdline.split()[0]
        self.blacklist = blacklist
        self.whitelist = whitelist
        self.skipped = []

        # log file filename
        logfile = "%s.log" % target
        csvfile = "%s.csv" % target

        self.logfile = open(logfile, "w")
        csvfile = open(csvfile, "w")
        self.csvwriter = csv.writer(csvfile)

        # prepare results table
        self.csvwriter.writerow(["test_name", "test_result", "result_str"])

    # set up cmdline string
    def __get_cmdline(self, test):
        cmdline = self.cmdline

        # append memory limitations for each test
        # otherwise tests won't run in parallel
        if "i686" not in self.target:
            cmdline += " --socket-mem=%s" % test["Memory"]
        else:
            # affinitize startup so that tests don't fail on i686
            cmdline = "taskset 1 " + cmdline
            cmdline += " -m " + str(sum(map(int, test["Memory"].split(","))))

        # set group prefix for autotest group
        # otherwise they won't run in parallel
        cmdline += " --file-prefix=%s" % test["Prefix"]

        return cmdline

    def add_parallel_test_group(self, test_group):
        self.parallel_test_groups.append(test_group)

    def add_non_parallel_test_group(self, test_group):
        self.non_parallel_test_groups.append(test_group)

    def __process_results(self, results):
        # this iterates over individual test results
        for i, result in enumerate(results):

            # increase total number of tests that were run
            # do not include "start" test
            if i > 0:
                self.n_tests += 1

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
            if i == len(results) - 1:
                print(result +
                      "[%02dm %02ds]" % (total_time / 60, total_time % 60))
            else:
                print(result)

            # if test failed and it wasn't a "start" test
            if test_result < 0 and not i == 0:
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
            if i != 0:
                self.csvwriter.writerow([test_name, test_result, result_str])

    # this function checks individual test and decides if this test should be in
    # the group by comparing it against  whitelist/blacklist. it also checks if
    # the test is compiled into the binary, and marks it as skipped if necessary
    def __filter_test(self, test):
        test_cmd = test["Command"]
        test_id = test_cmd

        # dump tests are specified in full e.g. "Dump_mempool"
        if "_autotest" in test_id:
            test_id = test_id[:-len("_autotest")]

        # filter out blacklisted/whitelisted tests
        if self.blacklist and test_id in self.blacklist:
            return False
        if self.whitelist and test_id not in self.whitelist:
            return False

        # if test wasn't compiled in, remove it as well

        # parse the binary for available test commands
        stripped = 'not stripped' not in \
                   subprocess.check_output(['file', self.binary])
        if not stripped:
            symbols = subprocess.check_output(['nm',
                                               self.binary]).decode('utf-8')
            avail_cmds = re.findall('test_register_(\w+)', symbols)

            if test_cmd not in avail_cmds:
                # notify user
                result = 0, "Skipped [Not compiled]", test_id, 0, "", None
                self.skipped.append(tuple(result))
                return False

        return True

    def __filter_group(self, group):
        group["Tests"] = list(filter(self.__filter_test, group["Tests"]))
        return len(group["Tests"]) > 0

    # iterate over test groups and run tests associated with them
    def run_all_tests(self):
        # filter groups
        # for each test group, check all tests against the filter, then remove
        # all groups that don't have any tests
        self.parallel_test_groups = list(
            filter(self.__filter_group,
                   self.parallel_test_groups)
        )
        self.non_parallel_test_groups = list(
            filter(self.__filter_group,
                   self.non_parallel_test_groups)
        )

        # create a pool of worker threads
        pool = multiprocessing.Pool(processes=1)

        results = []

        # whatever happens, try to save as much logs as possible
        try:

            # create table header
            print("")
            print("Test name".ljust(30) + "Test result".ljust(29) +
                  "Test".center(9) + "Total".center(9))
            print("=" * 80)

            # print out skipped autotests if there were any
            if len(self.skipped):
                print("Skipped autotests:")

                # print out any skipped tests
                for result in self.skipped:
                    # unpack result tuple
                    test_result, result_str, test_name, _, _, _ = result
                    self.csvwriter.writerow([test_name, test_result,
                                             result_str])

                    t = ("%s:" % test_name).ljust(30)
                    t += result_str.ljust(29)
                    t += "[00m 00s]"

                    print(t)

            # make a note of tests start time
            self.start = time.time()

            print("Parallel autotests:")
            # assign worker threads to run test groups
            for test_group in self.parallel_test_groups:
                result = pool.apply_async(run_test_group,
                                          [self.__get_cmdline(test_group),
                                           self.target,
                                           test_group])
                results.append(result)

            # iterate while we have group execution results to get
            while len(results) > 0:

                # iterate over a copy to be able to safely delete results
                # this iterates over a list of group results
                for group_result in results[:]:

                    # if the thread hasn't finished yet, continue
                    if not group_result.ready():
                        continue

                    res = group_result.get()

                    self.__process_results(res)

                    # remove result from results list once we're done with it
                    results.remove(group_result)

            print("Non-parallel autotests:")
            # run non_parallel tests. they are run one by one, synchronously
            for test_group in self.non_parallel_test_groups:
                group_result = run_test_group(
                    self.__get_cmdline(test_group), self.target, test_group)

                self.__process_results(group_result)

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
