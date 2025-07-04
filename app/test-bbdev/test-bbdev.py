#!/usr/bin/env python3

# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

import sys
import os
import argparse
import subprocess
import shlex

from threading import Timer

def kill(process):
    print("ERROR: Test app timed out")
    process.kill()

dpdk_path = "../.."
dpdk_target = "build"

parser = argparse.ArgumentParser(
                    description='BBdev Unit Test Application',
                    formatter_class=argparse.ArgumentDefaultsHelpFormatter)
parser.add_argument("-p", "--testapp-path",
                    help="specifies path to the bbdev test app",
                    default=dpdk_path + "/" + dpdk_target + "/app/dpdk-test-bbdev")
parser.add_argument("-e", "--eal-params",
                    help="EAL arguments which must be passed to the test app",
                    default="--vdev=baseband_null0 -a00:00.0")
parser.add_argument("-T", "--timeout",
                    type=int,
                    help="Timeout in seconds",
                    default=600)
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
parser.add_argument("-s", "--snr",
                    type=int,
                    help="SNR in dB for BLER tests",
                    default=0)
parser.add_argument("-t", "--iter-max",
                    type=int,
                    help="Max iterations",
                    default=6)
parser.add_argument("-l", "--num-lcores",
                    type=int,
                    help="Number of lcores to run.",
                    default=16)
parser.add_argument("-i", "--init-device",
                    action='store_true',
                    help="Initialise PF device with default values.")

args = parser.parse_args()

if not os.path.exists(args.testapp_path):
    print("No such file: " + args.testapp_path)
    sys.exit(1)

params = [args.testapp_path]
if args.eal_params:
    params.extend(shlex.split(args.eal_params))

params.extend(["--"])

if args.snr:
    params.extend(["-s", str(args.snr)])

if args.iter_max:
    params.extend(["-t", str(args.iter_max)])

if args.num_ops:
    params.extend(["-n", str(args.num_ops)])

if args.num_lcores:
    params.extend(["-l", str(args.num_lcores)])

if args.test_cases:
    params.extend(["-c"])
    params.extend([",".join(args.test_cases)])

if args.init_device:
    params.extend(["-i"])


exit_status = 0
for vector in args.test_vector:
    for burst_size in args.burst_size:
        call_params = params[:]
        call_params.extend(["-v", vector])
        call_params.extend(["-b", str(burst_size)])
        params_string = " ".join(call_params)

        print("Executing: {}".format(params_string))
        try:
            output = subprocess.run(call_params, timeout=args.timeout, universal_newlines=True)
        except subprocess.TimeoutExpired as e:
            print("===========================================================")
            print("Starting Test Suite : BBdev TimeOut Tests")
            print("INFO: One of the tests timed out {}".format(e))
            print("INFO: Unexpected Error")
            print("+ ------------------------------------------------------- +")
            print("== test: timeout")
            print("Unexpected Error")
            print("TestCase [ 0] : timeout failed")
            print(" + ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ +")
            print(" + Tests Failed :       1")
            print(" + ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ +")
            exit_status = 1
        if output.returncode < 0:
            print("===========================================================")
            print("Starting Test Suite : BBdev Exception Tests")
            print("INFO: One of the tests returned {}".format(output.returncode))
            print("INFO: Unexpected Error")
            print("+ ------------------------------------------------------- +")
            print("== test: exception")
            print("Unexpected Error")
            print("TestCase [ 0] : exception failed")
            print(" + ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ +")
            print(" + Tests Failed :       1")
            print(" + ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ +")
            exit_status = 1
sys.exit(exit_status)
