# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 PANTHEON.tech s.r.o.

"""Python interactive shell.

Typical usage example in a TestSuite::

    from framework.remote_session import PythonShell
    python_shell = self.tg_node.create_interactive_shell(
        PythonShell, timeout=5, privileged=True
    )
    python_shell.send_command("print('Hello World')")
    python_shell.close()
"""

from pathlib import PurePath
from typing import ClassVar

from .interactive_shell import InteractiveShell


class PythonShell(InteractiveShell):
    """Python interactive shell."""

    #: Python's prompt.
    _default_prompt: ClassVar[str] = ">>>"

    #: This forces the prompt to appear after sending a command.
    _command_extra_chars: ClassVar[str] = "\n"

    #: The Python executable.
    path: ClassVar[PurePath] = PurePath("python3")
