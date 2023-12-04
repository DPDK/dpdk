# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation
# Copyright(c) 2022-2023 PANTHEON.tech s.r.o.
# Copyright(c) 2022-2023 University of New Hampshire

"""Common functionality for node management.

A node is any host/server DTS connects to.

The base class, :class:`Node`, provides features common to all nodes and is supposed
to be extended by subclasses with features specific to each node type.
The :func:`~Node.skip_setup` decorator can be used without subclassing.
"""

from abc import ABC
from ipaddress import IPv4Interface, IPv6Interface
from typing import Any, Callable, Type, Union

from framework.config import (
    OS,
    BuildTargetConfiguration,
    ExecutionConfiguration,
    NodeConfiguration,
)
from framework.exception import ConfigurationError
from framework.logger import DTSLOG, getLogger
from framework.settings import SETTINGS

from .cpu import (
    LogicalCore,
    LogicalCoreCount,
    LogicalCoreList,
    LogicalCoreListFilter,
    lcore_filter,
)
from .linux_session import LinuxSession
from .os_session import InteractiveShellType, OSSession
from .port import Port
from .virtual_device import VirtualDevice


class Node(ABC):
    """The base class for node management.

    It shouldn't be instantiated, but rather subclassed.
    It implements common methods to manage any node:

        * Connection to the node,
        * Hugepages setup.

    Attributes:
        main_session: The primary OS-aware remote session used to communicate with the node.
        config: The node configuration.
        name: The name of the node.
        lcores: The list of logical cores that DTS can use on the node.
            It's derived from logical cores present on the node and the test run configuration.
        ports: The ports of this node specified in the test run configuration.
        virtual_devices: The virtual devices used on the node.
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
        """Connect to the node and gather info during initialization.

        Extra gathered information:

        * The list of available logical CPUs. This is then filtered by
          the ``lcores`` configuration in the YAML test run configuration file,
        * Information about ports from the YAML test run configuration file.

        Args:
            node_config: The node's test run configuration.
        """
        self.config = node_config
        self.name = node_config.name
        self._logger = getLogger(self.name)
        self.main_session = create_session(self.config, self.name, self._logger)

        self._logger.info(f"Connected to node: {self.name}")

        self._get_remote_cpus()
        # filter the node lcores according to the test run configuration
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
        """Execution setup steps.

        Configure hugepages and call :meth:`_set_up_execution` where
        the rest of the configuration steps (if any) are implemented.

        Args:
            execution_config: The execution test run configuration according to which
                the setup steps will be taken.
        """
        self._setup_hugepages()
        self._set_up_execution(execution_config)
        self._execution_config = execution_config
        for vdev in execution_config.vdevs:
            self.virtual_devices.append(VirtualDevice(vdev))

    def _set_up_execution(self, execution_config: ExecutionConfiguration) -> None:
        """Optional additional execution setup steps for subclasses.

        Subclasses should override this if they need to add additional execution setup steps.
        """

    def tear_down_execution(self) -> None:
        """Execution teardown steps.

        There are currently no common execution teardown steps common to all DTS node types.
        """
        self.virtual_devices = []
        self._tear_down_execution()

    def _tear_down_execution(self) -> None:
        """Optional additional execution teardown steps for subclasses.

        Subclasses should override this if they need to add additional execution teardown steps.
        """

    def set_up_build_target(self, build_target_config: BuildTargetConfiguration) -> None:
        """Build target setup steps.

        There are currently no common build target setup steps common to all DTS node types.

        Args:
            build_target_config: The build target test run configuration according to which
                the setup steps will be taken.
        """
        self._set_up_build_target(build_target_config)

    def _set_up_build_target(self, build_target_config: BuildTargetConfiguration) -> None:
        """Optional additional build target setup steps for subclasses.

        Subclasses should override this if they need to add additional build target setup steps.
        """

    def tear_down_build_target(self) -> None:
        """Build target teardown steps.

        There are currently no common build target teardown steps common to all DTS node types.
        """
        self._tear_down_build_target()

    def _tear_down_build_target(self) -> None:
        """Optional additional build target teardown steps for subclasses.

        Subclasses should override this if they need to add additional build target teardown steps.
        """

    def create_session(self, name: str) -> OSSession:
        """Create and return a new OS-aware remote session.

        The returned session won't be used by the node creating it. The session must be used by
        the caller. The session will be maintained for the entire lifecycle of the node object,
        at the end of which the session will be cleaned up automatically.

        Note:
            Any number of these supplementary sessions may be created.

        Args:
            name: The name of the session.

        Returns:
            A new OS-aware remote session.
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
        """Factory for interactive session handlers.

        Instantiate `shell_cls` according to the remote OS specifics.

        Args:
            shell_cls: The class of the shell.
            timeout: Timeout for reading output from the SSH channel. If you are reading from
                the buffer and don't receive any data within the timeout it will throw an error.
            privileged: Whether to run the shell with administrative privileges.
            app_args: The arguments to be passed to the application.

        Returns:
            An instance of the desired interactive application shell.
        """
        if not shell_cls.dpdk_app:
            shell_cls.path = self.main_session.join_remote_path(shell_cls.path)

        return self.main_session.create_interactive_shell(
            shell_cls,
            timeout,
            privileged,
            app_args,
        )

    def filter_lcores(
        self,
        filter_specifier: LogicalCoreCount | LogicalCoreList,
        ascending: bool = True,
    ) -> list[LogicalCore]:
        """Filter the node's logical cores that DTS can use.

        Logical cores that DTS can use are the ones that are present on the node, but filtered
        according to the test run configuration. The `filter_specifier` will filter cores from
        those logical cores.

        Args:
            filter_specifier: Two different filters can be used, one that specifies the number
                of logical cores per core, cores per socket and the number of sockets,
                and another one that specifies a logical core list.
            ascending: If :data:`True`, use cores with the lowest numerical id first and continue
                in ascending order. If :data:`False`, start with the highest id and continue
                in descending order. This ordering affects which sockets to consider first as well.

        Returns:
            The filtered logical cores.
        """
        self._logger.debug(f"Filtering {filter_specifier} from {self.lcores}.")
        return lcore_filter(
            self.lcores,
            filter_specifier,
            ascending,
        ).filter()

    def _get_remote_cpus(self) -> None:
        """Scan CPUs in the remote OS and store a list of LogicalCores."""
        self._logger.info("Getting CPU information.")
        self.lcores = self.main_session.get_remote_cpus(self.config.use_first_core)

    def _setup_hugepages(self) -> None:
        """Setup hugepages on the node.

        Configure the hugepages only if they're specified in the node's test run configuration.
        """
        if self.config.hugepages:
            self.main_session.setup_hugepages(
                self.config.hugepages.amount, self.config.hugepages.force_first_numa
            )

    def configure_port_state(self, port: Port, enable: bool = True) -> None:
        """Enable/disable `port`.

        Args:
            port: The port to enable/disable.
            enable: :data:`True` to enable, :data:`False` to disable.
        """
        self.main_session.configure_port_state(port, enable)

    def configure_port_ip_address(
        self,
        address: Union[IPv4Interface, IPv6Interface],
        port: Port,
        delete: bool = False,
    ) -> None:
        """Add an IP address to `port` on this node.

        Args:
            address: The IP address with mask in CIDR format. Can be either IPv4 or IPv6.
            port: The port to which to add the address.
            delete: If :data:`True`, will delete the address from the port instead of adding it.
        """
        self.main_session.configure_port_ip_address(address, port, delete)

    def close(self) -> None:
        """Close all connections and free other resources."""
        if self.main_session:
            self.main_session.close()
        for session in self._other_sessions:
            session.close()
        self._logger.logger_exit()

    @staticmethod
    def skip_setup(func: Callable[..., Any]) -> Callable[..., Any]:
        """Skip the decorated function.

        The :option:`--skip-setup` command line argument and the :envvar:`DTS_SKIP_SETUP`
        environment variable enable the decorator.
        """
        if SETTINGS.skip_setup:
            return lambda *args: None
        else:
            return func


def create_session(node_config: NodeConfiguration, name: str, logger: DTSLOG) -> OSSession:
    """Factory for OS-aware sessions.

    Args:
        node_config: The test run configuration of the node to connect to.
        name: The name of the session.
        logger: The logger instance this session will use.
    """
    match node_config.os:
        case OS.linux:
            return LinuxSession(node_config, name, logger)
        case _:
            raise ConfigurationError(f"Unsupported OS {node_config.os}")
