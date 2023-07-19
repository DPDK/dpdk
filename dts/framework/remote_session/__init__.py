# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 PANTHEON.tech s.r.o.
# Copyright(c) 2023 University of New Hampshire

"""
The package provides modules for managing remote connections to a remote host (node),
differentiated by OS.
The package provides a factory function, create_session, that returns the appropriate
remote connection based on the passed configuration. The differences are in the
underlying transport protocol (e.g. SSH) and remote OS (e.g. Linux).
"""

# pylama:ignore=W0611

from framework.config import OS, NodeConfiguration
from framework.exception import ConfigurationError
from framework.logger import DTSLOG

from .linux_session import LinuxSession
from .os_session import InteractiveShellType, OSSession
from .remote import (
    CommandResult,
    InteractiveRemoteSession,
    InteractiveShell,
    PythonShell,
    RemoteSession,
    SSHSession,
    TestPmdDevice,
    TestPmdShell,
)


def create_session(
    node_config: NodeConfiguration, name: str, logger: DTSLOG
) -> OSSession:
    match node_config.os:
        case OS.linux:
            return LinuxSession(node_config, name, logger)
        case _:
            raise ConfigurationError(f"Unsupported OS {node_config.os}")
