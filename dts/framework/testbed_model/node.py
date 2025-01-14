# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation
# Copyright(c) 2022-2023 PANTHEON.tech s.r.o.
# Copyright(c) 2022-2023 University of New Hampshire
# Copyright(c) 2024 Arm Limited

"""Common functionality for node management.

A node is any host/server DTS connects to.

The base class, :class:`Node`, provides features common to all nodes and is supposed
to be extended by subclasses with features specific to each node type.
The :func:`~Node.skip_setup` decorator can be used without subclassing.
"""

from abc import ABC

from framework.config.node import (
    OS,
    NodeConfiguration,
)
from framework.config.test_run import (
    DPDKBuildConfiguration,
    TestRunConfiguration,
)
from framework.exception import ConfigurationError
from framework.logger import DTSLogger, get_dts_logger

from .cpu import Architecture, LogicalCore, LogicalCoreCount, LogicalCoreList, lcore_filter
from .linux_session import LinuxSession
from .os_session import OSSession
from .port import Port


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
    """

    main_session: OSSession
    config: NodeConfiguration
    name: str
    arch: Architecture
    lcores: list[LogicalCore]
    ports: list[Port]
    _logger: DTSLogger
    _other_sessions: list[OSSession]
    _test_run_config: TestRunConfiguration

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
        self._logger = get_dts_logger(self.name)
        self.main_session = create_session(self.config, self.name, self._logger)
        self.arch = Architecture(self.main_session.get_arch_info())
        self._logger.info(f"Connected to node: {self.name}")
        self._get_remote_cpus()
        self._other_sessions = []
        self._init_ports()

    def _init_ports(self) -> None:
        self.ports = [Port(self.name, port_config) for port_config in self.config.ports]
        self.main_session.update_ports(self.ports)

    def set_up_test_run(
        self,
        test_run_config: TestRunConfiguration,
        dpdk_build_config: DPDKBuildConfiguration,
    ) -> None:
        """Test run setup steps.

        Configure hugepages on all DTS node types. Additional steps can be added by
        extending the method in subclasses with the use of super().

        Args:
            test_run_config: A test run configuration according to which
                the setup steps will be taken.
            dpdk_build_config: The build configuration of DPDK.
        """
        self._setup_hugepages()

    def tear_down_test_run(self) -> None:
        """Test run teardown steps.

        There are currently no common execution teardown steps common to all DTS node types.
        Additional steps can be added by extending the method in subclasses with the use of super().
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
            get_dts_logger(session_name),
        )
        self._other_sessions.append(connection)
        return connection

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
        self.lcores = self.main_session.get_remote_cpus()

    def _setup_hugepages(self) -> None:
        """Setup hugepages on the node.

        Configure the hugepages only if they're specified in the node's test run configuration.
        """
        if self.config.hugepages:
            self.main_session.setup_hugepages(
                self.config.hugepages.number_of,
                self.main_session.hugepage_size,
                self.config.hugepages.force_first_numa,
            )

    def close(self) -> None:
        """Close all connections and free other resources."""
        if self.main_session:
            self.main_session.close()
        for session in self._other_sessions:
            session.close()


def create_session(node_config: NodeConfiguration, name: str, logger: DTSLogger) -> OSSession:
    """Factory for OS-aware sessions.

    Args:
        node_config: The test run configuration of the node to connect to.
        name: The name of the session.
        logger: The logger instance this session will use.

    Raises:
        ConfigurationError: If the node's OS is unsupported.
    """
    match node_config.os:
        case OS.linux:
            return LinuxSession(node_config, name, logger)
        case _:
            raise ConfigurationError(f"Unsupported OS {node_config.os}")
