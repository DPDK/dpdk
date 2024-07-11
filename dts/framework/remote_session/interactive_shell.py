# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 University of New Hampshire
# Copyright(c) 2024 Arm Limited

"""Interactive shell with manual stop/start functionality.

Provides a class that doesn't require being started/stopped using a context manager and can instead
be started and stopped manually, or have the stopping process be handled at the time of garbage
collection.
"""

from .single_active_interactive_shell import SingleActiveInteractiveShell


class InteractiveShell(SingleActiveInteractiveShell):
    """Adds manual start and stop functionality to interactive shells.

    Like its super-class, this class should not be instantiated directly and should instead be
    extended. This class also provides an option for automated cleanup of the application through
    the garbage collector.
    """

    def start_application(self) -> None:
        """Start the application."""
        self._start_application()

    def close(self) -> None:
        """Properly free all resources."""
        self._close()

    def __del__(self) -> None:
        """Make sure the session is properly closed before deleting the object."""
        self.close()
