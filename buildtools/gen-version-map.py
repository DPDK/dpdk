#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2025 Red Hat, Inc.

"""Generate a version map file used by GNU or MSVC linker."""

import argparse
import re

# From eal_export.h
export_exp_sym_regexp = re.compile(
    r"^RTE_EXPORT_EXPERIMENTAL_SYMBOL\(([^,]+), ([0-9]+.[0-9]+)\)"
)
export_int_sym_regexp = re.compile(r"^RTE_EXPORT_INTERNAL_SYMBOL\(([^)]+)\)")
export_sym_regexp = re.compile(r"^RTE_EXPORT_SYMBOL\(([^)]+)\)")
ver_sym_regexp = re.compile(r"^RTE_VERSION_SYMBOL\(([^,]+), [^,]+, ([^,]+),")
ver_exp_sym_regexp = re.compile(r"^RTE_VERSION_EXPERIMENTAL_SYMBOL\([^,]+, ([^,]+),")
default_sym_regexp = re.compile(r"^RTE_DEFAULT_SYMBOL\(([^,]+), [^,]+, ([^,]+),")

parser = argparse.ArgumentParser(
    description=__doc__,
    formatter_class=argparse.RawDescriptionHelpFormatter,
)
parser.add_argument(
    "--linker", choices=["gnu", "mingw", "mslinker", ], required=True, help="Linker type"
)
parser.add_argument(
    "--abi-version",
    required=True,
    type=argparse.FileType("r", encoding="utf-8"),
    help="ABI version file",
)
parser.add_argument(
    "--output",
    required=True,
    type=argparse.FileType("w", encoding="utf-8"),
    help="Output file",
)
parser.add_argument(
    "--source",
    nargs="+",
    required=True,
    type=argparse.FileType("r", encoding="utf-8"),
    help="Input source to parse",
)
args = parser.parse_args()

ABI_MAJOR = re.match("([0-9]+).[0-9]", args.abi_version.readline()).group(1)
ABI = f"DPDK_{ABI_MAJOR}"

symbols = {}

for file in args.source:
    for ln in file.readlines():
        node = None
        symbol = None
        comment = ""
        if export_exp_sym_regexp.match(ln):
            node = "EXPERIMENTAL"
            symbol = export_exp_sym_regexp.match(ln).group(1)
            version = export_exp_sym_regexp.match(ln).group(2)
            comment = f" # added in {version}"
        elif export_int_sym_regexp.match(ln):
            node = "INTERNAL"
            symbol = export_int_sym_regexp.match(ln).group(1)
        elif export_sym_regexp.match(ln):
            node = ABI
            symbol = export_sym_regexp.match(ln).group(1)
        elif ver_sym_regexp.match(ln):
            abi = ver_sym_regexp.match(ln).group(1)
            node = f"DPDK_{abi}"
            symbol = ver_sym_regexp.match(ln).group(2)
        elif ver_exp_sym_regexp.match(ln):
            node = "EXPERIMENTAL"
            symbol = ver_exp_sym_regexp.match(ln).group(1)
        elif default_sym_regexp.match(ln):
            abi = default_sym_regexp.match(ln).group(1)
            node = f"DPDK_{abi}"
            symbol = default_sym_regexp.match(ln).group(2)

        if not symbol:
            continue

        if node not in symbols:
            symbols[node] = {}
        symbols[node][symbol] = comment

if args.linker == "mslinker":
    print("EXPORTS", file=args.output)
    for key in (ABI, "EXPERIMENTAL", "INTERNAL"):
        if key not in symbols:
            continue
        for symbol in sorted(symbols[key].keys()):
            print(f"\t{symbol}", file=args.output)
        del symbols[key]
else:  # GNU format
    local_token = False
    for key in (ABI, "EXPERIMENTAL", "INTERNAL"):
        if key not in symbols:
            continue
        print(f"{key} {{\n\tglobal:\n", file=args.output)
        for symbol in sorted(symbols[key].keys()):
            if args.linker == "mingw" and symbol.startswith("per_lcore"):
                prefix = "__emutls_v."
            else:
                prefix = ""
            comment = symbols[key][symbol]
            print(f"\t{prefix}{symbol};{comment}", file=args.output)
        if not local_token:
            print("\n\tlocal: *;", file=args.output)
            local_token = True
        print("};", file=args.output)
        del symbols[key]
    for key in sorted(symbols.keys()):
        print(f"{key} {{\n\tglobal:\n", file=args.output)
        for symbol in sorted(symbols[key].keys()):
            if args.linker == "mingw" and symbol.startswith("per_lcore"):
                prefix = "__emutls_v."
            else:
                prefix = ""
            comment = symbols[key][symbol]
            print(f"\t{prefix}{symbol};{comment}", file=args.output)
        print(f"}} {ABI};", file=args.output)
        if not local_token:
            print("\n\tlocal: *;", file=args.output)
            local_token = True
        del symbols[key]
    # No exported symbol, add a catch all
    if not local_token:
        print(f"{ABI} {{", file=args.output)
        print("\n\tlocal: *;", file=args.output)
        local_token = True
        print("};", file=args.output)
