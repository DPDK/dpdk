# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 PANTHEON.tech s.r.o.
# Copyright(c) 2023 University of New Hampshire
# Copyright(c) 2024 Arm Limited

"""OS-aware remote session.

DPDK supports multiple different operating systems, meaning it can run on these different operating
systems. This module defines the common API that OS-unaware layers use and translates the API into
OS-aware calls/utility usage.

Note:
    Running commands with administrative privileges requires OS awareness. This is the only layer
    that's aware of OS differences, so this is where non-privileged command get converted
    to privileged commands.

Example:
    A user wishes to remove a directory on a remote :class:`~.sut_node.SutNode`.
    The :class:`~.sut_node.SutNode` object isn't aware what OS the node is running - it delegates
    the OS translation logic to :attr:`~.node.Node.main_session`. The SUT node calls
    :meth:`~OSSession.remove_remote_dir` with a generic, OS-unaware path and
    the :attr:`~.node.Node.main_session` translates that to ``rm -rf`` if the node's OS is Linux
    and other commands for other OSs. It also translates the path to match the underlying OS.
"""

from abc import ABC, abstractmethod
from collections.abc import Iterable
from dataclasses import dataclass
from enum import Flag, auto
from pathlib import Path, PurePath, PurePosixPath

from framework.config.node import NodeConfiguration
from framework.logger import DTSLogger
from framework.remote_session import (
    InteractiveRemoteSession,
    RemoteSession,
    create_interactive_session,
    create_remote_session,
)
from framework.remote_session.remote_session import CommandResult
from framework.settings import SETTINGS
from framework.utils import MesonArgs, TarCompressionFormat

from .cpu import Architecture, LogicalCore
from .port import Port, PortInfo


class FilePermissions(Flag):
    """The permissions for a file and/or directory."""

    #:
    OTHERS_EXECUTE = auto()
    #:
    OTHERS_WRITE = auto()
    #:
    OTHERS_READ = auto()
    #:
    GROUP_EXECUTE = auto()
    #:
    GROUP_WRITE = auto()
    #:
    GROUP_READ = auto()
    #:
    OWNER_EXECUTE = auto()
    #:
    OWNER_WRITE = auto()
    #:
    OWNER_READ = auto()

    def to_octal(self) -> str:
        """Convert this flag to an octal representation."""
        return format(self.value, "03o")


@dataclass(slots=True, frozen=True)
class OSSessionInfo:
    """Supplemental OS session information.

    Attributes:
        os_name: The name of the running operating system of
            the :class:`~framework.testbed_model.node.Node`.
        os_version: The version of the running operating system of
            the :class:`~framework.testbed_model.node.Node`.
        kernel_version: The kernel version of the running operating system of
            the :class:`~framework.testbed_model.node.Node`.
    """

    os_name: str
    os_version: str
    kernel_version: str


class OSSession(ABC):
    """OS-unaware to OS-aware translation API definition.

    The OSSession classes create a remote session to a DTS node and implement OS specific
    behavior. There a few control methods implemented by the base class, the rest need
    to be implemented by subclasses.

    Attributes:
        name: The name of the session.
        remote_session: The remote session maintaining the connection to the node.
        interactive_session: The interactive remote session maintaining the connection to the node.
    """

    _config: NodeConfiguration
    name: str
    _logger: DTSLogger
    remote_session: RemoteSession
    interactive_session: InteractiveRemoteSession
    hugepage_size: int

    def __init__(
        self,
        node_config: NodeConfiguration,
        name: str,
        logger: DTSLogger,
    ) -> None:
        """Initialize the OS-aware session.

        Connect to the node right away and also create an interactive remote session.

        Args:
            node_config: The test run configuration of the node to connect to.
            name: The name of the session.
            logger: The logger instance this session will use.
        """
        self.hugepage_size = 2048
        self._config = node_config
        self.name = name
        self._logger = logger
        self.remote_session = create_remote_session(node_config, name, logger)
        self.interactive_session = create_interactive_session(node_config, logger)

    def is_alive(self) -> bool:
        """Check whether the underlying remote session is still responding."""
        return self.remote_session.is_alive()

    def send_command(
        self,
        command: str,
        timeout: float = SETTINGS.timeout,
        privileged: bool = False,
        verify: bool = False,
        env: dict | None = None,
    ) -> CommandResult:
        """An all-purpose API for OS-agnostic commands.

        This can be used for an execution of a portable command that's executed the same way
        on all operating systems, such as Python.

        The :option:`--timeout` command line argument and the :envvar:`DTS_TIMEOUT`
        environment variable configure the timeout of command execution.

        Args:
            command: The command to execute.
            timeout: Wait at most this long in seconds for `command` execution to complete.
            privileged: Whether to run the command with administrative privileges.
            verify: If :data:`True`, will check the exit code of the command.
            env: A dictionary with environment variables to be used with the command execution.

        Raises:
            RemoteCommandExecutionError: If verify is :data:`True` and the command failed.
        """
        if privileged:
            command = self._get_privileged_command(command)

        return self.remote_session.send_command(command, timeout, verify, env)

    def close(self) -> None:
        """Close the underlying remote session."""
        self.remote_session.close()

    @staticmethod
    @abstractmethod
    def _get_privileged_command(command: str) -> str:
        """Modify the command so that it executes with administrative privileges.

        Args:
            command: The command to modify.

        Returns:
            The modified command that executes with administrative privileges.
        """

    @abstractmethod
    def get_remote_tmp_dir(self) -> PurePath:
        """Get the path of the temporary directory of the remote OS.

        Returns:
            The absolute path of the temporary directory.
        """

    @abstractmethod
    def get_dpdk_build_env_vars(self, arch: Architecture) -> dict:
        """Create extra environment variables needed for the target architecture.

        Different architectures may require different configuration, such as setting 32-bit CFLAGS.

        Returns:
            A dictionary with keys as environment variables.
        """

    @abstractmethod
    def join_remote_path(self, *args: str | PurePath) -> PurePath:
        """Join path parts using the path separator that fits the remote OS.

        Args:
            args: Any number of paths to join.

        Returns:
            The resulting joined path.
        """

    @abstractmethod
    def remote_path_exists(self, remote_path: str | PurePath) -> bool:
        """Check whether `remote_path` exists on the remote system.

        Args:
            remote_path: The path to check.

        Returns:
            :data:`True` if the path exists, :data:`False` otherwise.
        """

    @abstractmethod
    def create_tmp_dir(self, template: str = "dts.XXXXX") -> PurePath:
        """Create a temporary directory on the remote node.

        Args:
            template: The template to use for the name of the directory. "X"s are treated
                as placeholder.

        Returns:
            The path to the created directory.
        """

    @abstractmethod
    def copy_from(self, source_file: str | PurePath, destination_dir: str | Path) -> None:
        """Copy a file from the remote node to the local filesystem.

        Copy `source_file` from the remote node associated with this remote
        session to `destination_dir` on the local filesystem.

        Args:
            source_file: The file on the remote node.
            destination_dir: The directory path on the local filesystem where the `source_file`
                will be saved.
        """

    @abstractmethod
    def copy_to(self, source_file: str | Path, destination_dir: str | PurePath) -> None:
        """Copy a file from local filesystem to the remote node.

        Copy `source_file` from local filesystem to `destination_dir`
        on the remote node associated with this remote session.

        Args:
            source_file: The file on the local filesystem.
            destination_dir: The directory path on the remote Node where the `source_file`
                will be saved.
        """

    @abstractmethod
    def copy_dir_from(
        self,
        source_dir: str | PurePath,
        destination_dir: str | Path,
        compress_format: TarCompressionFormat = TarCompressionFormat.none,
        exclude: str | list[str] | None = None,
    ) -> None:
        """Copy a directory from the remote node to the local filesystem.

        Copy `source_dir` from the remote node associated with this remote session to
        `destination_dir` on the local filesystem. The new local directory will be created
        at `destination_dir` path.

        Example:
            source_dir = '/remote/path/to/source'
            destination_dir = '/local/path/to/destination'
            compress_format = TarCompressionFormat.xz

            The method will:
                1. Create a tarball from `source_dir`, resulting in:
                    '/remote/path/to/source.tar.xz',
                2. Copy '/remote/path/to/source.tar.xz' to
                    '/local/path/to/destination/source.tar.xz',
                3. Extract the contents of the tarball, resulting in:
                    '/local/path/to/destination/source/',
                4. Remove the tarball after extraction
                    ('/local/path/to/destination/source.tar.xz').

            Final Path Structure:
                '/local/path/to/destination/source/'

        Args:
            source_dir: The directory on the remote node.
            destination_dir: The directory path on the local filesystem.
            compress_format: The compression format to use. Defaults to no compression.
            exclude: Patterns for files or directories to exclude from the tarball.
                These patterns are used with `tar`'s `--exclude` option.
        """

    @abstractmethod
    def copy_dir_to(
        self,
        source_dir: str | Path,
        destination_dir: str | PurePath,
        compress_format: TarCompressionFormat = TarCompressionFormat.none,
        exclude: str | list[str] | None = None,
    ) -> None:
        """Copy a directory from the local filesystem to the remote node.

        Copy `source_dir` from the local filesystem to `destination_dir` on the remote node
        associated with this remote session. The new remote directory will be created at
        `destination_dir` path.

        Example:
            source_dir = '/local/path/to/source'
            destination_dir = '/remote/path/to/destination'
            compress_format = TarCompressionFormat.xz

            The method will:
                1. Create a tarball from `source_dir`, resulting in:
                    '/local/path/to/source.tar.xz',
                2. Copy '/local/path/to/source.tar.xz' to
                    '/remote/path/to/destination/source.tar.xz',
                3. Extract the contents of the tarball, resulting in:
                    '/remote/path/to/destination/source/',
                4. Remove the tarball after extraction
                    ('/remote/path/to/destination/source.tar.xz').

            Final Path Structure:
                '/remote/path/to/destination/source/'

        Args:
            source_dir: The directory on the local filesystem.
            destination_dir: The directory path on the remote node.
            compress_format: The compression format to use. Defaults to no compression.
            exclude: Patterns for files or directories to exclude from the tarball.
                These patterns are used with `fnmatch.fnmatch` to filter out files.
        """

    @abstractmethod
    def change_permissions(
        self, remote_path: PurePath, permissions: FilePermissions, recursive: bool = False
    ) -> None:
        """Change the permissions of the given path."""

    @abstractmethod
    def remove_remote_file(self, remote_file_path: str | PurePath, force: bool = True) -> None:
        """Remove remote file, by default remove forcefully.

        Args:
            remote_file_path: The file path to remove.
            force: If :data:`True`, ignore all warnings and try to remove at all costs.
        """

    @abstractmethod
    def remove_remote_dir(
        self,
        remote_dir_path: str | PurePath,
        recursive: bool = True,
        force: bool = True,
    ) -> None:
        """Remove remote directory, by default remove recursively and forcefully.

        Args:
            remote_dir_path: The directory path to remove.
            recursive: If :data:`True`, also remove all contents inside the directory.
            force: If :data:`True`, ignore all warnings and try to remove at all costs.
        """

    @abstractmethod
    def create_remote_tarball(
        self,
        remote_dir_path: str | PurePath,
        compress_format: TarCompressionFormat = TarCompressionFormat.none,
        exclude: str | list[str] | None = None,
    ) -> PurePosixPath:
        """Create a tarball from the contents of the specified remote directory.

        This method creates a tarball containing all files and directories
        within `remote_dir_path`. The tarball will be saved in the directory of
        `remote_dir_path` and will be named based on `remote_dir_path`.

        Args:
            remote_dir_path: The directory path on the remote node.
            compress_format: The compression format to use. Defaults to no compression.
            exclude: Patterns for files or directories to exclude from the tarball.
                These patterns are used with `tar`'s `--exclude` option.

        Returns:
            The path to the created tarball on the remote node.
        """

    @abstractmethod
    def create_directory(self, path: PurePath) -> None:
        """Create a directory at a specified `path`."""

    @abstractmethod
    def extract_remote_tarball(
        self,
        remote_tarball_path: str | PurePath,
        destination_path: str | PurePath,
        strip_root_dir: bool = False,
    ) -> None:
        """Extract remote tarball in the given path.

        Args:
            remote_tarball_path: The tarball path on the remote node.
            destination_path: The location the tarball will be extracted to.
            strip_root_dir: If :data:`True` and the root of the tarball is a folder, strip it and
                extract its contents only.
        """

    @abstractmethod
    def is_remote_dir(self, remote_path: PurePath) -> bool:
        """Check if the `remote_path` is a directory.

        Args:
            remote_path: The path to the remote tarball.

        Returns:
            If :data:`True` the `remote_path` is a directory, otherwise :data:`False`.
        """

    @abstractmethod
    def is_remote_tarfile(self, remote_tarball_path: PurePath) -> bool:
        """Check if the `remote_tarball_path` is a tar archive.

        Args:
            remote_tarball_path: The path to the remote tarball.

        Returns:
            If :data:`True` the `remote_tarball_path` is a tar archive, otherwise :data:`False`.
        """

    @abstractmethod
    def get_tarball_top_dir(
        self, remote_tarball_path: str | PurePath
    ) -> str | PurePosixPath | None:
        """Get the top directory of the remote tarball.

        Examines the contents of a tarball located at the given `remote_tarball_path` and
        determines the top-level directory. If all files and directories in the tarball share
        the same top-level directory, that directory name is returned. If the tarball contains
        multiple top-level directories or is empty, the method return None.

        Args:
            remote_tarball_path: The path to the remote tarball.

        Returns:
            The top directory of the tarball. If there are multiple top directories
            or the tarball is empty, returns :data:`None`.
        """

    @abstractmethod
    def build_dpdk(
        self,
        env_vars: dict,
        meson_args: MesonArgs,
        remote_dpdk_dir: str | PurePath,
        remote_dpdk_build_dir: str | PurePath,
        rebuild: bool = False,
        timeout: float = SETTINGS.compile_timeout,
    ) -> None:
        """Build DPDK on the remote node.

        An extracted DPDK tarball must be present on the node. The build consists of two steps::

            meson setup <meson args> remote_dpdk_dir remote_dpdk_build_dir
            ninja -C remote_dpdk_build_dir

        The :option:`--compile-timeout` command line argument and the :envvar:`DTS_COMPILE_TIMEOUT`
        environment variable configure the timeout of DPDK build.

        Args:
            env_vars: Use these environment variables when building DPDK.
            meson_args: Use these meson arguments when building DPDK.
            remote_dpdk_dir: The directory on the remote node where DPDK will be built.
            remote_dpdk_build_dir: The target build directory on the remote node.
            rebuild: If :data:`True`, do a subsequent build with ``meson configure`` instead
                of ``meson setup``.
            timeout: Wait at most this long in seconds for the build execution to complete.
        """

    @abstractmethod
    def get_dpdk_version(self, version_path: str | PurePath) -> str:
        """Inspect the DPDK version on the remote node.

        Args:
            version_path: The path to the VERSION file containing the DPDK version.

        Returns:
            The DPDK version.
        """

    @abstractmethod
    def get_remote_cpus(self) -> list[LogicalCore]:
        r"""Get the list of :class:`~.cpu.LogicalCore`\s on the remote node.

        Args:
            use_first_core: If :data:`False`, the first physical core won't be used.

        Returns:
            The logical cores present on the node.
        """

    @abstractmethod
    def kill_cleanup_dpdk_apps(self, dpdk_prefix_list: Iterable[str]) -> None:
        """Kill and cleanup all DPDK apps.

        Args:
            dpdk_prefix_list: Kill all apps identified by `dpdk_prefix_list`.
                If `dpdk_prefix_list` is empty, attempt to find running DPDK apps to kill and clean.
        """

    @abstractmethod
    def get_dpdk_file_prefix(self, dpdk_prefix: str) -> str:
        """Make OS-specific modification to the DPDK file prefix.

        Args:
           dpdk_prefix: The OS-unaware file prefix.

        Returns:
            The OS-specific file prefix.
        """

    @abstractmethod
    def setup_hugepages(self, number_of: int, hugepage_size: int, force_first_numa: bool) -> None:
        """Configure hugepages on the node.

        Get the node's Hugepage Size, configure the specified count of hugepages
        if needed and mount the hugepages if needed.

        Args:
            number_of: Configure this many hugepages.
            hugepage_size: Configure hugepages of this size.
            force_first_numa:  If :data:`True`, configure just on the first numa node.
        """

    @abstractmethod
    def get_compiler_version(self, compiler_name: str) -> str:
        """Get installed version of compiler used for DPDK.

        Args:
            compiler_name: The name of the compiler executable.

        Returns:
            The compiler's version.
        """

    @abstractmethod
    def get_node_info(self) -> OSSessionInfo:
        """Collect additional information about the node.

        Returns:
            Node information.
        """

    @abstractmethod
    def get_arch_info(self) -> str:
        """Discover CPU architecture of the remote host.

        Returns:
            Remote host CPU architecture.
        """

    @abstractmethod
    def get_port_info(self, pci_address: str) -> PortInfo:
        """Get port information.

        Returns:
            An instance of :class:`PortInfo`.

        Raises:
            ConfigurationError: If the port could not be found.
        """

    @abstractmethod
    def bind_ports_to_driver(self, ports: list[Port], driver_name: str) -> None:
        """Bind `ports` to the given `driver_name`.

        Args:
            ports: The list of the ports to bind to the driver.
            driver_name: The name of the driver to bind the ports to.
        """

    @abstractmethod
    def bring_up_link(self, ports: Iterable[Port]) -> None:
        """Send operating system specific command for bringing up link on node interfaces.

        Args:
            ports: The ports to apply the link up command to.
        """

    @abstractmethod
    def configure_port_mtu(self, mtu: int, port: Port) -> None:
        """Configure `mtu` on `port`.

        Args:
            mtu: Desired MTU value.
            port: Port to set `mtu` on.
        """

    @abstractmethod
    def create_vfs(self, pf_port: Port) -> None:
        """Creates virtual functions for `pf_port`.

        Checks how many virtual functions (VFs) `pf_port` supports, and creates that
        number of VFs on the port.

        Args:
            pf_port: The port to create virtual functions on.

        Raises:
            InternalError: If the number of VFs is greater than 0 but less than the
            maximum for `pf_port`.
        """

    @abstractmethod
    def delete_vfs(self, pf_port: Port) -> None:
        """Deletes virtual functions for `pf_port`.

        Checks how many virtual functions (VFs) `pf_port` supports, and deletes that
        number of VFs on the port.

        Args:
            pf_port: The port to delete virtual functions on.

        Raises:
            InternalError: If the number of VFs is greater than 0 but less than the
            maximum for `pf_port`.
        """

    @abstractmethod
    def get_pci_addr_of_vfs(self, pf_port: Port) -> list[str]:
        """Find the PCI addresses of all virtual functions (VFs) on the port `pf_port`.

        Args:
            pf_port: The port to find the VFs on.

        Returns:
            A list containing all of the PCI addresses of the VFs on the port. If the port has no
            VFs then the list will be empty.
        """
