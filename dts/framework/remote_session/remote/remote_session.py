# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation
# Copyright(c) 2022-2023 PANTHEON.tech s.r.o.
# Copyright(c) 2022-2023 University of New Hampshire

import dataclasses
from abc import ABC, abstractmethod
from pathlib import PurePath

from framework.config import NodeConfiguration
from framework.exception import RemoteCommandExecutionError
from framework.logger import DTSLOG
from framework.settings import SETTINGS
from framework.utils import EnvVarsDict


@dataclasses.dataclass(slots=True, frozen=True)
class CommandResult:
    """
    The result of remote execution of a command.
    """

    name: str
    command: str
    stdout: str
    stderr: str
    return_code: int

    def __str__(self) -> str:
        return (
            f"stdout: '{self.stdout}'\n"
            f"stderr: '{self.stderr}'\n"
            f"return_code: '{self.return_code}'"
        )


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
    history: list[CommandResult]
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

    def send_command(
        self,
        command: str,
        timeout: float = SETTINGS.timeout,
        verify: bool = False,
        env: EnvVarsDict | None = None,
    ) -> CommandResult:
        """
        Send a command to the connected node using optional env vars
        and return CommandResult.
        If verify is True, check the return code of the executed command
        and raise a RemoteCommandExecutionError if the command failed.
        """
        self._logger.info(
            f"Sending: '{command}'" + (f" with env vars: '{env}'" if env else "")
        )
        result = self._send_command(command, timeout, env)
        if verify and result.return_code:
            self._logger.debug(
                f"Command '{command}' failed with return code '{result.return_code}'"
            )
            self._logger.debug(f"stdout: '{result.stdout}'")
            self._logger.debug(f"stderr: '{result.stderr}'")
            raise RemoteCommandExecutionError(command, result.return_code)
        self._logger.debug(f"Received from '{command}':\n{result}")
        self.history.append(result)
        return result

    @abstractmethod
    def _send_command(
        self, command: str, timeout: float, env: EnvVarsDict | None
    ) -> CommandResult:
        """
        Use the underlying protocol to execute the command using optional env vars
        and return CommandResult.
        """

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

    @abstractmethod
    def copy_file(
        self,
        source_file: str | PurePath,
        destination_file: str | PurePath,
        source_remote: bool = False,
    ) -> None:
        """
        Copy source_file from local filesystem to destination_file on the remote Node
        associated with the remote session.
        If source_remote is True, reverse the direction - copy source_file from the
        associated Node to destination_file on local filesystem.
        """
