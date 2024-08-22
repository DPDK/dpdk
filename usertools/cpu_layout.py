#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation
# Copyright(c) 2017 Cavium, Inc. All rights reserved.

"""Display CPU topology information."""

import glob
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


def read_numa_node(base: str) -> int:
    """Read the NUMA node of a CPU."""
    node_glob = f"{base}/node*"
    node_dirs = glob.glob(node_glob)
    if not node_dirs:
        return 0  # default to node 0
    return int(node_dirs[0].split("node")[1])


def print_row(row: T.Tuple[str, ...], col_widths: T.List[int]) -> None:
    """Print a row of a table with the given column widths."""
    first, *rest = row
    w_first, *w_rest = col_widths
    first_end = " " * 4
    rest_end = " " * 4

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
    numa_map: T.Dict[int, int] = {}
    base_path = "/sys/devices/system/cpu"

    cpus = range_expand(read_sysfs(f"{base_path}/online"))

    for cpu in cpus:
        lcore_base = f"{base_path}/cpu{cpu}"
        core = int(read_sysfs(f"{lcore_base}/topology/core_id"))
        socket = int(read_sysfs(f"{lcore_base}/topology/physical_package_id"))
        node = read_numa_node(lcore_base)

        cores_s.add(core)
        sockets_s.add(socket)
        key = (socket, core)
        core_map.setdefault(key, [])
        core_map[key].append(cpu)
        numa_map[cpu] = node

    cores = sorted(cores_s)
    sockets = sorted(sockets_s)

    print_section(f"Core and Socket Information (as reported by '{base_path}')")

    print("cores = ", cores)
    print("sockets = ", sockets)
    print("numa = ", sorted(set(numa_map.values())))
    print()

    # Core, [NUMA, Socket, NUMA, Socket, ...]
    heading_strs = "", *[v for s in sockets for v in ("", f"Socket {s}")]
    sep_strs = tuple("-" * len(hstr) for hstr in heading_strs)
    rows: T.List[T.Tuple[str, ...]] = []

    # track NUMA changes per socket
    prev_numa: T.Dict[int, T.Optional[int]] = {socket: None for socket in sockets}
    for c in cores:
        # Core,
        row: T.Tuple[str, ...] = (f"Core {c}",)

        # [NUMA, lcores, NUMA, lcores, ...]
        for s in sockets:
            try:
                lcores = core_map[(s, c)]

                numa = numa_map[lcores[0]]
                numa_changed = prev_numa[s] != numa
                prev_numa[s] = numa

                if numa_changed:
                    row += (f"NUMA {numa}",)
                else:
                    row += ("",)
                row += (str(lcores),)
            except KeyError:
                row += ("", "")
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
