# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 PANTHEON.tech s.r.o.

"""SSH remote session."""

import socket
import traceback
from pathlib import PurePath

from fabric import Connection  # type: ignore[import]
from invoke.exceptions import (  # type: ignore[import]
    CommandTimedOut,
    ThreadException,
    UnexpectedExit,
)
from paramiko.ssh_exception import (  # type: ignore[import]
    AuthenticationException,
    BadHostKeyException,
    NoValidConnectionsError,
    SSHException,
)

from framework.exception import SSHConnectionError, SSHSessionDeadError, SSHTimeoutError

from .remote_session import CommandResult, RemoteSession


class SSHSession(RemoteSession):
    """A persistent SSH connection to a remote Node.

    The connection is implemented with
    `the Fabric Python library <https://docs.fabfile.org/en/latest/>`_.

    Attributes:
        session: The underlying Fabric SSH connection.

    Raises:
        SSHConnectionError: The connection cannot be established.
    """

    session: Connection

    def _connect(self) -> None:
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

    def is_alive(self) -> bool:
        """Overrides :meth:`~.remote_session.RemoteSession.is_alive`."""
        return self.session.is_connected

    def _send_command(self, command: str, timeout: float, env: dict | None) -> CommandResult:
        """Send a command and return the result of the execution.

        Args:
            command: The command to execute.
            timeout: Wait at most this long in seconds for the command execution to complete.
            env: Extra environment variables that will be used in command execution.

        Raises:
            SSHSessionDeadError: The session died while executing the command.
            SSHTimeoutError: The command execution timed out.
        """
        try:
            output = self.session.run(command, env=env, warn=True, hide=True, timeout=timeout)

        except (UnexpectedExit, ThreadException) as e:
            self._logger.exception(e)
            raise SSHSessionDeadError(self.hostname) from e

        except CommandTimedOut as e:
            self._logger.exception(e)
            raise SSHTimeoutError(command) from e

        return CommandResult(self.name, command, output.stdout, output.stderr, output.return_code)

    def copy_from(
        self,
        source_file: str | PurePath,
        destination_file: str | PurePath,
    ) -> None:
        """Overrides :meth:`~.remote_session.RemoteSession.copy_from`."""
        self.session.get(str(destination_file), str(source_file))

    def copy_to(
        self,
        source_file: str | PurePath,
        destination_file: str | PurePath,
    ) -> None:
        """Overrides :meth:`~.remote_session.RemoteSession.copy_to`."""
        self.session.put(str(source_file), str(destination_file))

    def _close(self, force: bool = False) -> None:
        self.session.close()
