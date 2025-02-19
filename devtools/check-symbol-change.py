#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2025 Red Hat, Inc.

"""Check exported symbols change in a patch."""

import argparse
import re

file_header_regexp = re.compile(r"^(\-\-\-|\+\+\+) [ab]/(lib|drivers)/([^/]+)/([^/]+)")
# From eal_exports.h
export_exp_sym_regexp = re.compile(r"^.RTE_EXPORT_EXPERIMENTAL_SYMBOL\(([^,]+),")
export_int_sym_regexp = re.compile(r"^.RTE_EXPORT_INTERNAL_SYMBOL\(([^)]+)\)")
export_sym_regexp = re.compile(r"^.RTE_EXPORT_SYMBOL\(([^)]+)\)")

parser = argparse.ArgumentParser(
    description=__doc__,
    formatter_class=argparse.RawDescriptionHelpFormatter,
)
parser.add_argument(
    "--patch",
    nargs="+",
    required=True,
    type=argparse.FileType("r", encoding="utf-8"),
    help="Input patch to parse",
)
args = parser.parse_args()

symbols = {}

for file in args.patch:
    for ln in file.readlines():
        if file_header_regexp.match(ln):
            if file_header_regexp.match(ln).group(2) == "lib":
                lib = "/".join(file_header_regexp.match(ln).group(2, 3))
            elif file_header_regexp.match(ln).group(3) == "intel":
                lib = "/".join(file_header_regexp.match(ln).group(2, 3, 4))
            else:
                lib = "/".join(file_header_regexp.match(ln).group(2, 3))

            if lib not in symbols:
                symbols[lib] = {}
            continue

        if export_exp_sym_regexp.match(ln):
            symbol = export_exp_sym_regexp.match(ln).group(1)
            node = "EXPERIMENTAL"
        elif export_int_sym_regexp.match(ln):
            node = "INTERNAL"
            symbol = export_int_sym_regexp.match(ln).group(1)
        elif export_sym_regexp.match(ln):
            symbol = export_sym_regexp.match(ln).group(1)
            node = "stable"
        else:
            continue

        if symbol not in symbols[lib]:
            symbols[lib][symbol] = {}
        added = ln[0] == "+"
        if (
            added
            and "added" in symbols[lib][symbol]
            and node != symbols[lib][symbol]["added"]
        ):
            print(f"{symbol} in {lib} was found in multiple ABI, please check.")
        if (
            not added
            and "removed" in symbols[lib][symbol]
            and node != symbols[lib][symbol]["removed"]
        ):
            print(f"{symbol} in {lib} was found in multiple ABI, please check.")
        if added:
            symbols[lib][symbol]["added"] = node
        else:
            symbols[lib][symbol]["removed"] = node

    for lib in sorted(symbols.keys()):
        error = False
        for symbol in sorted(symbols[lib].keys()):
            if "removed" not in symbols[lib][symbol]:
                # Symbol addition
                node = symbols[lib][symbol]["added"]
                if node == "stable":
                    print(
                        f"ERROR: {symbol} in {lib} has been added directly to stable ABI."
                    )
                    error = True
                else:
                    print(f"INFO: {symbol} in {lib} has been added to {node} ABI.")
                continue

            if "added" not in symbols[lib][symbol]:
                # Symbol removal
                node = symbols[lib][symbol]["removed"]
                if node == "stable":
                    print(f"INFO: {symbol} in {lib} has been removed from stable ABI.")
                    print("Please check it has gone though the deprecation process.")
                continue

            if symbols[lib][symbol]["added"] == symbols[lib][symbol]["removed"]:
                # Symbol was moved around
                continue

            # Symbol modifications
            added = symbols[lib][symbol]["added"]
            removed = symbols[lib][symbol]["removed"]
            print(f"INFO: {symbol} in {lib} is moving from {removed} to {added}")
            print("Please check it has gone though the deprecation process.")
