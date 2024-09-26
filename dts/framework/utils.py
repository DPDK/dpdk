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

import atexit
import json
import os
import random
import subprocess
from enum import Enum, Flag
from pathlib import Path
from subprocess import SubprocessError

from scapy.layers.inet import IP, TCP, UDP, Ether  # type: ignore[import-untyped]
from scapy.packet import Packet  # type: ignore[import-untyped]

from .exception import ConfigurationError, InternalError

REGEX_FOR_PCI_ADDRESS: str = "/[0-9a-fA-F]{4}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}.[0-9]{1}/"
_REGEX_FOR_COLON_OR_HYPHEN_SEP_MAC: str = r"(?:[\da-fA-F]{2}[:-]){5}[\da-fA-F]{2}"
_REGEX_FOR_DOT_SEP_MAC: str = r"(?:[\da-fA-F]{4}.){2}[\da-fA-F]{4}"
REGEX_FOR_MAC_ADDRESS: str = rf"{_REGEX_FOR_COLON_OR_HYPHEN_SEP_MAC}|{_REGEX_FOR_DOT_SEP_MAC}"
REGEX_FOR_BASE64_ENCODING: str = "[-a-zA-Z0-9+\\/]*={0,3}"


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


def get_commit_id(rev_id: str) -> str:
    """Given a Git revision ID, return the corresponding commit ID.

    Args:
        rev_id: The Git revision ID.

    Raises:
        ConfigurationError: The ``git rev-parse`` command failed, suggesting
            an invalid or ambiguous revision ID was supplied.
    """
    result = subprocess.run(
        ["git", "rev-parse", "--verify", rev_id],
        text=True,
        capture_output=True,
    )
    if result.returncode != 0:
        raise ConfigurationError(
            f"{rev_id} is not a valid git reference.\n"
            f"Command: {result.args}\n"
            f"Stdout: {result.stdout}\n"
            f"Stderr: {result.stderr}"
        )
    return result.stdout.strip()


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

                meson_args = MesonArgs(enable_kmods=True).
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


class _TarCompressionFormat(StrEnum):
    """Compression formats that tar can use.

    Enum names are the shell compression commands
    and Enum values are the associated file extensions.
    """

    gzip = "gz"
    compress = "Z"
    bzip2 = "bz2"
    lzip = "lz"
    lzma = "lzma"
    lzop = "lzo"
    xz = "xz"
    zstd = "zst"


class DPDKGitTarball:
    """Compressed tarball of DPDK from the repository.

    The class supports the :class:`os.PathLike` protocol,
    which is used to get the Path of the tarball::

        from pathlib import Path
        tarball = DPDKGitTarball("HEAD", "output")
        tarball_path = Path(tarball)
    """

    _git_ref: str
    _tar_compression_format: _TarCompressionFormat
    _tarball_dir: Path
    _tarball_name: str
    _tarball_path: Path | None

    def __init__(
        self,
        git_ref: str,
        output_dir: str,
        tar_compression_format: _TarCompressionFormat = _TarCompressionFormat.xz,
    ):
        """Create the tarball during initialization.

        The DPDK version is specified with `git_ref`. The tarball will be compressed with
        `tar_compression_format`, which must be supported by the DTS execution environment.
        The resulting tarball will be put into `output_dir`.

        Args:
            git_ref: A git commit ID, tag ID or tree ID.
            output_dir: The directory where to put the resulting tarball.
            tar_compression_format: The compression format to use.
        """
        self._git_ref = git_ref
        self._tar_compression_format = tar_compression_format

        self._tarball_dir = Path(output_dir, "tarball")

        self._create_tarball_dir()

        self._tarball_name = (
            f"dpdk-tarball-{self._git_ref}.tar.{self._tar_compression_format.value}"
        )
        self._tarball_path = self._check_tarball_path()
        if not self._tarball_path:
            self._create_tarball()

    def _create_tarball_dir(self) -> None:
        os.makedirs(self._tarball_dir, exist_ok=True)

    def _check_tarball_path(self) -> Path | None:
        if self._tarball_name in os.listdir(self._tarball_dir):
            return Path(self._tarball_dir, self._tarball_name)
        return None

    def _create_tarball(self) -> None:
        self._tarball_path = Path(self._tarball_dir, self._tarball_name)

        atexit.register(self._delete_tarball)

        result = subprocess.run(
            'git -C "$(git rev-parse --show-toplevel)" archive '
            f'{self._git_ref} --prefix="dpdk-tarball-{self._git_ref + os.sep}" | '
            f"{self._tar_compression_format} > {Path(self._tarball_path.absolute())}",
            shell=True,
            text=True,
            capture_output=True,
        )

        if result.returncode != 0:
            raise SubprocessError(
                f"Git archive creation failed with exit code {result.returncode}.\n"
                f"Command: {result.args}\n"
                f"Stdout: {result.stdout}\n"
                f"Stderr: {result.stderr}"
            )

        atexit.unregister(self._delete_tarball)

    def _delete_tarball(self) -> None:
        if self._tarball_path and os.path.exists(self._tarball_path):
            os.remove(self._tarball_path)

    def __fspath__(self) -> str:
        """The os.PathLike protocol implementation."""
        return str(self._tarball_path)


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

    l4_factories = []
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


class MultiInheritanceBaseClass:
    """A base class for classes utilizing multiple inheritance.

    This class enables it's subclasses to support both single and multiple inheritance by acting as
    a stopping point in the tree of calls to the constructors of superclasses. This class is able
    to exist at the end of the Method Resolution Order (MRO) so that subclasses can call
    :meth:`super.__init__` without repercussion.
    """

    def __init__(self, *args, **kwargs) -> None:
        """Call the init method of :class:`object`."""
        super().__init__()
