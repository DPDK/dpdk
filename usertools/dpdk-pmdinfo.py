#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2016  Neil Horman <nhorman@tuxdriver.com>
# Copyright(c) 2022  Robin Jarry
# pylint: disable=invalid-name

r"""
Utility to dump PMD_INFO_STRING support from DPDK binaries.

This script prints JSON output to be interpreted by other tools. Here are some
examples with jq:

Get the complete info for a given driver:

  %(prog)s dpdk-testpmd | \
  jq '.[] | select(.name == "cnxk_nix_inl")'

Get only the required kernel modules for a given driver:

  %(prog)s dpdk-testpmd | \
  jq '.[] | select(.name == "net_i40e").kmod'

Get only the required kernel modules for a given device:

  %(prog)s dpdk-testpmd | \
  jq '.[] | select(.pci_ids[]? | .vendor == "15b3" and .device == "1013").kmod'
"""

import argparse
import json
import logging
import os
import re
import string
import subprocess
import sys
from pathlib import Path
from typing import Iterable, Iterator, List, Union

import elftools
from elftools.elf.elffile import ELFError, ELFFile


# ----------------------------------------------------------------------------
def main() -> int:  # pylint: disable=missing-docstring
    try:
        args = parse_args()
        logging.basicConfig(
            stream=sys.stderr,
            format="%(levelname)s: %(message)s",
            level={
                0: logging.ERROR,
                1: logging.WARNING,
            }.get(args.verbose, logging.DEBUG),
        )
        info = parse_pmdinfo(args.elf_files, args.search_plugins)
        print(json.dumps(info, indent=2))
    except BrokenPipeError:
        pass
    except KeyboardInterrupt:
        return 1
    except Exception as e:  # pylint: disable=broad-except
        logging.error("%s", e)
        return 1
    return 0


# ----------------------------------------------------------------------------
def parse_args() -> argparse.Namespace:
    """
    Parse command line arguments.
    """
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "-p",
        "--search-plugins",
        action="store_true",
        help="""
        In addition of ELF_FILEs and their linked dynamic libraries, also scan
        the DPDK plugins path.
        """,
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="count",
        default=0,
        help="""
        Display warnings due to linked libraries not found or ELF/JSON parsing
        errors in these libraries. Use twice to show debug messages.
        """,
    )
    parser.add_argument(
        "elf_files",
        metavar="ELF_FILE",
        nargs="+",
        type=existing_file,
        help="""
        DPDK application binary or dynamic library.
        """,
    )
    return parser.parse_args()


# ----------------------------------------------------------------------------
def parse_pmdinfo(paths: Iterable[Path], search_plugins: bool) -> List[dict]:
    """
    Extract DPDK PMD info JSON strings from an ELF file.

    :returns:
        A list of DPDK drivers info dictionaries.
    """
    binaries = set(paths)
    for p in paths:
        binaries.update(get_needed_libs(p))
    if search_plugins:
        # cast to list to avoid errors with update while iterating
        binaries.update(list(get_plugin_libs(binaries)))

    drivers = []

    for b in binaries:
        logging.debug("analyzing %s", b)
        try:
            for s in get_elf_strings(b, ".rodata", "PMD_INFO_STRING="):
                try:
                    info = json.loads(s)
                    scrub_pci_ids(info)
                    drivers.append(info)
                except ValueError as e:
                    # invalid JSON, should never happen
                    logging.warning("%s: %s", b, e)
        except ELFError as e:
            # only happens for discovered plugins that are not ELF
            logging.debug("%s: cannot parse ELF: %s", b, e)

    return drivers


# ----------------------------------------------------------------------------
PCI_FIELDS = ("vendor", "device", "subsystem_vendor", "subsystem_device")


def scrub_pci_ids(info: dict):
    """
    Convert numerical ids to hex strings.
    Strip empty pci_ids lists.
    Strip wildcard 0xFFFF ids.
    """
    pci_ids = []
    for pci_fields in info.pop("pci_ids"):
        pci = {}
        for name, value in zip(PCI_FIELDS, pci_fields):
            if value != 0xFFFF:
                pci[name] = f"{value:04x}"
        if pci:
            pci_ids.append(pci)
    if pci_ids:
        info["pci_ids"] = pci_ids


# ----------------------------------------------------------------------------
def get_plugin_libs(binaries: Iterable[Path]) -> Iterator[Path]:
    """
    Look into the provided binaries for DPDK_PLUGIN_PATH and scan the path
    for files.
    """
    for b in binaries:
        for p in get_elf_strings(b, ".rodata", "DPDK_PLUGIN_PATH="):
            plugin_path = p.strip()
            logging.debug("discovering plugins in %s", plugin_path)
            for root, _, files in os.walk(plugin_path):
                for f in files:
                    yield Path(root) / f
            # no need to search in other binaries.
            return


# ----------------------------------------------------------------------------
def existing_file(value: str) -> Path:
    """
    Argparse type= callback to ensure an argument points to a valid file path.
    """
    path = Path(value)
    if not path.is_file():
        raise argparse.ArgumentTypeError(f"{value}: No such file")
    return path


# ----------------------------------------------------------------------------
PRINTABLE_BYTES = frozenset(string.printable.encode("ascii"))


def find_strings(buf: bytes, prefix: str) -> Iterator[str]:
    """
    Extract strings of printable ASCII characters from a bytes buffer.
    """
    view = memoryview(buf)
    start = None

    for i, b in enumerate(view):
        if start is None and b in PRINTABLE_BYTES:
            # mark beginning of string
            start = i
            continue
        if start is not None:
            if b in PRINTABLE_BYTES:
                # string not finished
                continue
            if b == 0:
                # end of string
                s = view[start:i].tobytes().decode("ascii")
                if s.startswith(prefix):
                    yield s[len(prefix) :]
            # There can be byte sequences where a non-printable byte
            # follows a printable one. Ignore that.
            start = None


# ----------------------------------------------------------------------------
def elftools_version():
    """
    Extract pyelftools version as a tuple of integers for easy comparison.
    """
    version = getattr(elftools, "__version__", "")
    match = re.match(r"^(\d+)\.(\d+).*$", str(version))
    if not match:
        # cannot determine version, hope for the best
        return (0, 24)
    return (int(match[1]), int(match[2]))


ELFTOOLS_VERSION = elftools_version()


def from_elftools(s: Union[bytes, str]) -> str:
    """
    Earlier versions of pyelftools (< 0.24) return bytes encoded with "latin-1"
    instead of python strings.
    """
    if isinstance(s, bytes):
        return s.decode("latin-1")
    return s


def to_elftools(s: str) -> Union[bytes, str]:
    """
    Earlier versions of pyelftools (< 0.24) assume that ELF section and tags
    are bytes encoded with "latin-1" instead of python strings.
    """
    if ELFTOOLS_VERSION < (0, 24):
        return s.encode("latin-1")
    return s


# ----------------------------------------------------------------------------
def get_elf_strings(path: Path, section: str, prefix: str) -> Iterator[str]:
    """
    Extract strings from a named ELF section in a file.
    """
    with path.open("rb") as f:
        elf = ELFFile(f)
        sec = elf.get_section_by_name(to_elftools(section))
        if not sec:
            return
        yield from find_strings(sec.data(), prefix)


# ----------------------------------------------------------------------------
LDD_LIB_RE = re.compile(
    r"""
    ^                  # beginning of line
    \t                 # tab
    (\S+)              # lib name
    \s+=>\s+
    (/\S+)             # lib path
    \s+
    \(0x[0-9A-Fa-f]+\) # address
    \s*
    $                  # end of line
    """,
    re.MULTILINE | re.VERBOSE,
)


def get_needed_libs(path: Path) -> Iterator[Path]:
    """
    Extract the dynamic library dependencies from an ELF executable.
    """
    with subprocess.Popen(
        ["ldd", str(path)], stdout=subprocess.PIPE, stderr=subprocess.PIPE
    ) as proc:
        out, err = proc.communicate()
        if proc.returncode != 0:
            err = err.decode("utf-8").splitlines()[-1].strip()
            raise Exception(f"cannot read ELF file: {err}")
        for match in LDD_LIB_RE.finditer(out.decode("utf-8")):
            libname, libpath = match.groups()
            if libname.startswith("librte_"):
                libpath = Path(libpath)
                if libpath.is_file():
                    yield libpath.resolve()


# ----------------------------------------------------------------------------
if __name__ == "__main__":
    sys.exit(main())
