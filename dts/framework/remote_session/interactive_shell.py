# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 University of New Hampshire
# Copyright(c) 2024 Arm Limited

"""Common functionality for interactive shell handling.

The base class, :class:`InteractiveShell`, is meant to be extended by subclasses that contain
functionality specific to that shell type. These subclasses will often modify things like
the prompt to expect or the arguments to pass into the application, but still utilize
the same method for sending a command and collecting output. How this output is handled however
is often application specific. If an application needs elevated privileges to start it is expected
that the method for gaining those privileges is provided when initializing the class.

The :option:`--timeout` command line argument and the :envvar:`DTS_TIMEOUT`
environment variable configure the timeout of getting the output from command execution.
"""

from abc import ABC
from pathlib import PurePath
from typing import ClassVar

from paramiko import Channel, channel  # type: ignore[import-untyped]

from framework.logger import DTSLogger
from framework.params import Params
from framework.settings import SETTINGS
from framework.testbed_model.node import Node


class InteractiveShell(ABC):
    """The base class for managing interactive shells.

    This class shouldn't be instantiated directly, but instead be extended. It contains
    methods for starting interactive shells as well as sending commands to these shells
    and collecting input until reaching a certain prompt. All interactive applications
    will use the same SSH connection, but each will create their own channel on that
    session.
    """

    _node: Node
    _stdin: channel.ChannelStdinFile
    _stdout: channel.ChannelFile
    _ssh_channel: Channel
    _logger: DTSLogger
    _timeout: float
    _app_params: Params
    _privileged: bool
    _real_path: PurePath

    #: Prompt to expect at the end of output when sending a command.
    #: This is often overridden by subclasses.
    _default_prompt: ClassVar[str] = ""

    #: Extra characters to add to the end of every command
    #: before sending them. This is often overridden by subclasses and is
    #: most commonly an additional newline character.
    _command_extra_chars: ClassVar[str] = ""

    #: Path to the executable to start the interactive application.
    path: ClassVar[PurePath]

    def __init__(
        self,
        node: Node,
        privileged: bool = False,
        timeout: float = SETTINGS.timeout,
        start_on_init: bool = True,
        app_params: Params = Params(),
    ) -> None:
        """Create an SSH channel during initialization.

        Args:
            node: The node on which to run start the interactive shell.
            privileged: Enables the shell to run as superuser.
            timeout: The timeout used for the SSH channel that is dedicated to this interactive
                shell. This timeout is for collecting output, so if reading from the buffer
                and no output is gathered within the timeout, an exception is thrown.
            start_on_init: Start interactive shell automatically after object initialisation.
            app_params: The command line parameters to be passed to the application on startup.
        """
        self._node = node
        self._logger = node._logger
        self._app_params = app_params
        self._privileged = privileged
        self._timeout = timeout
        # Ensure path is properly formatted for the host
        self._update_real_path(self.path)

        if start_on_init:
            self.start_application()

    def _setup_ssh_channel(self):
        self._ssh_channel = self._node.main_session.interactive_session.session.invoke_shell()
        self._stdin = self._ssh_channel.makefile_stdin("w")
        self._stdout = self._ssh_channel.makefile("r")
        self._ssh_channel.settimeout(self._timeout)
        self._ssh_channel.set_combine_stderr(True)  # combines stdout and stderr streams

    def _make_start_command(self) -> str:
        """Makes the command that starts the interactive shell."""
        start_command = f"{self._real_path} {self._app_params or ''}"
        if self._privileged:
            start_command = self._node.main_session._get_privileged_command(start_command)
        return start_command

    def start_application(self) -> None:
        """Starts a new interactive application based on the path to the app.

        This method is often overridden by subclasses as their process for
        starting may look different.
        """
        self._setup_ssh_channel()
        self.send_command(self._make_start_command())

    def send_command(
        self, command: str, prompt: str | None = None, skip_first_line: bool = False
    ) -> str:
        """Send `command` and get all output before the expected ending string.

        Lines that expect input are not included in the stdout buffer, so they cannot
        be used for expect.

        Example:
            If you were prompted to log into something with a username and password,
            you cannot expect ``username:`` because it won't yet be in the stdout buffer.
            A workaround for this could be consuming an extra newline character to force
            the current `prompt` into the stdout buffer.

        Args:
            command: The command to send.
            prompt: After sending the command, `send_command` will be expecting this string.
                If :data:`None`, will use the class's default prompt.
            skip_first_line: Skip the first line when capturing the output.

        Returns:
            All output in the buffer before expected string.
        """
        self._logger.info(f"Sending: '{command}'")
        if prompt is None:
            prompt = self._default_prompt
        self._stdin.write(f"{command}{self._command_extra_chars}\n")
        self._stdin.flush()
        out: str = ""
        for line in self._stdout:
            if skip_first_line:
                skip_first_line = False
                continue
            if prompt in line and not line.rstrip().endswith(
                command.rstrip()
            ):  # ignore line that sent command
                break
            out += line
        self._logger.debug(f"Got output: {out}")
        return out

    def close(self) -> None:
        """Properly free all resources."""
        self._stdin.close()
        self._ssh_channel.close()

    def __del__(self) -> None:
        """Make sure the session is properly closed before deleting the object."""
        self.close()

    def _update_real_path(self, path: PurePath) -> None:
        """Updates the interactive shell's real path used at command line."""
        self._real_path = self._node.main_session.join_remote_path(path)
