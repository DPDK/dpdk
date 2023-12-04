# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 PANTHEON.tech s.r.o.

from pathlib import PurePath

from .interactive_shell import InteractiveShell


class PythonShell(InteractiveShell):
    _default_prompt: str = ">>>"
    _command_extra_chars: str = "\n"
    path: PurePath = PurePath("python3")
