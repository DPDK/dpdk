# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 PANTHEON.tech s.r.o.
# Copyright(c) 2023 University of New Hampshire

"""Remote interactive and non-interactive sessions.

This package provides modules for managing remote connections to a remote host (node).

The non-interactive sessions send commands and return their output and exit code.

The interactive sessions open an interactive shell which is continuously open,
allowing it to send and receive data within that particular shell.
"""

from framework.config.node import NodeConfiguration
from framework.logger import DTSLogger

from .interactive_remote_session import InteractiveRemoteSession
from .remote_session import RemoteSession


def create_remote_session(
    node_config: NodeConfiguration, name: str, logger: DTSLogger
) -> RemoteSession:
    """Factory for non-interactive remote sessions.

    The function returns an SSH session, but will be extended if support
    for other protocols is added.

    Args:
        node_config: The test run configuration of the node to connect to.
        name: The name of the session.
        logger: The logger instance this session will use.

    Returns:
        The SSH remote session.
    """
    return RemoteSession(node_config, name, logger)


def create_interactive_session(
    node_config: NodeConfiguration, logger: DTSLogger
) -> InteractiveRemoteSession:
    """Factory for interactive remote sessions.

    The function returns an interactive SSH session, but will be extended if support
    for other protocols is added.

    Args:
        node_config: The test run configuration of the node to connect to.
        logger: The logger instance this session will use.

    Returns:
        The interactive SSH remote session.
    """
    return InteractiveRemoteSession(node_config, logger)
