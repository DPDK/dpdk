# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation
# Copyright(c) 2022-2023 PANTHEON.tech s.r.o.
# Copyright(c) 2022-2023 University of New Hampshire

"""
A node is a generic host that DTS connects to and manages.
"""

from abc import ABC
from ipaddress import IPv4Interface, IPv6Interface
from typing import Any, Callable, Type, Union

from framework.config import (
    BuildTargetConfiguration,
    ExecutionConfiguration,
    NodeConfiguration,
)
from framework.logger import DTSLOG, getLogger
from framework.remote_session import InteractiveShellType, OSSession, create_session
from framework.settings import SETTINGS

from .hw import (
    LogicalCore,
    LogicalCoreCount,
    LogicalCoreList,
    LogicalCoreListFilter,
    VirtualDevice,
    lcore_filter,
)
from .hw.port import Port


class Node(ABC):
    """
    Basic class for node management. This class implements methods that
    manage a node, such as information gathering (of CPU/PCI/NIC) and
    environment setup.
    """

    main_session: OSSession
    config: NodeConfiguration
    name: str
    lcores: list[LogicalCore]
    ports: list[Port]
    _logger: DTSLOG
    _other_sessions: list[OSSession]
    _execution_config: ExecutionConfiguration
    virtual_devices: list[VirtualDevice]

    def __init__(self, node_config: NodeConfiguration):
        self.config = node_config
        self.name = node_config.name
        self._logger = getLogger(self.name)
        self.main_session = create_session(self.config, self.name, self._logger)

        self._logger.info(f"Connected to node: {self.name}")

        self._get_remote_cpus()
        # filter the node lcores according to user config
        self.lcores = LogicalCoreListFilter(
            self.lcores, LogicalCoreList(self.config.lcores)
        ).filter()

        self._other_sessions = []
        self.virtual_devices = []
        self._init_ports()

    def _init_ports(self) -> None:
        self.ports = [Port(self.name, port_config) for port_config in self.config.ports]
        self.main_session.update_ports(self.ports)
        for port in self.ports:
            self.configure_port_state(port)

    def set_up_execution(self, execution_config: ExecutionConfiguration) -> None:
        """
        Perform the execution setup that will be done for each execution
        this node is part of.
        """
        self._setup_hugepages()
        self._set_up_execution(execution_config)
        self._execution_config = execution_config
        for vdev in execution_config.vdevs:
            self.virtual_devices.append(VirtualDevice(vdev))

    def _set_up_execution(self, execution_config: ExecutionConfiguration) -> None:
        """
        This method exists to be optionally overwritten by derived classes and
        is not decorated so that the derived class doesn't have to use the decorator.
        """

    def tear_down_execution(self) -> None:
        """
        Perform the execution teardown that will be done after each execution
        this node is part of concludes.
        """
        self.virtual_devices = []
        self._tear_down_execution()

    def _tear_down_execution(self) -> None:
        """
        This method exists to be optionally overwritten by derived classes and
        is not decorated so that the derived class doesn't have to use the decorator.
        """

    def set_up_build_target(
        self, build_target_config: BuildTargetConfiguration
    ) -> None:
        """
        Perform the build target setup that will be done for each build target
        tested on this node.
        """
        self._set_up_build_target(build_target_config)

    def _set_up_build_target(
        self, build_target_config: BuildTargetConfiguration
    ) -> None:
        """
        This method exists to be optionally overwritten by derived classes and
        is not decorated so that the derived class doesn't have to use the decorator.
        """

    def tear_down_build_target(self) -> None:
        """
        Perform the build target teardown that will be done after each build target
        tested on this node.
        """
        self._tear_down_build_target()

    def _tear_down_build_target(self) -> None:
        """
        This method exists to be optionally overwritten by derived classes and
        is not decorated so that the derived class doesn't have to use the decorator.
        """

    def create_session(self, name: str) -> OSSession:
        """
        Create and return a new OSSession tailored to the remote OS.
        """
        session_name = f"{self.name} {name}"
        connection = create_session(
            self.config,
            session_name,
            getLogger(session_name, node=self.name),
        )
        self._other_sessions.append(connection)
        return connection

    def create_interactive_shell(
        self,
        shell_cls: Type[InteractiveShellType],
        timeout: float = SETTINGS.timeout,
        privileged: bool = False,
        app_args: str = "",
    ) -> InteractiveShellType:
        """Create a handler for an interactive session.

        Instantiate shell_cls according to the remote OS specifics.

        Args:
            shell_cls: The class of the shell.
            timeout: Timeout for reading output from the SSH channel. If you are
                reading from the buffer and don't receive any data within the timeout
                it will throw an error.
            privileged: Whether to run the shell with administrative privileges.
            app_args: The arguments to be passed to the application.
        Returns:
            Instance of the desired interactive application.
        """
        if not shell_cls.dpdk_app:
            shell_cls.path = self.main_session.join_remote_path(shell_cls.path)

        return self.main_session.create_interactive_shell(
            shell_cls,
            app_args,
            timeout,
            privileged,
        )

    def filter_lcores(
        self,
        filter_specifier: LogicalCoreCount | LogicalCoreList,
        ascending: bool = True,
    ) -> list[LogicalCore]:
        """
        Filter the LogicalCores found on the Node according to
        a LogicalCoreCount or a LogicalCoreList.

        If ascending is True, use cores with the lowest numerical id first
        and continue in ascending order. If False, start with the highest
        id and continue in descending order. This ordering affects which
        sockets to consider first as well.
        """
        self._logger.debug(f"Filtering {filter_specifier} from {self.lcores}.")
        return lcore_filter(
            self.lcores,
            filter_specifier,
            ascending,
        ).filter()

    def _get_remote_cpus(self) -> None:
        """
        Scan CPUs in the remote OS and store a list of LogicalCores.
        """
        self._logger.info("Getting CPU information.")
        self.lcores = self.main_session.get_remote_cpus(self.config.use_first_core)

    def _setup_hugepages(self):
        """
        Setup hugepages on the Node. Different architectures can supply different
        amounts of memory for hugepages and numa-based hugepage allocation may need
        to be considered.
        """
        if self.config.hugepages:
            self.main_session.setup_hugepages(
                self.config.hugepages.amount, self.config.hugepages.force_first_numa
            )

    def configure_port_state(self, port: Port, enable: bool = True) -> None:
        """
        Enable/disable port.
        """
        self.main_session.configure_port_state(port, enable)

    def configure_port_ip_address(
        self,
        address: Union[IPv4Interface, IPv6Interface],
        port: Port,
        delete: bool = False,
    ) -> None:
        """
        Configure the IP address of a port on this node.
        """
        self.main_session.configure_port_ip_address(address, port, delete)

    def close(self) -> None:
        """
        Close all connections and free other resources.
        """
        if self.main_session:
            self.main_session.close()
        for session in self._other_sessions:
            session.close()
        self._logger.logger_exit()

    @staticmethod
    def skip_setup(func: Callable[..., Any]) -> Callable[..., Any]:
        if SETTINGS.skip_setup:
            return lambda *args: None
        else:
            return func
