#! /usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 Intel Corporation

import sys
import re

input_list = sys.argv[1:]
test_def_regex = re.compile(
        r"^\s*REGISTER_([A-Z]+)_TEST\s*\(\s*([a-z0-9_]+)[^)]*\)", re.MULTILINE)
test_suites = {}
# track tests not in any test suite.
non_suite_regex = re.compile(
        r"^\s*REGISTER_TEST_COMMAND\s*\(\s*([a-z0-9_]+)[^)]*\)", re.MULTILINE)
non_suite_tests = []

def get_fast_test_params(test_name, ln):
    "Extract the extra fast-test parameters from the line"
    (_, rest_of_line) = ln.split(test_name, 1)
    (_, nohuge, asan, _func) = rest_of_line.split(',', 3)
    return f":{nohuge.strip()}:{asan.strip()}"

for fname in input_list:
    with open(fname, "r", encoding="utf-8") as f:
        contents = f.read()
    non_suite_tests.extend(
            match.group(1) for match in non_suite_regex.finditer(contents))
    for match in test_def_regex.finditer(contents):
        (test_suite, test_name) = match.group(1, 2)
        suite_name = f"{test_suite.lower()}-tests"
        if suite_name in test_suites:
            test_suites[suite_name].append(test_name)
        else:
            test_suites[suite_name] = [test_name]
        if suite_name == "fast-tests":
            test_suites["fast-tests"][-1] += get_fast_test_params(
                    test_name, match.group(0))

for suite in test_suites.keys():
    print(f"{suite}={','.join(test_suites[suite])}")
if non_suite_tests:
    print(f"non_suite_tests={','.join(non_suite_tests)}")
