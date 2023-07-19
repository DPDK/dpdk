# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 PANTHEON.tech s.r.o.
# Copyright(c) 2023 University of New Hampshire

from abc import ABC, abstractmethod
from collections.abc import Iterable
from ipaddress import IPv4Interface, IPv6Interface
from pathlib import PurePath
from typing import Type, TypeVar, Union

from framework.config import Architecture, NodeConfiguration, NodeInfo
from framework.logger import DTSLOG
from framework.remote_session.remote import InteractiveShell
from framework.settings import SETTINGS
from framework.testbed_model import LogicalCore
from framework.testbed_model.hw.port import Port
from framework.utils import MesonArgs

from .remote import (
    CommandResult,
    InteractiveRemoteSession,
    RemoteSession,
    create_interactive_session,
    create_remote_session,
)

InteractiveShellType = TypeVar("InteractiveShellType", bound=InteractiveShell)


class OSSession(ABC):
    """
    The OS classes create a DTS node remote session and implement OS specific
    behavior. There a few control methods implemented by the base class, the rest need
    to be implemented by derived classes.
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
        self._config = node_config
        self.name = name
        self._logger = logger
        self.remote_session = create_remote_session(node_config, name, logger)
        self.interactive_session = create_interactive_session(node_config, logger)

    def close(self, force: bool = False) -> None:
        """
        Close the remote session.
        """
        self.remote_session.close(force)

    def is_alive(self) -> bool:
        """
        Check whether the remote session is still responding.
        """
        return self.remote_session.is_alive()

    def send_command(
        self,
        command: str,
        timeout: float = SETTINGS.timeout,
        privileged: bool = False,
        verify: bool = False,
        env: dict | None = None,
    ) -> CommandResult:
        """
        An all-purpose API in case the command to be executed is already
        OS-agnostic, such as when the path to the executed command has been
        constructed beforehand.
        """
        if privileged:
            command = self._get_privileged_command(command)

        return self.remote_session.send_command(command, timeout, verify, env)

    def create_interactive_shell(
        self,
        shell_cls: Type[InteractiveShellType],
        eal_parameters: str,
        timeout: float,
        privileged: bool,
    ) -> InteractiveShellType:
        """
        See "create_interactive_shell" in SutNode
        """
        return shell_cls(
            self.interactive_session.session,
            self._logger,
            self._get_privileged_command if privileged else None,
            eal_parameters,
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
    def guess_dpdk_remote_dir(self, remote_dir) -> PurePath:
        """
        Try to find DPDK remote dir in remote_dir.
        """

    @abstractmethod
    def get_remote_tmp_dir(self) -> PurePath:
        """
        Get the path of the temporary directory of the remote OS.
        """

    @abstractmethod
    def get_dpdk_build_env_vars(self, arch: Architecture) -> dict:
        """
        Create extra environment variables needed for the target architecture. Get
        information from the node if needed.
        """

    @abstractmethod
    def join_remote_path(self, *args: str | PurePath) -> PurePath:
        """
        Join path parts using the path separator that fits the remote OS.
        """

    @abstractmethod
    def copy_from(
        self,
        source_file: str | PurePath,
        destination_file: str | PurePath,
    ) -> None:
        """Copy a file from the remote Node to the local filesystem.

        Copy source_file from the remote Node associated with this remote
        session to destination_file on the local filesystem.

        Args:
            source_file: the file on the remote Node.
            destination_file: a file or directory path on the local filesystem.
        """

    @abstractmethod
    def copy_to(
        self,
        source_file: str | PurePath,
        destination_file: str | PurePath,
    ) -> None:
        """Copy a file from local filesystem to the remote Node.

        Copy source_file from local filesystem to destination_file
        on the remote Node associated with this remote session.

        Args:
            source_file: the file on the local filesystem.
            destination_file: a file or directory path on the remote Node.
        """

    @abstractmethod
    def remove_remote_dir(
        self,
        remote_dir_path: str | PurePath,
        recursive: bool = True,
        force: bool = True,
    ) -> None:
        """
        Remove remote directory, by default remove recursively and forcefully.
        """

    @abstractmethod
    def extract_remote_tarball(
        self,
        remote_tarball_path: str | PurePath,
        expected_dir: str | PurePath | None = None,
    ) -> None:
        """
        Extract remote tarball in place. If expected_dir is a non-empty string, check
        whether the dir exists after extracting the archive.
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
        """
        Build DPDK in the input dir with specified environment variables and meson
        arguments.
        """

    @abstractmethod
    def get_dpdk_version(self, version_path: str | PurePath) -> str:
        """
        Inspect DPDK version on the remote node from version_path.
        """

    @abstractmethod
    def get_remote_cpus(self, use_first_core: bool) -> list[LogicalCore]:
        """
        Compose a list of LogicalCores present on the remote node.
        If use_first_core is False, the first physical core won't be used.
        """

    @abstractmethod
    def kill_cleanup_dpdk_apps(self, dpdk_prefix_list: Iterable[str]) -> None:
        """
        Kill and cleanup all DPDK apps identified by dpdk_prefix_list. If
        dpdk_prefix_list is empty, attempt to find running DPDK apps to kill and clean.
        """

    @abstractmethod
    def get_dpdk_file_prefix(self, dpdk_prefix) -> str:
        """
        Get the DPDK file prefix that will be used when running DPDK apps.
        """

    @abstractmethod
    def setup_hugepages(self, hugepage_amount: int, force_first_numa: bool) -> None:
        """
        Get the node's Hugepage Size, configure the specified amount of hugepages
        if needed and mount the hugepages if needed.
        If force_first_numa is True, configure hugepages just on the first socket.
        """

    @abstractmethod
    def get_compiler_version(self, compiler_name: str) -> str:
        """
        Get installed version of compiler used for DPDK
        """

    @abstractmethod
    def get_node_info(self) -> NodeInfo:
        """
        Collect information about the node
        """

    @abstractmethod
    def update_ports(self, ports: list[Port]) -> None:
        """
        Get additional information about ports:
            Logical name (e.g. enp7s0) if applicable
            Mac address
        """

    @abstractmethod
    def configure_port_state(self, port: Port, enable: bool) -> None:
        """
        Enable/disable port.
        """

    @abstractmethod
    def configure_port_ip_address(
        self,
        address: Union[IPv4Interface, IPv6Interface],
        port: Port,
        delete: bool,
    ) -> None:
        """
        Configure (add or delete) an IP address of the input port.
        """

    @abstractmethod
    def configure_ipv4_forwarding(self, enable: bool) -> None:
        """
        Enable IPv4 forwarding in the underlying OS.
        """
