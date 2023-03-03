# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation
# Copyright(c) 2022-2023 PANTHEON.tech s.r.o.
# Copyright(c) 2022-2023 University of New Hampshire

import dataclasses
from abc import ABC, abstractmethod

from framework.config import NodeConfiguration
from framework.logger import DTSLOG
from framework.settings import SETTINGS


@dataclasses.dataclass(slots=True, frozen=True)
class HistoryRecord:
    name: str
    command: str
    output: str | int


class RemoteSession(ABC):
    """
    The base class for defining which methods must be implemented in order to connect
    to a remote host (node) and maintain a remote session. The derived classes are
    supposed to implement/use some underlying transport protocol (e.g. SSH) to
    implement the methods. On top of that, it provides some basic services common to
    all derived classes, such as keeping history and logging what's being executed
    on the remote node.
    """

    name: str
    hostname: str
    ip: str
    port: int | None
    username: str
    password: str
    history: list[HistoryRecord]
    _logger: DTSLOG
    _node_config: NodeConfiguration

    def __init__(
        self,
        node_config: NodeConfiguration,
        session_name: str,
        logger: DTSLOG,
    ):
        self._node_config = node_config

        self.name = session_name
        self.hostname = node_config.hostname
        self.ip = self.hostname
        self.port = None
        if ":" in self.hostname:
            self.ip, port = self.hostname.split(":")
            self.port = int(port)
        self.username = node_config.user
        self.password = node_config.password or ""
        self.history = []

        self._logger = logger
        self._logger.info(f"Connecting to {self.username}@{self.hostname}.")
        self._connect()
        self._logger.info(f"Connection to {self.username}@{self.hostname} successful.")

    @abstractmethod
    def _connect(self) -> None:
        """
        Create connection to assigned node.
        """

    def send_command(self, command: str, timeout: float = SETTINGS.timeout) -> str:
        """
        Send a command and return the output.
        """
        self._logger.info(f"Sending: {command}")
        out = self._send_command(command, timeout)
        self._logger.debug(f"Received from {command}: {out}")
        self._history_add(command=command, output=out)
        return out

    @abstractmethod
    def _send_command(self, command: str, timeout: float) -> str:
        """
        Use the underlying protocol to execute the command and return the output
        of the command.
        """

    def _history_add(self, command: str, output: str) -> None:
        self.history.append(
            HistoryRecord(name=self.name, command=command, output=output)
        )

    def close(self, force: bool = False) -> None:
        """
        Close the remote session and free all used resources.
        """
        self._logger.logger_exit()
        self._close(force)

    @abstractmethod
    def _close(self, force: bool = False) -> None:
        """
        Execute protocol specific steps needed to close the session properly.
        """

    @abstractmethod
    def is_alive(self) -> bool:
        """
        Check whether the remote session is still responding.
        """
