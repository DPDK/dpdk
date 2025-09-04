# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation
# Copyright(c) 2022-2023 PANTHEON.tech s.r.o.
# Copyright(c) 2022-2023 University of New Hampshire
# Copyright(c) 2024 Arm Limited

"""SSH remote session."""

import socket
import traceback
from dataclasses import InitVar, dataclass, field
from pathlib import Path, PurePath

from fabric import Connection  # type: ignore[import-untyped]
from invoke.exceptions import (
    CommandTimedOut,
    ThreadException,
    UnexpectedExit,
)
from paramiko.ssh_exception import (
    AuthenticationException,
    BadHostKeyException,
    NoValidConnectionsError,
    SSHException,
)

from framework.config.node import NodeConfiguration
from framework.exception import (
    RemoteCommandExecutionError,
    SSHConnectionError,
    SSHSessionDeadError,
    SSHTimeoutError,
)
from framework.logger import DTSLogger
from framework.settings import SETTINGS


@dataclass(slots=True, frozen=True)
class CommandResult:
    """The result of remote execution of a command.

    Attributes:
        name: The name of the session that executed the command.
        command: The executed command.
        stdout: The standard output the command produced.
        stderr: The standard error output the command produced.
        return_code: The return code the command exited with.
    """

    name: str
    command: str
    init_stdout: InitVar[str]
    init_stderr: InitVar[str]
    return_code: int
    stdout: str = field(init=False)
    stderr: str = field(init=False)

    def __post_init__(self, init_stdout: str, init_stderr: str) -> None:
        """Strip the whitespaces from stdout and stderr.

        The generated __init__ method uses object.__setattr__() when the dataclass is frozen,
        so that's what we use here as well.

        In order to get access to dataclass fields in the __post_init__ method,
        we have to type them as InitVars. These InitVars are included in the __init__ method's
        signature, so we have to exclude the actual stdout and stderr fields
        from the __init__ method's signature, so that we have the proper number of arguments.
        """
        object.__setattr__(self, "stdout", init_stdout.strip())
        object.__setattr__(self, "stderr", init_stderr.strip())

    def __str__(self) -> str:
        """Format the command outputs."""
        return (
            f"stdout: '{self.stdout}'\n"
            f"stderr: '{self.stderr}'\n"
            f"return_code: '{self.return_code}'"
        )


class RemoteSession:
    """Non-interactive remote session.

    The connection is implemented with
    `the Fabric Python library <https://docs.fabfile.org/en/latest/>`_.

    Attributes:
        name: The name of the session.
        hostname: The node's hostname. Could be an IP (possibly with port, separated by a colon)
            or a domain name.
        ip: The IP address of the node or a domain name, whichever was used in `hostname`.
        port: The port of the node, if given in `hostname`.
        username: The username used in the connection.
        password: The password used in the connection. Most frequently empty,
            as the use of passwords is discouraged.
        history: The executed commands during this session.
        session: The underlying Fabric SSH session.
    """

    name: str
    hostname: str
    ip: str
    port: int | None
    username: str
    password: str
    history: list[CommandResult]
    session: Connection
    _logger: DTSLogger
    _node_config: NodeConfiguration

    def __init__(
        self,
        node_config: NodeConfiguration,
        session_name: str,
        logger: DTSLogger,
    ):
        """Connect to the node during initialization.

        Args:
            node_config: The test run configuration of the node to connect to.
            session_name: The name of the session.
            logger: The logger instance this session will use.

        Raises:
            SSHConnectionError: If the connection to the node was not successful.
        """
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

    def _connect(self) -> None:
        """Create a connection to the node.

        The implementation must assign the established session to self.session.

        The implementation must except all exceptions and convert them to an SSHConnectionError.

        The implementation may optionally implement retry attempts.

        Raises:
            SSHConnectionError: If the connection to the node was not successful.
        """
        errors = []
        retry_attempts = 10
        login_timeout = 20 if self.port else 10
        for retry_attempt in range(retry_attempts):
            try:
                self.session = Connection(
                    self.ip,
                    user=self.username,
                    port=self.port,
                    connect_kwargs={"password": self.password},
                    connect_timeout=login_timeout,
                )
                self.session.open()

            except (ValueError, BadHostKeyException, AuthenticationException) as e:
                self._logger.exception(e)
                raise SSHConnectionError(self.hostname) from e

            except (NoValidConnectionsError, socket.error, SSHException) as e:
                self._logger.debug(traceback.format_exc())
                self._logger.warning(e)

                error = repr(e)
                if error not in errors:
                    errors.append(error)

                self._logger.info(f"Retrying connection: retry number {retry_attempt + 1}.")

            else:
                break
        else:
            raise SSHConnectionError(self.hostname, errors)

    def send_command(
        self,
        command: str,
        timeout: float = SETTINGS.timeout,
        verify: bool = False,
        env: dict | None = None,
    ) -> CommandResult:
        """Send `command` to the connected node.

        The :option:`--timeout` command line argument and the :envvar:`DTS_TIMEOUT`
        environment variable configure the timeout of command execution.

        Args:
            command: The command to execute.
            timeout: Wait at most this long in seconds for `command` execution to complete.
            verify: If :data:`True`, will check the exit code of `command`.
            env: A dictionary with environment variables to be used with `command` execution.

        Raises:
            SSHSessionDeadError: If the session isn't alive when sending `command`.
            SSHTimeoutError: If `command` execution timed out.
            RemoteCommandExecutionError: If verify is :data:`True` and `command` execution failed.

        Returns:
            The output of the command along with the return code.
        """
        self._logger.info(f"Sending: '{command}'" + (f" with env vars: '{env}'" if env else ""))

        try:
            output = self.session.run(command, env=env, warn=True, hide=True, timeout=timeout)
        except (UnexpectedExit, ThreadException) as e:
            self._logger.exception(e)
            raise SSHSessionDeadError(self.hostname) from e
        except CommandTimedOut as e:
            self._logger.exception(e)
            raise SSHTimeoutError(command) from e

        result = CommandResult(self.name, command, output.stdout, output.stderr, output.return_code)

        if verify and result.return_code:
            self._logger.debug(
                f"Command '{command}' failed with return code '{result.return_code}'"
            )
            self._logger.debug(f"stdout: '{result.stdout}'")
            self._logger.debug(f"stderr: '{result.stderr}'")
            raise RemoteCommandExecutionError(command, result.stderr, result.return_code)
        self._logger.debug(f"Received from '{command}':\n{result}")
        self.history.append(result)
        return result

    def is_alive(self) -> bool:
        """Check whether the remote session is still responding."""
        return self.session.is_connected

    def copy_from(self, source_file: str | PurePath, destination_dir: str | Path) -> None:
        """Copy a file from the remote Node to the local filesystem.

        Copy `source_file` from the remote Node associated with this remote session
        to `destination_dir` on the local filesystem.

        Args:
            source_file: The file on the remote Node.
            destination_dir: The directory path on the local filesystem where the `source_file`
                will be saved.
        """
        self.session.get(str(source_file), str(destination_dir))

    def copy_to(self, source_file: str | Path, destination_dir: str | PurePath) -> None:
        """Copy a file from local filesystem to the remote Node.

        Copy `source_file` from local filesystem to `destination_dir` on the remote Node
        associated with this remote session.

        Args:
            source_file: The file on the local filesystem.
            destination_dir: The directory path on the remote Node where the `source_file`
                will be saved.
        """
        self.session.put(str(source_file), str(destination_dir))

    def close(self) -> None:
        """Close the remote session and free all used resources."""
        self.session.close()
