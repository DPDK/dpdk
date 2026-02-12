# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 PANTHEON.tech s.r.o.

"""Python interactive shell.

Typical usage example in a TestSuite::

    from framework.remote_session import PythonShell

    python_shell = PythonShell(self.tg_node, timeout=5, privileged=True)
    python_shell.send_command("print('Hello World')")
    python_shell.close()
"""

from pathlib import PurePath
from typing import ClassVar

from .interactive_shell import InteractiveShell, only_active


class PythonShell(InteractiveShell):
    """Python interactive shell."""

    #: Python's prompt.
    _default_prompt: ClassVar[str] = ">>>"

    #: This forces the prompt to appear after sending a command.
    _command_extra_chars: ClassVar[str] = "\n"

    @property
    def path(self) -> PurePath:
        """Path to the Python3 executable."""
        return PurePath("python3")

    @only_active
    def close(self) -> None:
        """Close Python shell."""
        return super().close()
