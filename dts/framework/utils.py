# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation
# Copyright(c) 2022-2023 PANTHEON.tech s.r.o.
# Copyright(c) 2022-2023 University of New Hampshire
# Copyright(c) 2024 Arm Limited

"""Various utility classes and functions.

These are used in multiple modules across the framework. They're here because
they provide some non-specific functionality, greatly simplify imports or just don't
fit elsewhere.

Attributes:
    REGEX_FOR_PCI_ADDRESS: The regex representing a PCI address, e.g. ``0000:00:08.0``.
"""

import fnmatch
import json
import os
import random
import tarfile
from enum import Enum, Flag
from pathlib import Path
from typing import Any, Callable

from scapy.layers.inet import IP, TCP, UDP, Ether
from scapy.packet import Packet

from .exception import InternalError

REGEX_FOR_PCI_ADDRESS: str = r"[0-9a-fA-F]{4}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}.[0-9]{1}"
_REGEX_FOR_COLON_OR_HYPHEN_SEP_MAC: str = r"(?:[\da-fA-F]{2}[:-]){5}[\da-fA-F]{2}"
_REGEX_FOR_DOT_SEP_MAC: str = r"(?:[\da-fA-F]{4}.){2}[\da-fA-F]{4}"
REGEX_FOR_MAC_ADDRESS: str = rf"{_REGEX_FOR_COLON_OR_HYPHEN_SEP_MAC}|{_REGEX_FOR_DOT_SEP_MAC}"
REGEX_FOR_IDENTIFIER: str = r"\w+(?:[\w -]*\w+)?"
REGEX_FOR_PORT_LINK: str = (
    rf"(?:(sut|tg)\.)?({REGEX_FOR_IDENTIFIER})"  # left side
    r"\s+<->\s+"
    rf"(?:(sut|tg)\.)?({REGEX_FOR_IDENTIFIER})"  # right side
)


def expand_range(range_str: str) -> list[int]:
    """Process `range_str` into a list of integers.

    There are two possible formats of `range_str`:

        * ``n`` - a single integer,
        * ``n-m`` - a range of integers.

    The returned range includes both ``n`` and ``m``. Empty string returns an empty list.

    Args:
        range_str: The range to expand.

    Returns:
        All the numbers from the range.
    """
    expanded_range: list[int] = []
    if range_str:
        range_boundaries = range_str.split("-")
        # will throw an exception when items in range_boundaries can't be converted,
        # serving as type check
        expanded_range.extend(range(int(range_boundaries[0]), int(range_boundaries[-1]) + 1))

    return expanded_range


def get_packet_summaries(packets: list[Packet]) -> str:
    """Format a string summary from `packets`.

    Args:
        packets: The packets to format.

    Returns:
        The summary of `packets`.
    """
    if len(packets) == 1:
        packet_summaries = packets[0].summary()
    else:
        packet_summaries = json.dumps(list(map(lambda pkt: pkt.summary(), packets)), indent=4)
    return f"Packet contents: \n{packet_summaries}"


class StrEnum(Enum):
    """Enum with members stored as strings."""

    @staticmethod
    def _generate_next_value_(name: str, start: int, count: int, last_values: object) -> str:
        return name

    def __str__(self) -> str:
        """The string representation is the name of the member."""
        return self.name


class MesonArgs:
    """Aggregate the arguments needed to build DPDK."""

    _default_library: str

    def __init__(self, default_library: str | None = None, **dpdk_args: str | bool):
        """Initialize the meson arguments.

        Args:
            default_library: The default library type, Meson supports ``shared``, ``static`` and
                ``both``. Defaults to :data:`None`, in which case the argument won't be used.
            dpdk_args: The arguments found in ``meson_options.txt`` in root DPDK directory.
                Do not use ``-D`` with them.

        Example:
            ::

                meson_args = MesonArgs(check_includes=True).
        """
        self._default_library = f"--default-library={default_library}" if default_library else ""
        self._dpdk_args = " ".join(
            (
                f"-D{dpdk_arg_name}={dpdk_arg_value}"
                for dpdk_arg_name, dpdk_arg_value in dpdk_args.items()
            )
        )

    def __str__(self) -> str:
        """The actual args."""
        return " ".join(f"{self._default_library} {self._dpdk_args}".split())


class TarCompressionFormat(StrEnum):
    """Compression formats that tar can use.

    Enum names are the shell compression commands
    and Enum values are the associated file extensions.

    The 'none' member represents no compression, only archiving with tar.
    Its value is set to 'tar' to indicate that the file is an uncompressed tar archive.
    """

    none = "tar"
    gzip = "gz"
    compress = "Z"
    bzip2 = "bz2"
    lzip = "lz"
    lzma = "lzma"
    lzop = "lzo"
    xz = "xz"
    zstd = "zst"

    @property
    def extension(self) -> str:
        """Return the extension associated with the compression format.

        If the compression format is 'none', the extension will be in the format 'tar'.
        For other compression formats, the extension will be in the format
        'tar.{compression format}'.
        """
        return f"{self.value}" if self == self.none else f"{type(self).none.value}.{self.value}"


def convert_to_list_of_string(value: Any | list[Any]) -> list[str]:
    """Convert the input to the list of strings."""
    return list(map(str, value) if isinstance(value, list) else str(value))


def create_tarball(
    dir_path: Path,
    compress_format: TarCompressionFormat = TarCompressionFormat.none,
    exclude: Any | list[Any] | None = None,
) -> Path:
    """Create a tarball from the contents of the specified directory.

    This method creates a tarball containing all files and directories within `dir_path`.
    The tarball will be saved in the directory of `dir_path` and will be named based on `dir_path`.

    Args:
        dir_path: The directory path.
        compress_format: The compression format to use. Defaults to no compression.
        exclude: Patterns for files or directories to exclude from the tarball.
                These patterns are used with `fnmatch.fnmatch` to filter out files.

    Returns:
        The path to the created tarball.
    """

    def create_filter_function(
        exclude_patterns: str | list[str] | None,
    ) -> Callable | None:
        """Create a filter function based on the provided exclude patterns.

        Args:
            exclude_patterns: Patterns for files or directories to exclude from the tarball.
                These patterns are used with `fnmatch.fnmatch` to filter out files.

        Returns:
            The filter function that excludes files based on the patterns.
        """
        if exclude_patterns:
            exclude_patterns = convert_to_list_of_string(exclude_patterns)

            def filter_func(tarinfo: tarfile.TarInfo) -> tarfile.TarInfo | None:
                file_name = os.path.basename(tarinfo.name)
                if any(fnmatch.fnmatch(file_name, pattern) for pattern in exclude_patterns):
                    return None
                return tarinfo

            return filter_func
        return None

    target_tarball_path = dir_path.with_suffix(f".{compress_format.extension}")
    with tarfile.open(target_tarball_path, f"w:{compress_format.value}") as tar:
        tar.add(dir_path, arcname=dir_path.name, filter=create_filter_function(exclude))

    return target_tarball_path


def extract_tarball(tar_path: str | Path) -> None:
    """Extract the contents of a tarball.

    The tarball will be extracted in the same path as `tar_path` parent path.

    Args:
        tar_path: The path to the tarball file to extract.
    """
    with tarfile.open(tar_path, "r") as tar:
        tar.extractall(path=Path(tar_path).parent)


class PacketProtocols(Flag):
    """Flag specifying which protocols to use for packet generation."""

    #:
    IP = 1
    #:
    TCP = 2 | IP
    #:
    UDP = 4 | IP
    #:
    ALL = TCP | UDP


def generate_random_packets(
    number_of: int,
    payload_size: int = 1500,
    protocols: PacketProtocols = PacketProtocols.ALL,
    ports_range: range = range(1024, 49152),
    mtu: int = 1500,
) -> list[Packet]:
    """Generate a number of random packets.

    The payload of the packets will consist of random bytes. If `payload_size` is too big, then the
    maximum payload size allowed for the specific packet type is used. The size is calculated based
    on the specified `mtu`, therefore it is essential that `mtu` is set correctly to match the MTU
    of the port that will send out the generated packets.

    If `protocols` has any L4 protocol enabled then all the packets are generated with any of
    the specified L4 protocols chosen at random. If only :attr:`~PacketProtocols.IP` is set, then
    only L3 packets are generated.

    If L4 packets will be generated, then the TCP/UDP ports to be used will be chosen at random from
    `ports_range`.

    Args:
        number_of: The number of packets to generate.
        payload_size: The packet payload size to generate, capped based on `mtu`.
        protocols: The protocols to use for the generated packets.
        ports_range: The range of L4 port numbers to use. Used only if `protocols` has L4 protocols.
        mtu: The MTU of the NIC port that will send out the generated packets.

    Raises:
        InternalError: If the `payload_size` is invalid.

    Returns:
        A list containing the randomly generated packets.
    """
    if payload_size < 0:
        raise InternalError(f"An invalid payload_size of {payload_size} was given.")

    l4_factories: list[type[Packet]] = []
    if protocols & PacketProtocols.TCP:
        l4_factories.append(TCP)
    if protocols & PacketProtocols.UDP:
        l4_factories.append(UDP)

    def _make_packet() -> Packet:
        packet = Ether()

        if protocols & PacketProtocols.IP:
            packet /= IP()

        if len(l4_factories) > 0:
            src_port, dst_port = random.choices(ports_range, k=2)
            packet /= random.choice(l4_factories)(sport=src_port, dport=dst_port)

        max_payload_size = mtu - len(packet)
        usable_payload_size = payload_size if payload_size < max_payload_size else max_payload_size
        return packet / random.randbytes(usable_payload_size)

    return [_make_packet() for _ in range(number_of)]


def to_pascal_case(text: str) -> str:
    """Convert `text` from snake_case to PascalCase."""
    return "".join([seg.capitalize() for seg in text.split("_")])
