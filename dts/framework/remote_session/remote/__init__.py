# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 PANTHEON.tech s.r.o.
# Copyright(c) 2023 University of New Hampshire

# pylama:ignore=W0611

from framework.config import NodeConfiguration
from framework.logger import DTSLOG

from .interactive_remote_session import InteractiveRemoteSession
from .interactive_shell import InteractiveShell
from .python_shell import PythonShell
from .remote_session import CommandResult, RemoteSession
from .ssh_session import SSHSession
from .testpmd_shell import TestPmdDevice, TestPmdShell


def create_remote_session(
    node_config: NodeConfiguration, name: str, logger: DTSLOG
) -> RemoteSession:
    return SSHSession(node_config, name, logger)


def create_interactive_session(
    node_config: NodeConfiguration, logger: DTSLOG
) -> InteractiveRemoteSession:
    return InteractiveRemoteSession(node_config, logger)
