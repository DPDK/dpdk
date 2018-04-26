#!/usr/bin/env python

# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

import sys
import os
import argparse
import subprocess
import shlex

from threading import Timer

def kill(process):
    print "ERROR: Test app timed out"
    process.kill()

if "RTE_SDK" in os.environ:
    dpdk_path = os.environ["RTE_SDK"]
else:
    dpdk_path = "../.."

if "RTE_TARGET" in os.environ:
    dpdk_target = os.environ["RTE_TARGET"]
else:
    dpdk_target = "x86_64-native-linuxapp-gcc"

parser = argparse.ArgumentParser(
                    description='BBdev Unit Test Application',
                    formatter_class=argparse.ArgumentDefaultsHelpFormatter)
parser.add_argument("-p", "--testapp-path",
                    help="specifies path to the bbdev test app",
                    default=dpdk_path + "/" + dpdk_target + "/app/testbbdev")
parser.add_argument("-e", "--eal-params",
                    help="EAL arguments which are passed to the test app",
                    default="--vdev=baseband_null0")
parser.add_argument("-t", "--timeout",
                    type=int,
                    help="Timeout in seconds",
                    default=300)
parser.add_argument("-c", "--test-cases",
                    nargs="+",
                    help="Defines test cases to run. Run all if not specified")
parser.add_argument("-v", "--test-vector",
                    nargs="+",
                    help="Specifies paths to the test vector files.",
                    default=[dpdk_path +
                    "/app/test-bbdev/test_vectors/bbdev_null.data"])
parser.add_argument("-n", "--num-ops",
                    type=int,
                    help="Number of operations to process on device.",
                    default=32)
parser.add_argument("-b", "--burst-size",
                    nargs="+",
                    type=int,
                    help="Operations enqueue/dequeue burst size.",
                    default=[32])
parser.add_argument("-l", "--num-lcores",
                    type=int,
                    help="Number of lcores to run.",
                    default=16)

args = parser.parse_args()

if not os.path.exists(args.testapp_path):
    print "No such file: " + args.testapp_path
    sys.exit(1)

params = [args.testapp_path]
if args.eal_params:
    params.extend(shlex.split(args.eal_params))

params.extend(["--"])

if args.num_ops:
    params.extend(["-n", str(args.num_ops)])

if args.num_lcores:
    params.extend(["-l", str(args.num_lcores)])

if args.test_cases:
    params.extend(["-c"])
    params.extend([",".join(args.test_cases)])

exit_status = 0
for vector in args.test_vector:
    for burst_size in args.burst_size:
        call_params = params[:]
        call_params.extend(["-v", vector])
        call_params.extend(["-b", str(burst_size)])
        params_string = " ".join(call_params)

        print("Executing: {}".format(params_string))
        app_proc = subprocess.Popen(call_params)
        if args.timeout > 0:
            timer = Timer(args.timeout, kill, [app_proc])
            timer.start()

        try:
            app_proc.communicate()
        except:
            print("Error: failed to execute: {}".format(params_string))
        finally:
            timer.cancel()

        if app_proc.returncode != 0:
            exit_status = 1
            print("ERROR TestCase failed. Failed test for vector {}. Return code: {}".format(
                vector, app_proc.returncode))

sys.exit(exit_status)
