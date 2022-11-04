#! /usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2020 Microsoft Corporation
"""Script to query and setup huge pages for DPDK applications."""

import argparse
import glob
import os
import re
import sys
from math import log2

# Standard binary prefix
BINARY_PREFIX = "KMG"

# systemd mount point for huge pages
HUGE_MOUNT = "/dev/hugepages"


def fmt_memsize(kb):
    '''Format memory size in kB into conventional format'''
    logk = int(log2(kb) / 10)
    suffix = BINARY_PREFIX[logk]
    unit = 2**(logk * 10)
    return '{}{}b'.format(int(kb / unit), suffix)


def get_memsize(arg):
    '''Convert memory size with suffix to kB'''
    match = re.match(r'(\d+)([' + BINARY_PREFIX + r']?)$', arg.upper())
    if match is None:
        sys.exit('{} is not a valid size'.format(arg))
    num = float(match.group(1))
    suffix = match.group(2)
    if suffix == "":
        return int(num / 1024)
    idx = BINARY_PREFIX.find(suffix)
    return int(num * (2**(idx * 10)))


def is_numa():
    '''Test if NUMA is necessary on this system'''
    return os.path.exists('/sys/devices/system/node')


def get_valid_page_sizes(path):
    '''Extract valid hugepage sizes'''
    dir = os.path.dirname(path)
    pg_sizes = (d.split("-")[1] for d in os.listdir(dir))
    return " ".join(pg_sizes)


def get_hugepages(path):
    '''Read number of reserved pages'''
    with open(path + '/nr_hugepages') as nr_hugepages:
        return int(nr_hugepages.read())
    return 0


def set_hugepages(path, reqpages):
    '''Write the number of reserved huge pages'''
    filename = path + '/nr_hugepages'
    try:
        with open(filename, 'w') as nr_hugepages:
            nr_hugepages.write('{}\n'.format(reqpages))
    except PermissionError:
        sys.exit('Permission denied: need to be root!')
    except FileNotFoundError:
        sys.exit("Invalid page size. Valid page sizes: {}".format(
                 get_valid_page_sizes(path)))
    gotpages = get_hugepages(path)
    if gotpages != reqpages:
        sys.exit('Unable to set pages ({} instead of {} in {}).'.format(
                 gotpages, reqpages, filename))


def show_numa_pages():
    '''Show huge page reservations on Numa system'''
    print('Node Pages Size Total')
    for numa_path in glob.glob('/sys/devices/system/node/node*'):
        node = numa_path[29:]  # slice after /sys/devices/system/node/node
        path = numa_path + '/hugepages'
        if not os.path.exists(path):
            continue
        for hdir in os.listdir(path):
            pages = get_hugepages(path + '/' + hdir)
            if pages > 0:
                kb = int(hdir[10:-2])  # slice out of hugepages-NNNkB
                print('{:<4} {:<5} {:<6} {}'.format(node, pages,
                                                    fmt_memsize(kb),
                                                    fmt_memsize(pages * kb)))


def show_non_numa_pages():
    '''Show huge page reservations on non Numa system'''
    print('Pages Size Total')
    path = '/sys/kernel/mm/hugepages'
    for hdir in os.listdir(path):
        pages = get_hugepages(path + '/' + hdir)
        if pages > 0:
            kb = int(hdir[10:-2])
            print('{:<5} {:<6} {}'.format(pages, fmt_memsize(kb),
                                          fmt_memsize(pages * kb)))


def show_pages():
    '''Show existing huge page settings'''
    if is_numa():
        show_numa_pages()
    else:
        show_non_numa_pages()


def clear_pages():
    '''Clear all existing huge page mappings'''
    if is_numa():
        dirs = glob.glob(
            '/sys/devices/system/node/node*/hugepages/hugepages-*')
    else:
        dirs = glob.glob('/sys/kernel/mm/hugepages/hugepages-*')

    for path in dirs:
        set_hugepages(path, 0)


def default_pagesize():
    '''Get default huge page size from /proc/meminfo'''
    with open('/proc/meminfo') as meminfo:
        for line in meminfo:
            if line.startswith('Hugepagesize:'):
                return int(line.split()[1])
    return None


def set_numa_pages(pages, hugepgsz, node=None):
    '''Set huge page reservation on Numa system'''
    if node:
        nodes = ['/sys/devices/system/node/node{}/hugepages'.format(node)]
    else:
        nodes = glob.glob('/sys/devices/system/node/node*/hugepages')

    for node_path in nodes:
        huge_path = '{}/hugepages-{}kB'.format(node_path, hugepgsz)
        set_hugepages(huge_path, pages)


def set_non_numa_pages(pages, hugepgsz):
    '''Set huge page reservation on non Numa system'''
    path = '/sys/kernel/mm/hugepages/hugepages-{}kB'.format(hugepgsz)
    set_hugepages(path, pages)


def reserve_pages(pages, hugepgsz, node=None):
    '''Set the number of huge pages to be reserved'''
    if node or is_numa():
        set_numa_pages(pages, hugepgsz, node=node)
    else:
        set_non_numa_pages(pages, hugepgsz)


def get_mountpoints():
    '''Get list of where hugepage filesystem is mounted'''
    mounted = []
    with open('/proc/mounts') as mounts:
        for line in mounts:
            fields = line.split()
            if fields[2] != 'hugetlbfs':
                continue
            mounted.append(fields[1])
    return mounted


def mount_huge(pagesize, mountpoint, user, group):
    '''Mount the huge TLB file system'''
    if mountpoint in get_mountpoints():
        print(mountpoint, "already mounted")
        return
    cmd = "mount -t hugetlbfs"
    if pagesize:
        cmd += ' -o pagesize={}'.format(pagesize * 1024)
    if user:
        cmd += ' -o uid=' + user
    if group:
        cmd += ' -o gid=' + group
    cmd += ' nodev ' + mountpoint
    os.system(cmd)


def umount_huge(mountpoint):
    '''Unmount the huge TLB file system (if mounted)'''
    if mountpoint in get_mountpoints():
        os.system("umount " + mountpoint)


def show_mount():
    '''Show where huge page filesystem is mounted'''
    mounted = get_mountpoints()
    if mounted:
        print("Hugepages mounted on", *mounted)
    else:
        print("Hugepages not mounted")


def main():
    '''Process the command line arguments and setup huge pages'''
    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description="Setup huge pages",
        epilog="""
Examples:

To display current huge page settings:
    %(prog)s -s

To a complete setup of with 2 Gigabyte of 1G huge pages:
    %(prog)s -p 1G --setup 2G
""")
    parser.add_argument(
        '--show',
        '-s',
        action='store_true',
        help="print the current huge page configuration")
    parser.add_argument(
        '--clear', '-c', action='store_true', help="clear existing huge pages")
    parser.add_argument(
        '--mount',
        '-m',
        action='store_true',
        help='mount the huge page filesystem')
    parser.add_argument(
        '--unmount',
        '-u',
        action='store_true',
        help='unmount the system huge page directory')
    parser.add_argument(
        '--directory',
        '-d',
        metavar='DIR',
        default=HUGE_MOUNT,
        help='mount point')
    parser.add_argument(
        '--user',
        '-U',
        metavar='UID',
        help='set the mounted directory owner user')
    parser.add_argument(
        '--group',
        '-G',
        metavar='GID',
        help='set the mounted directory owner group')
    parser.add_argument(
        '--node', '-n', help='select numa node to reserve pages on')
    parser.add_argument(
        '--pagesize',
        '-p',
        metavar='SIZE',
        help='choose huge page size to use')
    parser.add_argument(
        '--reserve',
        '-r',
        metavar='SIZE',
        help='reserve huge pages. Size is in bytes with K, M, or G suffix')
    parser.add_argument(
        '--setup',
        metavar='SIZE',
        help='setup huge pages by doing clear, unmount, reserve and mount')
    args = parser.parse_args()

    if args.setup:
        args.clear = True
        args.unmount = True
        args.reserve = args.setup
        args.mount = True

    if not (args.show or args.mount or args.unmount or args.clear or args.reserve):
        parser.error("no action specified")

    if args.pagesize:
        pagesize_kb = get_memsize(args.pagesize)
    else:
        pagesize_kb = default_pagesize()
    if not pagesize_kb:
        sys.exit("Invalid page size: {}kB".format(pagesize_kb))

    if args.clear:
        clear_pages()
    if args.unmount:
        umount_huge(args.directory)

    if args.reserve:
        reserve_kb = get_memsize(args.reserve)
        if reserve_kb % pagesize_kb != 0:
            sys.exit(
                'Huge reservation {}kB is not a multiple of page size {}kB'.
                format(reserve_kb, pagesize_kb))
        reserve_pages(
            int(reserve_kb / pagesize_kb), pagesize_kb, node=args.node)
    if args.mount:
        mount_huge(pagesize_kb, args.directory, args.user, args.group)
    if args.show:
        show_pages()
        print()
        show_mount()


if __name__ == "__main__":
    main()
