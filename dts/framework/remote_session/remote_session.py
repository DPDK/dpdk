# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation
# Copyright(c) 2022 PANTHEON.tech s.r.o.
# Copyright(c) 2022 University of New Hampshire

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
    name: str
    hostname: str
    ip: str
    port: int | None
    username: str
    password: str
    logger: DTSLOG
    history: list[HistoryRecord]
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
        self.logger = logger
        self.history = []

        self.logger.info(f"Connecting to {self.username}@{self.hostname}.")
        self._connect()
        self.logger.info(f"Connection to {self.username}@{self.hostname} successful.")

    @abstractmethod
    def _connect(self) -> None:
        """
        Create connection to assigned node.
        """
        pass

    def send_command(self, command: str, timeout: float = SETTINGS.timeout) -> str:
        self.logger.info(f"Sending: {command}")
        out = self._send_command(command, timeout)
        self.logger.debug(f"Received from {command}: {out}")
        self._history_add(command=command, output=out)
        return out

    @abstractmethod
    def _send_command(self, command: str, timeout: float) -> str:
        """
        Send a command and return the output.
        """

    def _history_add(self, command: str, output: str) -> None:
        self.history.append(
            HistoryRecord(name=self.name, command=command, output=output)
        )

    def close(self, force: bool = False) -> None:
        self.logger.logger_exit()
        self._close(force)

    @abstractmethod
    def _close(self, force: bool = False) -> None:
        """
        Close the remote session, freeing all used resources.
        """

    @abstractmethod
    def is_alive(self) -> bool:
        """
        Check whether the session is still responding.
        """
