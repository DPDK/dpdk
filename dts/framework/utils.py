# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation
# Copyright(c) 2022-2023 PANTHEON.tech s.r.o.
# Copyright(c) 2022-2023 University of New Hampshire

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
import subprocess
from enum import Enum
from pathlib import Path
from subprocess import SubprocessError

from scapy.packet import Packet  # type: ignore[import]

from .exception import ConfigurationError

REGEX_FOR_PCI_ADDRESS: str = "/[0-9a-fA-F]{4}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}.[0-9]{1}/"


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


class MesonArgs(object):
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


class DPDKGitTarball(object):
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

        self._get_commit_id()
        self._create_tarball_dir()

        self._tarball_name = (
            f"dpdk-tarball-{self._git_ref}.tar.{self._tar_compression_format.value}"
        )
        self._tarball_path = self._check_tarball_path()
        if not self._tarball_path:
            self._create_tarball()

    def _get_commit_id(self) -> None:
        result = subprocess.run(
            ["git", "rev-parse", "--verify", self._git_ref],
            text=True,
            capture_output=True,
        )
        if result.returncode != 0:
            raise ConfigurationError(
                f"{self._git_ref} is neither a path to an existing DPDK "
                "archive nor a valid git reference.\n"
                f"Command: {result.args}\n"
                f"Stdout: {result.stdout}\n"
                f"Stderr: {result.stderr}"
            )
        self._git_ref = result.stdout.strip()

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
