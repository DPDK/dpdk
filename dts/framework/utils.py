# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation
# Copyright(c) 2022-2023 PANTHEON.tech s.r.o.
# Copyright(c) 2022-2023 University of New Hampshire

import atexit
import json
import os
import subprocess
import sys
from enum import Enum
from pathlib import Path
from subprocess import SubprocessError

from scapy.packet import Packet  # type: ignore[import]

from .exception import ConfigurationError


class StrEnum(Enum):
    @staticmethod
    def _generate_next_value_(
        name: str, start: int, count: int, last_values: object
    ) -> str:
        return name

    def __str__(self) -> str:
        return self.name


REGEX_FOR_PCI_ADDRESS = "/[0-9a-fA-F]{4}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}.[0-9]{1}/"


def check_dts_python_version() -> None:
    if sys.version_info.major < 3 or (
        sys.version_info.major == 3 and sys.version_info.minor < 10
    ):
        print(
            RED(
                (
                    "WARNING: DTS execution node's python version is lower than"
                    "python 3.10, is deprecated and will not work in future releases."
                )
            ),
            file=sys.stderr,
        )
        print(RED("Please use Python >= 3.10 instead"), file=sys.stderr)


def expand_range(range_str: str) -> list[int]:
    """
    Process range string into a list of integers. There are two possible formats:
    n - a single integer
    n-m - a range of integers

    The returned range includes both n and m. Empty string returns an empty list.
    """
    expanded_range: list[int] = []
    if range_str:
        range_boundaries = range_str.split("-")
        # will throw an exception when items in range_boundaries can't be converted,
        # serving as type check
        expanded_range.extend(
            range(int(range_boundaries[0]), int(range_boundaries[-1]) + 1)
        )

    return expanded_range


def get_packet_summaries(packets: list[Packet]):
    if len(packets) == 1:
        packet_summaries = packets[0].summary()
    else:
        packet_summaries = json.dumps(
            list(map(lambda pkt: pkt.summary(), packets)), indent=4
        )
    return f"Packet contents: \n{packet_summaries}"


def RED(text: str) -> str:
    return f"\u001B[31;1m{str(text)}\u001B[0m"


class MesonArgs(object):
    """
    Aggregate the arguments needed to build DPDK:
    default_library: Default library type, Meson allows "shared", "static" and "both".
               Defaults to None, in which case the argument won't be used.
    Keyword arguments: The arguments found in meson_options.txt in root DPDK directory.
               Do not use -D with them, for example:
               meson_args = MesonArgs(enable_kmods=True).
    """

    _default_library: str

    def __init__(self, default_library: str | None = None, **dpdk_args: str | bool):
        self._default_library = (
            f"--default-library={default_library}" if default_library else ""
        )
        self._dpdk_args = " ".join(
            (
                f"-D{dpdk_arg_name}={dpdk_arg_value}"
                for dpdk_arg_name, dpdk_arg_value in dpdk_args.items()
            )
        )

    def __str__(self) -> str:
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
    """Create a compressed tarball of DPDK from the repository.

    The DPDK version is specified with git object git_ref.
    The tarball will be compressed with _TarCompressionFormat,
    which must be supported by the DTS execution environment.
    The resulting tarball will be put into output_dir.

    The class supports the os.PathLike protocol,
    which is used to get the Path of the tarball::

        from pathlib import Path
        tarball = DPDKGitTarball("HEAD", "output")
        tarball_path = Path(tarball)

    Arguments:
        git_ref: A git commit ID, tag ID or tree ID.
        output_dir: The directory where to put the resulting tarball.
        tar_compression_format: The compression format to use.
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

    def __fspath__(self):
        return str(self._tarball_path)
