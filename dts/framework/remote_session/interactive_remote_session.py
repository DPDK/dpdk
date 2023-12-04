# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 University of New Hampshire

"""Handler for an SSH session dedicated to interactive shells."""

import socket
import traceback

from paramiko import AutoAddPolicy, SSHClient, Transport  # type: ignore[import]
from paramiko.ssh_exception import (  # type: ignore[import]
    AuthenticationException,
    BadHostKeyException,
    NoValidConnectionsError,
    SSHException,
)

from framework.config import NodeConfiguration
from framework.exception import SSHConnectionError
from framework.logger import DTSLOG


class InteractiveRemoteSession:
    """SSH connection dedicated to interactive applications.

    The connection is created using `paramiko <https://docs.paramiko.org/en/latest/>`_
    and is a persistent connection to the host. This class defines the methods for connecting
    to the node and configures the connection to send "keep alive" packets every 30 seconds.
    Because paramiko attempts to use SSH keys to establish a connection first, providing
    a password is optional. This session is utilized by InteractiveShells
    and cannot be interacted with directly.

    Attributes:
        hostname: The hostname that will be used to initialize a connection to the node.
        ip: A subsection of `hostname` that removes the port for the connection if there
            is one. If there is no port, this will be the same as hostname.
        port: Port to use for the ssh connection. This will be extracted from `hostname`
            if there is a port included, otherwise it will default to ``22``.
        username: User to connect to the node with.
        password: Password of the user connecting to the host. This will default to an
            empty string if a password is not provided.
        session: The underlying paramiko connection.

    Raises:
        SSHConnectionError: There is an error creating the SSH connection.
    """

    hostname: str
    ip: str
    port: int
    username: str
    password: str
    session: SSHClient
    _logger: DTSLOG
    _node_config: NodeConfiguration
    _transport: Transport | None

    def __init__(self, node_config: NodeConfiguration, logger: DTSLOG) -> None:
        """Connect to the node during initialization.

        Args:
            node_config: The test run configuration of the node to connect to.
            logger: The logger instance this session will use.
        """
        self._node_config = node_config
        self._logger = logger
        self.hostname = node_config.hostname
        self.username = node_config.user
        self.password = node_config.password if node_config.password else ""
        port = "22"
        self.ip = node_config.hostname
        if ":" in node_config.hostname:
            self.ip, port = node_config.hostname.split(":")
        self.port = int(port)
        self._logger.info(
            f"Initializing interactive connection for {self.username}@{self.hostname}"
        )
        self._connect()
        self._logger.info(f"Interactive connection successful for {self.username}@{self.hostname}")

    def _connect(self) -> None:
        """Establish a connection to the node.

        Connection attempts can be retried up to 10 times if certain errors are
        encountered (NoValidConnectionsError, socket.error, SSHException). If a
        connection is made, a 30 second "keep alive" interval will be set on the
        session.

        Raises:
            SSHConnectionError: Connection cannot be established.
        """
        client = SSHClient()
        client.set_missing_host_key_policy(AutoAddPolicy)
        self.session = client
        retry_attempts = 10
        for retry_attempt in range(retry_attempts):
            try:
                client.connect(
                    self.ip,
                    username=self.username,
                    port=self.port,
                    password=self.password,
                    timeout=20 if self.port else 10,
                )
            except (TypeError, BadHostKeyException, AuthenticationException) as e:
                self._logger.exception(e)
                raise SSHConnectionError(self.hostname) from e
            except (NoValidConnectionsError, socket.error, SSHException) as e:
                self._logger.debug(traceback.format_exc())
                self._logger.warning(e)
                self._logger.info(
                    f"Retrying interactive session connection: retry number {retry_attempt +1}"
                )
            else:
                break
        else:
            raise SSHConnectionError(self.hostname)
        # Interactive sessions are used on an "as needed" basis so we have
        # to set a keepalive
        self._transport = self.session.get_transport()
        if self._transport is not None:
            self._transport.set_keepalive(30)
