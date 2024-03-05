#! /usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 Intel Corporation

import sys
import re

input_list = sys.argv[1:]
test_def_regex = re.compile("REGISTER_([A-Z]+)_TEST\s*\(\s*([a-z0-9_]+)")
test_suites = {}
# track tests not in any test suite.
non_suite_regex = re.compile("REGISTER_TEST_COMMAND\s*\(\s*([a-z0-9_]+)")
non_suite_tests = []

def get_fast_test_params(test_name, ln):
    "Extract the extra fast-test parameters from the line"
    (_, rest_of_line) = ln.split(test_name, 1)
    (_, nohuge, asan, _func) = rest_of_line.split(',', 3)
    return f":{nohuge.strip().lower()}:{asan.strip().lower()}"

for fname in input_list:
    with open(fname, "r", encoding="utf-8") as f:
        contents = [ln.strip() for ln in f.readlines()]
        test_lines = [ln for ln in contents if test_def_regex.match(ln)]
        non_suite_tests.extend([non_suite_regex.match(ln).group(1)
                for ln in contents if non_suite_regex.match(ln)])
    for ln in test_lines:
        (test_suite, test_name) = test_def_regex.match(ln).group(1, 2)
        suite_name = f"{test_suite.lower()}-tests"
        if suite_name in test_suites:
            test_suites[suite_name].append(test_name)
        else:
            test_suites[suite_name] = [test_name]
        if suite_name == "fast-tests":
            test_suites["fast-tests"][-1] += get_fast_test_params(test_name, ln)

for suite in test_suites.keys():
    print(f"{suite}={','.join(test_suites[suite])}")
print(f"non_suite_tests={','.join(non_suite_tests)}")
