# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 University of New Hampshire

"""Common functionality for interactive shell handling.

This base class, InteractiveShell, is meant to be extended by other classes that
contain functionality specific to that shell type. These derived classes will often
modify things like the prompt to expect or the arguments to pass into the application,
but still utilize the same method for sending a command and collecting output. How
this output is handled however is often application specific. If an application needs
elevated privileges to start it is expected that the method for gaining those
privileges is provided when initializing the class.
"""

from abc import ABC
from pathlib import PurePath
from typing import Callable

from paramiko import Channel, SSHClient, channel  # type: ignore[import]

from framework.logger import DTSLOG
from framework.settings import SETTINGS


class InteractiveShell(ABC):
    """The base class for managing interactive shells.

    This class shouldn't be instantiated directly, but instead be extended. It contains
    methods for starting interactive shells as well as sending commands to these shells
    and collecting input until reaching a certain prompt. All interactive applications
    will use the same SSH connection, but each will create their own channel on that
    session.

    Arguments:
        interactive_session: The SSH session dedicated to interactive shells.
        logger: Logger used for displaying information in the console.
        get_privileged_command: Method for modifying a command to allow it to use
            elevated privileges. If this is None, the application will not be started
            with elevated privileges.
        app_args: Command line arguments to be passed to the application on startup.
        timeout: Timeout used for the SSH channel that is dedicated to this interactive
            shell. This timeout is for collecting output, so if reading from the buffer
            and no output is gathered within the timeout, an exception is thrown.

    Attributes
        _default_prompt: Prompt to expect at the end of output when sending a command.
            This is often overridden by derived classes.
        _command_extra_chars: Extra characters to add to the end of every command
            before sending them. This is often overridden by derived classes and is
            most commonly an additional newline character.
        path: Path to the executable to start the interactive application.
        dpdk_app: Whether this application is a DPDK app. If it is, the build
            directory for DPDK on the node will be prepended to the path to the
            executable.
    """

    _interactive_session: SSHClient
    _stdin: channel.ChannelStdinFile
    _stdout: channel.ChannelFile
    _ssh_channel: Channel
    _logger: DTSLOG
    _timeout: float
    _app_args: str
    _default_prompt: str = ""
    _command_extra_chars: str = ""
    path: PurePath
    dpdk_app: bool = False

    def __init__(
        self,
        interactive_session: SSHClient,
        logger: DTSLOG,
        get_privileged_command: Callable[[str], str] | None,
        app_args: str = "",
        timeout: float = SETTINGS.timeout,
    ) -> None:
        self._interactive_session = interactive_session
        self._ssh_channel = self._interactive_session.invoke_shell()
        self._stdin = self._ssh_channel.makefile_stdin("w")
        self._stdout = self._ssh_channel.makefile("r")
        self._ssh_channel.settimeout(timeout)
        self._ssh_channel.set_combine_stderr(True)  # combines stdout and stderr streams
        self._logger = logger
        self._timeout = timeout
        self._app_args = app_args
        self._start_application(get_privileged_command)

    def _start_application(self, get_privileged_command: Callable[[str], str] | None) -> None:
        """Starts a new interactive application based on the path to the app.

        This method is often overridden by subclasses as their process for
        starting may look different.
        """
        start_command = f"{self.path} {self._app_args}"
        if get_privileged_command is not None:
            start_command = get_privileged_command(start_command)
        self.send_command(start_command)

    def send_command(self, command: str, prompt: str | None = None) -> str:
        """Send a command and get all output before the expected ending string.

        Lines that expect input are not included in the stdout buffer, so they cannot
        be used for expect. For example, if you were prompted to log into something
        with a username and password, you cannot expect "username:" because it won't
        yet be in the stdout buffer. A workaround for this could be consuming an
        extra newline character to force the current prompt into the stdout buffer.

        Returns:
            All output in the buffer before expected string
        """
        self._logger.info(f"Sending: '{command}'")
        if prompt is None:
            prompt = self._default_prompt
        self._stdin.write(f"{command}{self._command_extra_chars}\n")
        self._stdin.flush()
        out: str = ""
        for line in self._stdout:
            out += line
            if prompt in line and not line.rstrip().endswith(
                command.rstrip()
            ):  # ignore line that sent command
                break
        self._logger.debug(f"Got output: {out}")
        return out

    def close(self) -> None:
        self._stdin.close()
        self._ssh_channel.close()

    def __del__(self) -> None:
        self.close()
