# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 PANTHEON.tech s.r.o.
# Copyright(c) 2023 University of New Hampshire

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
from ipaddress import IPv4Interface, IPv6Interface
from pathlib import PurePath
from typing import Type, TypeVar, Union

from framework.config import Architecture, NodeConfiguration, NodeInfo
from framework.logger import DTSLOG
from framework.remote_session import (
    CommandResult,
    InteractiveRemoteSession,
    InteractiveShell,
    RemoteSession,
    create_interactive_session,
    create_remote_session,
)
from framework.settings import SETTINGS
from framework.utils import MesonArgs

from .cpu import LogicalCore
from .port import Port

InteractiveShellType = TypeVar("InteractiveShellType", bound=InteractiveShell)


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
    _logger: DTSLOG
    remote_session: RemoteSession
    interactive_session: InteractiveRemoteSession

    def __init__(
        self,
        node_config: NodeConfiguration,
        name: str,
        logger: DTSLOG,
    ):
        """Initialize the OS-aware session.

        Connect to the node right away and also create an interactive remote session.

        Args:
            node_config: The test run configuration of the node to connect to.
            name: The name of the session.
            logger: The logger instance this session will use.
        """
        self._config = node_config
        self.name = name
        self._logger = logger
        self.remote_session = create_remote_session(node_config, name, logger)
        self.interactive_session = create_interactive_session(node_config, logger)

    def close(self, force: bool = False) -> None:
        """Close the underlying remote session.

        Args:
            force: Force the closure of the connection.
        """
        self.remote_session.close(force)

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

    def create_interactive_shell(
        self,
        shell_cls: Type[InteractiveShellType],
        timeout: float,
        privileged: bool,
        app_args: str,
    ) -> InteractiveShellType:
        """Factory for interactive session handlers.

        Instantiate `shell_cls` according to the remote OS specifics.

        Args:
            shell_cls: The class of the shell.
            timeout: Timeout for reading output from the SSH channel. If you are
                reading from the buffer and don't receive any data within the timeout
                it will throw an error.
            privileged: Whether to run the shell with administrative privileges.
            app_args: The arguments to be passed to the application.

        Returns:
            An instance of the desired interactive application shell.
        """
        return shell_cls(
            self.interactive_session.session,
            self._logger,
            self._get_privileged_command if privileged else None,
            app_args,
            timeout,
        )

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
    def guess_dpdk_remote_dir(self, remote_dir: str | PurePath) -> PurePath:
        """Try to find DPDK directory in `remote_dir`.

        The directory is the one which is created after the extraction of the tarball. The files
        are usually extracted into a directory starting with ``dpdk-``.

        Returns:
            The absolute path of the DPDK remote directory, empty path if not found.
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
    def copy_from(
        self,
        source_file: str | PurePath,
        destination_file: str | PurePath,
    ) -> None:
        """Copy a file from the remote node to the local filesystem.

        Copy `source_file` from the remote node associated with this remote
        session to `destination_file` on the local filesystem.

        Args:
            source_file: the file on the remote node.
            destination_file: a file or directory path on the local filesystem.
        """

    @abstractmethod
    def copy_to(
        self,
        source_file: str | PurePath,
        destination_file: str | PurePath,
    ) -> None:
        """Copy a file from local filesystem to the remote node.

        Copy `source_file` from local filesystem to `destination_file`
        on the remote node associated with this remote session.

        Args:
            source_file: the file on the local filesystem.
            destination_file: a file or directory path on the remote node.
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
            remote_dir_path: The path of the directory to remove.
            recursive: If :data:`True`, also remove all contents inside the directory.
            force: If :data:`True`, ignore all warnings and try to remove at all costs.
        """

    @abstractmethod
    def extract_remote_tarball(
        self,
        remote_tarball_path: str | PurePath,
        expected_dir: str | PurePath | None = None,
    ) -> None:
        """Extract remote tarball in its remote directory.

        Args:
            remote_tarball_path: The path of the tarball on the remote node.
            expected_dir: If non-empty, check whether `expected_dir` exists after extracting
                the archive.
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
    def get_remote_cpus(self, use_first_core: bool) -> list[LogicalCore]:
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
    def setup_hugepages(self, hugepage_count: int, force_first_numa: bool) -> None:
        """Configure hugepages on the node.

        Get the node's Hugepage Size, configure the specified count of hugepages
        if needed and mount the hugepages if needed.

        Args:
            hugepage_count: Configure this many hugepages.
            force_first_numa:  If :data:`True`, configure hugepages just on the first numa node.
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
    def get_node_info(self) -> NodeInfo:
        """Collect additional information about the node.

        Returns:
            Node information.
        """

    @abstractmethod
    def update_ports(self, ports: list[Port]) -> None:
        """Get additional information about ports from the operating system and update them.

        The additional information is:

            * Logical name (e.g. ``enp7s0``) if applicable,
            * Mac address.

        Args:
            ports: The ports to update.
        """

    @abstractmethod
    def configure_port_state(self, port: Port, enable: bool) -> None:
        """Enable/disable `port` in the operating system.

        Args:
            port: The port to configure.
            enable: If :data:`True`, enable the port, otherwise shut it down.
        """

    @abstractmethod
    def configure_port_ip_address(
        self,
        address: Union[IPv4Interface, IPv6Interface],
        port: Port,
        delete: bool,
    ) -> None:
        """Configure an IP address on `port` in the operating system.

        Args:
            address: The address to configure.
            port: The port to configure.
            delete: If :data:`True`, remove the IP address, otherwise configure it.
        """

    @abstractmethod
    def configure_ipv4_forwarding(self, enable: bool) -> None:
        """Enable IPv4 forwarding in the operating system.

        Args:
            enable: If :data:`True`, enable the forwarding, otherwise disable it.
        """
