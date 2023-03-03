# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 PANTHEON.tech s.r.o.

# pylama:ignore=W0611

from framework.config import NodeConfiguration
from framework.logger import DTSLOG

from .remote_session import CommandResult, RemoteSession
from .ssh_session import SSHSession


def create_remote_session(
    node_config: NodeConfiguration, name: str, logger: DTSLOG
) -> RemoteSession:
    return SSHSession(node_config, name, logger)
