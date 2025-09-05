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

from functools import cached_property
from pathlib import PurePath
from typing import Literal, TypeAlias

from framework.config.node import (
    OS,
    NodeConfiguration,
)
from framework.exception import ConfigurationError, InternalError
from framework.logger import DTSLogger, get_dts_logger

from .cpu import Architecture, LogicalCore
from .linux_session import LinuxSession
from .os_session import OSSession, OSSessionInfo
from .port import Port


class Node:
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
    _node_info: OSSessionInfo | None
    _compiler_version: str | None
    _setup: bool

    def __init__(self, node_config: NodeConfiguration) -> None:
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
        self._setup = False
        self.ports = [Port(self, port_config) for port_config in self.config.ports]
        self._logger.info(f"Created node: {self.name}")

    def setup(self) -> None:
        """Node setup."""
        if self._setup:
            return

        self._setup_hugepages()
        self.tmp_dir = self.main_session.create_tmp_dir()
        self._setup = True

    def teardown(self) -> None:
        """Node teardown."""
        if not self._setup:
            return

        self.main_session.remove_remote_dir(self.tmp_dir)
        del self.tmp_dir
        self._setup = False

    @cached_property
    def tmp_dir(self) -> PurePath:
        """Path to the temporary directory.

        Raises:
            InternalError: If called before the node has been setup.
        """
        raise InternalError("Temporary directory requested before setup.")

    @cached_property
    def ports_by_name(self) -> dict[str, Port]:
        """Ports mapped by the name assigned at configuration."""
        return {port.name: port for port in self.ports}

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

    def _get_remote_cpus(self) -> None:
        """Scan CPUs in the remote OS and store a list of LogicalCores."""
        self._logger.info("Getting CPU information.")
        self.lcores = self.main_session.get_remote_cpus()

    @cached_property
    def node_info(self) -> OSSessionInfo:
        """Additional node information."""
        return self.main_session.get_node_info()

    @property
    def compiler_version(self) -> str | None:
        """The node's compiler version."""
        if self._compiler_version is None:
            self._logger.warning("The `compiler_version` is None because a pre-built DPDK is used.")

        return self._compiler_version

    @compiler_version.setter
    def compiler_version(self, value: str) -> None:
        """Set the `compiler_version` used on the SUT node.

        Args:
            value: The node's compiler version.
        """
        self._compiler_version = value

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


LocalNodeIdentifier: TypeAlias = Literal["local"]
"""Local node identifier for testbed model."""

RemoteNodeIdentifier: TypeAlias = Literal["sut", "tg"]
"""Remote node identifiers for testbed model."""

NodeIdentifier: TypeAlias = Literal["local", "sut", "tg"]
"""Node identifiers for testbed model."""


def get_node(node_identifier: NodeIdentifier) -> Node | None:
    """Get the node based on the identifier.

    Args:
        node_identifier: The identifier of the node.

    Returns:
        The node object corresponding to the identifier, or :data:`None` if the identifier is
            "local".

    Raises:
        InternalError: If the node identifier is unknown.
    """
    if node_identifier == "local":
        return None

    from framework.context import get_ctx

    ctx = get_ctx()
    if node_identifier == "sut":
        return ctx.sut_node
    elif node_identifier == "tg":
        return ctx.tg_node
    else:
        raise InternalError(f"Unknown node identifier: {node_identifier}")
