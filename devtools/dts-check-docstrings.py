#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2025 University of New Hampshire

import sys
from ast import FunctionDef, Name, walk, get_docstring, parse
from pathlib import Path

BASE_DIR = Path(__file__).resolve().parent.parent  # dpdk/
TESTS_DIR = BASE_DIR / "dts" / "tests"  # dts/tests/
DECORATOR_NAMES = {"func_test", "perf_test"}


def has_test_decorator(node: FunctionDef) -> bool:
    """Return True if function has @func_test or @perf_test decorator."""
    for decorator in node.decorator_list:
        if isinstance(decorator, Name) and decorator.id in DECORATOR_NAMES:
            return True
    return False


def check_file(path: Path) -> bool:
    """Return True if file has steps and verify sections in each test case docstring."""
    source = path.read_text(encoding="utf-8")
    tree = parse(source, filename=str(path))
    ok = True

    for node in walk(tree):
        if isinstance(node, FunctionDef):
            if has_test_decorator(node):
                doc = get_docstring(node)
                if not doc:
                    print(f"{path}:{node.lineno} missing docstring for test case")
                    ok = False
                else:
                    if "Steps:" not in doc:
                        print(f"{path}:{node.lineno} missing 'Steps:' section")
                        ok = False
                    if "Verify:" not in doc:
                        print(f"{path}:{node.lineno} missing 'Verify:' section")
                        ok = False
    return ok


def main():
    all_ok = True
    for path in TESTS_DIR.rglob("*.py"):
        if not check_file(path):
            all_ok = False
    sys.exit(0 if all_ok else 1)


if __name__ == "__main__":
    main()
