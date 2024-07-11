# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 University of New Hampshire
# Copyright(c) 2024 Arm Limited

"""Interactive shell with manual stop/start functionality.

Provides a class that doesn't require being started/stopped using a context manager and can instead
be started and stopped manually, or have the stopping process be handled at the time of garbage
collection.
"""

import weakref
from typing import ClassVar

from .single_active_interactive_shell import SingleActiveInteractiveShell


class InteractiveShell(SingleActiveInteractiveShell):
    """Adds manual start and stop functionality to interactive shells.

    Like its super-class, this class should not be instantiated directly and should instead be
    extended. This class also provides an option for automated cleanup of the application using a
    weakref and a finalize class. This finalize class allows for cleanup of the class at the time
    of garbage collection and also ensures that cleanup only happens once. This way if a user
    initiates the closing of the shell manually it is not repeated at the time of garbage
    collection.
    """

    _finalizer: weakref.finalize
    #: One attempt should be enough for shells which don't have to worry about other instances
    #: closing before starting a new one.
    _init_attempts: ClassVar[int] = 1

    def start_application(self) -> None:
        """Start the application.

        After the application has started, use :class:`weakref.finalize` to manage cleanup.
        """
        self._start_application()
        self._finalizer = weakref.finalize(self, self._close)

    def close(self) -> None:
        """Free all resources using :class:`weakref.finalize`."""
        self._finalizer()
