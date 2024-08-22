#! /usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2020 Microsoft Corporation

"""Script to query and setup huge pages for DPDK applications."""

import argparse
import os
import re
import subprocess
import sys
import typing as T
from math import log2

# Standard binary prefix
BINARY_PREFIX = "KMG"

# systemd mount point for huge pages
HUGE_MOUNT = "/dev/hugepages"
# default directory for non-NUMA huge pages
NO_NUMA_HUGE_DIR = "/sys/kernel/mm/hugepages"
# default base directory for NUMA nodes
NUMA_NODE_BASE_DIR = "/sys/devices/system/node"
# procfs paths
MEMINFO_PATH = "/proc/meminfo"
MOUNTS_PATH = "/proc/mounts"


class HugepageMount:
    """Mount operations for huge page filesystem."""

    def __init__(self, path: str, mounted: bool):
        self.path = path
        # current mount status
        self.mounted = mounted

    def mount(
        self, pagesize_kb: int, user: T.Optional[str], group: T.Optional[str]
    ) -> None:
        """Mount the huge TLB file system"""
        if self.mounted:
            return
        cmd = ["mount", "-t", "hugetlbfs"]
        cmd += ["-o", f"pagesize={pagesize_kb * 1024}"]
        if user is not None:
            cmd += ["-o", f"uid={user}"]
        if group is not None:
            cmd += ["-o", f"gid={group}"]
        cmd += ["nodev", self.path]

        subprocess.run(cmd, check=True)
        self.mounted = True

    def unmount(self) -> None:
        """Unmount the huge TLB file system (if mounted)"""
        if self.mounted:
            subprocess.run(["umount", self.path], check=True)
            self.mounted = False


class HugepageRes:
    """Huge page reserve operations. Can be NUMA-node-specific."""

    def __init__(self, path: str, node: T.Optional[int] = None):
        self.path = path
        # if this is a per-NUMA node huge page dir, store the node number
        self.node = node
        self.valid_page_sizes = self._get_valid_page_sizes()

    def _get_valid_page_sizes(self) -> T.List[int]:
        """Extract valid huge page sizes"""
        return [get_memsize(d.split("-")[1]) for d in os.listdir(self.path)]

    def _nr_pages_path(self, sz: int) -> str:
        if sz not in self.valid_page_sizes:
            raise ValueError(
                f"Invalid page size {sz}. " f"Valid sizes: {self.valid_page_sizes}"
            )
        return os.path.join(self.path, f"hugepages-{sz}kB", "nr_hugepages")

    def __getitem__(self, sz: int) -> int:
        """Get current number of reserved pages of specified size"""
        with open(self._nr_pages_path(sz), encoding="utf-8") as f:
            return int(f.read())

    def __setitem__(self, sz: int, nr_pages: int) -> None:
        """Set number of reserved pages of specified size"""
        with open(self._nr_pages_path(sz), "w", encoding="utf-8") as f:
            f.write(f"{nr_pages}\n")


def fmt_memsize(kb: int) -> str:
    """Format memory size in kB into conventional format"""
    logk = int(log2(kb) / 10)
    suffix = BINARY_PREFIX[logk]
    unit = 2 ** (logk * 10)
    return f"{int(kb / unit)}{suffix}b"


def get_memsize(arg: str) -> int:
    """Convert memory size with suffix to kB"""
    # arg may have a 'b' at the end
    if arg[-1].lower() == "b":
        arg = arg[:-1]
    match = re.match(rf"(\d+)([{BINARY_PREFIX}]?)$", arg.upper())
    if match is None:
        raise ValueError(f"{arg} is not a valid size")
    num = float(match.group(1))
    suffix = match.group(2)
    if not suffix:
        return int(num / 1024)
    idx = BINARY_PREFIX.find(suffix)
    return int(num * (2 ** (idx * 10)))


def is_numa() -> bool:
    """Check if NUMA is supported"""
    return os.path.exists(NUMA_NODE_BASE_DIR)


def default_pagesize() -> int:
    """Get default huge page size from /proc/meminfo"""
    key = "Hugepagesize"
    with open(MEMINFO_PATH, encoding="utf-8") as meminfo:
        for line in meminfo:
            if line.startswith(f"{key}:"):
                return int(line.split()[1])
    raise KeyError(f'"{key}" not found in {MEMINFO_PATH}')


def get_hugetlbfs_mountpoints() -> T.List[str]:
    """Get list of where huge page filesystem is mounted"""
    mounted: T.List[str] = []
    with open(MOUNTS_PATH, encoding="utf-8") as mounts:
        for line in mounts:
            fields = line.split()
            if fields[2] != "hugetlbfs":
                continue
            mounted.append(fields[1])
    return mounted


def print_row(cells: T.Tuple[str, ...], widths: T.List[int]) -> None:
    """Print a row of a table with the given column widths"""
    first, *rest = cells
    w_first, *w_rest = widths
    first_end = " " * 2
    rest_end = " " * 2

    print(first.ljust(w_first), end=first_end)
    for cell, width in zip(rest, w_rest):
        print(cell.rjust(width), end=rest_end)
    print()


def print_hp_status(hp_res: T.List[HugepageRes]) -> None:
    """Display status of huge page reservations"""
    numa = is_numa()

    # print out huge page information in a table
    rows: T.List[T.Tuple[str, ...]]
    headers: T.Tuple[str, ...]
    if numa:
        headers = "Node", "Pages", "Size", "Total"
        rows = [
            (
                str(hp.node),
                str(nr_pages),
                fmt_memsize(sz),
                fmt_memsize(sz * nr_pages),
            )
            # iterate over each huge page sysfs node...
            for hp in hp_res
            # ...and each page size within that node...
            for sz in hp.valid_page_sizes
            # ...we need number of pages multiple times, so we read it here...
            for nr_pages in [hp[sz]]
            # ...include this row only if there are pages reserved
            if nr_pages
        ]
    else:
        headers = "Pages", "Size", "Total"
        # if NUMA is disabled, we know there's only one huge page dir
        hp = hp_res[0]
        rows = [
            (str(nr_pages), fmt_memsize(sz), fmt_memsize(sz * nr_pages))
            # iterate over each page size within the huge page dir
            for sz in hp.valid_page_sizes
            # read number of pages for this size
            for nr_pages in [hp[sz]]
            # skip if no pages
            if nr_pages
        ]
    if not rows:
        print("No huge pages reserved")
        return

    # find max widths for each column, including header and rows
    col_widths = [
        max(len(tup[col_idx]) for tup in rows + [headers])
        for col_idx in range(len(headers))
    ]

    # print everything
    print_row(headers, col_widths)
    for r in rows:
        print_row(r, col_widths)


def print_mount_status() -> None:
    """Display status of huge page filesystem mounts"""
    mounted = get_hugetlbfs_mountpoints()
    if not mounted:
        print("No huge page filesystems mounted")
        return
    print("Huge page filesystems mounted at:", *mounted, sep=" ")


def scan_huge_dirs(node: T.Optional[int]) -> T.List[HugepageRes]:
    """Return a HugepageRes object for each huge page directory"""
    # if NUMA is enabled, scan per-NUMA node huge pages
    if is_numa():
        # helper function to extract node number from directory name
        def _get_node(path: str) -> T.Optional[int]:
            m = re.match(r"node(\d+)", os.path.basename(path))
            return int(m.group(1)) if m else None

        # we want a sorted list of NUMA nodes
        nodes = sorted(
            n
            # iterate over all directories in the base directory
            for d in os.listdir(NUMA_NODE_BASE_DIR)
            # extract the node number from the directory name
            for n in [_get_node(d)]
            # filter out None values (non-NUMA node directories)
            if n is not None
        )
        return [
            HugepageRes(os.path.join(NUMA_NODE_BASE_DIR, f"node{n}", "hugepages"), n)
            for n in nodes
            # if user requested a specific node, only include that one
            if node is None or n == node
        ]
    # otherwise, use non-NUMA huge page directory
    if node is not None:
        raise ValueError("NUMA node requested but not supported")
    return [HugepageRes(NO_NUMA_HUGE_DIR)]


def try_reserve_huge_pages(
    hp_res: T.List[HugepageRes], mem_sz: str, pagesize_kb: int
) -> None:
    """Reserve huge pages if possible"""
    reserve_kb = get_memsize(mem_sz)

    # is this a valid request?
    if reserve_kb % pagesize_kb != 0:
        fmt_res = fmt_memsize(reserve_kb)
        fmt_sz = fmt_memsize(pagesize_kb)
        raise ValueError(
            f"Huge reservation {fmt_res} is " f"not a multiple of page size {fmt_sz}"
        )

    # request is valid, reserve pages
    for hp in hp_res:
        req = reserve_kb // pagesize_kb
        hp[pagesize_kb] = req
        got = hp[pagesize_kb]
        # did we fulfill our request?
        if got != req:
            raise OSError(
                f"Failed to reserve {req} pages of size "
                f"{fmt_memsize(pagesize_kb)}, "
                f"got {got} pages instead"
            )


def main():
    """Process the command line arguments and setup huge pages"""
    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description="Setup huge pages",
        epilog="""
Examples:

To display current huge page settings:
    %(prog)s -s

To a complete setup of with 2 Gigabyte of 1G huge pages:
    %(prog)s -p 1G --setup 2G
""",
    )
    parser.add_argument(
        "--show",
        "-s",
        action="store_true",
        help="Print current huge page configuration",
    )
    parser.add_argument(
        "--clear", "-c", action="store_true", help="Clear existing huge pages"
    )
    parser.add_argument(
        "--mount",
        "-m",
        action="store_true",
        help="Mount the huge page filesystem",
    )
    parser.add_argument(
        "--unmount",
        "-u",
        action="store_true",
        help="Unmount the system huge page directory",
    )
    parser.add_argument(
        "--directory",
        "-d",
        metavar="DIR",
        default=HUGE_MOUNT,
        help="Mount point for huge pages",
    )
    parser.add_argument(
        "--user",
        "-U",
        metavar="UID",
        help="Set the mounted directory owner user",
    )
    parser.add_argument(
        "--group",
        "-G",
        metavar="GID",
        help="Set the mounted directory owner group",
    )
    parser.add_argument(
        "--node", "-n", type=int, help="Select numa node to reserve pages on"
    )
    parser.add_argument(
        "--pagesize", "-p", metavar="SIZE", help="Choose huge page size to use"
    )
    parser.add_argument(
        "--reserve",
        "-r",
        metavar="SIZE",
        help="Reserve huge pages. Size is in bytes with K, M, or G suffix",
    )
    parser.add_argument(
        "--setup",
        metavar="SIZE",
        help="Setup huge pages by doing clear, unmount, reserve and mount",
    )
    args = parser.parse_args()

    # setup is clear, then unmount, then reserve, then mount
    if args.setup:
        args.clear = True
        args.unmount = True
        args.reserve = args.setup
        args.mount = True

    if not (args.show or args.mount or args.unmount or args.clear or args.reserve):
        parser.error("no action specified")

    # read huge page data from sysfs
    hp_res = scan_huge_dirs(args.node)

    # read huge page mountpoint data
    hp_mountpoint = args.directory
    hp_mounted = hp_mountpoint in get_hugetlbfs_mountpoints()
    hp_mount = HugepageMount(hp_mountpoint, hp_mounted)

    # get requested page size we will be working with
    if args.pagesize:
        pagesize_kb = get_memsize(args.pagesize)
    else:
        pagesize_kb = default_pagesize()

    # were we asked to clear?
    if args.clear:
        for hp in hp_res:
            for sz in hp.valid_page_sizes:
                hp[sz] = 0

    # were we asked to unmount?
    if args.unmount:
        hp_mount.unmount()

    # were we asked to reserve pages?
    if args.reserve:
        try_reserve_huge_pages(hp_res, args.reserve, pagesize_kb)

    # were we asked to mount?
    if args.mount:
        hp_mount.mount(pagesize_kb, args.user, args.group)

    # were we asked to display status?
    if args.show:
        print_hp_status(hp_res)
        print()
        print_mount_status()


if __name__ == "__main__":
    try:
        main()
    except PermissionError:
        sys.exit("Permission denied: need to be root!")
    except subprocess.CalledProcessError as e:
        sys.exit(f"Command failed: {e}")
    except (KeyError, ValueError, OSError) as e:
        sys.exit(f"Error: {e}")
