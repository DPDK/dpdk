#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2026 Kevin Bowling

"""Exercise an idle FreeBSD nic_uio device; requires root and MSI-X hardware."""

import argparse
import ctypes
import errno
import fcntl
import mmap
import os
import signal
import subprocess

INIT, ENABLE, DISABLE = (0x20004E00 + i for i in (1, 2, 3))
LIBC = ctypes.CDLL(None, use_errno=True)
LIBC.mmap.argtypes = (
    ctypes.c_void_p,
    ctypes.c_size_t,
    ctypes.c_int,
    ctypes.c_int,
    ctypes.c_int,
    ctypes.c_int64,
)
LIBC.mmap.restype = ctypes.c_void_p
LIBC.munmap.argtypes = (ctypes.c_void_p, ctypes.c_size_t)


def expect_errno(fd, command, expected):
    try:
        fcntl.ioctl(fd, command)
    except OSError as error:
        assert error.errno == expected, error
    else:
        raise AssertionError(f"ioctl {command:#x} unexpectedly succeeded")


def busmaster(pci, enabled):
    command = int(subprocess.check_output(["pciconf", "-r", "-h", pci, "4"], text=True).strip(), 16)
    assert command != 0xFFFF and bool(command & 4) == enabled, hex(command)


def lifecycle(device, pci):
    busmaster(pci, False)
    readonly = os.open(device, os.O_RDONLY | os.O_NONBLOCK)
    try:
        busmaster(pci, False)
        for command in (INIT, ENABLE, DISABLE):
            expect_errno(readonly, command, errno.EBADF)
        expect_errno(readonly, 0x20004EFF, errno.ENOTTY)

        owner = os.open(device, os.O_RDWR | os.O_NONBLOCK)
        try:
            busmaster(pci, True)
            fcntl.ioctl(owner, INIT)
            fcntl.ioctl(owner, ENABLE)
            mapping_fd = os.open(device, os.O_RDWR)
            try:
                # Python's mmap object retains a duplicate fd. Use the
                # syscall directly to test the mapping's kernel lifetime.
                mapping = LIBC.mmap(
                    None,
                    mmap.PAGESIZE,
                    mmap.PROT_READ | mmap.PROT_WRITE,
                    mmap.MAP_SHARED,
                    mapping_fd,
                    0,
                )
                assert mapping != ctypes.c_void_p(-1).value, ctypes.get_errno()
            finally:
                os.close(mapping_fd)
            try:
                busmaster(pci, True)
                duplicate = os.dup(owner)
                try:
                    os.close(owner)
                    owner = -1
                    busmaster(pci, True)
                    other = os.open(device, os.O_RDWR)
                    try:
                        expect_errno(other, INIT, errno.EBUSY)
                        os.close(duplicate)
                        duplicate = -1
                        busmaster(pci, True)
                        fcntl.ioctl(other, INIT)
                        fcntl.ioctl(other, ENABLE)
                    finally:
                        os.close(other)
                finally:
                    if duplicate >= 0:
                        os.close(duplicate)
                # Neither an mmap nor a read-only descriptor retains DMA.
                busmaster(pci, False)
            finally:
                assert LIBC.munmap(mapping, mmap.PAGESIZE) == 0
        finally:
            if owner >= 0:
                os.close(owner)

        owner = os.open(device, os.O_RDWR)
        try:
            busmaster(pci, True)
            fcntl.ioctl(owner, INIT)
            fcntl.ioctl(owner, ENABLE)
            child = os.fork()
            if child == 0:
                while True:
                    signal.pause()
            try:
                os.close(owner)
                owner = -1
                busmaster(pci, True)
            finally:
                os.kill(child, signal.SIGKILL)
                _, status = os.waitpid(child, 0)
                assert os.WIFSIGNALED(status)
            busmaster(pci, False)
        finally:
            if owner >= 0:
                os.close(owner)
    finally:
        os.close(readonly)
    busmaster(pci, False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("device", help="idle nic_uio device node")
    parser.add_argument("pci", help="matching pciconf selector, e.g. pci0:2:16:0")
    args = parser.parse_args()
    if os.geteuid() != 0:
        parser.error("root is required")
    for repetition in range(1, 4):
        lifecycle(args.device, args.pci)
        print(f"PASS lifecycle repetition {repetition}", flush=True)


if __name__ == "__main__":
    main()
