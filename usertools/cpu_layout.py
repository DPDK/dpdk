#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation
# Copyright(c) 2017 Cavium, Inc. All rights reserved.

"""Display CPU topology information."""

import typing as T


def range_expand(rstr: str) -> T.List[int]:
    """Expand a range string into a list of integers."""
    # 0,1-3 => [0, 1-3]
    ranges = rstr.split(",")
    valset: T.List[int] = []
    for r in ranges:
        # 1-3 => [1, 2, 3]
        if "-" in r:
            start, end = r.split("-")
            valset.extend(range(int(start), int(end) + 1))
        else:
            valset.append(int(r))
    return valset


def read_sysfs(path: str) -> str:
    """Read a sysfs file and return its contents."""
    with open(path, encoding="utf-8") as fd:
        return fd.read().strip()


def print_row(row: T.Tuple[str, ...], col_widths: T.List[int]) -> None:
    """Print a row of a table with the given column widths."""
    first, *rest = row
    w_first, *w_rest = col_widths
    first_end = " " * 4
    rest_end = " " * 10

    print(first.ljust(w_first), end=first_end)
    for cell, width in zip(rest, w_rest):
        print(cell.rjust(width), end=rest_end)
    print()


def print_section(heading: str) -> None:
    """Print a section heading."""
    sep = "=" * len(heading)
    print(sep)
    print(heading)
    print(sep)
    print()


def main() -> None:
    """Print CPU topology information."""
    sockets_s: T.Set[int] = set()
    cores_s: T.Set[int] = set()
    core_map: T.Dict[T.Tuple[int, int], T.List[int]] = {}
    base_path = "/sys/devices/system/cpu"

    cpus = range_expand(read_sysfs(f"{base_path}/online"))

    for cpu in cpus:
        lcore_base = f"{base_path}/cpu{cpu}"
        core = int(read_sysfs(f"{lcore_base}/topology/core_id"))
        socket = int(read_sysfs(f"{lcore_base}/topology/physical_package_id"))

        cores_s.add(core)
        sockets_s.add(socket)
        key = (socket, core)
        core_map.setdefault(key, [])
        core_map[key].append(cpu)

    cores = sorted(cores_s)
    sockets = sorted(sockets_s)

    print_section(f"Core and Socket Information (as reported by '{base_path}')")

    print("cores = ", cores)
    print("sockets = ", sockets)
    print()

    # Core, [Socket, Socket, ...]
    heading_strs = "", *[f"Socket {s}" for s in sockets]
    sep_strs = tuple("-" * len(hstr) for hstr in heading_strs)
    rows: T.List[T.Tuple[str, ...]] = []

    for c in cores:
        # Core,
        row: T.Tuple[str, ...] = (f"Core {c}",)

        # [lcores, lcores, ...]
        for s in sockets:
            try:
                lcores = core_map[(s, c)]
                row += (str(lcores),)
            except KeyError:
                row += ("",)
        rows += [row]

    # find max widths for each column, including header and rows
    col_widths = [
        max(len(tup[col_idx]) for tup in rows + [heading_strs])
        for col_idx in range(len(heading_strs))
    ]

    # print out table taking row widths into account
    print_row(heading_strs, col_widths)
    print_row(sep_strs, col_widths)
    for row in rows:
        print_row(row, col_widths)


if __name__ == "__main__":
    main()
