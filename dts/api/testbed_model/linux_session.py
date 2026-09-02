# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2026 University of New Hampshire

"""Linux OS translator.

Translate OS-unaware calls into Linux calls/utilities. Most of Linux distributions are mostly
compliant with POSIX standards, so this module only implements the parts that aren't.
This intermediate module implements the common parts of mostly POSIX compliant distributions.
"""

from abc import ABC, abstractmethod
from pathlib import PurePath


class LinuxSession(ABC):
    """The implementation of non-Posix compliant parts of Linux."""

    @abstractmethod
    def set_interface_link_up(self, name: str) -> None:
        """Set the link status of an interface to up.

        Args:
            name: the name of the interface
        """

    @abstractmethod
    def delete_interface(self, name: str) -> None:
        """Delete a virtual interface.

        Args:
            name: the name of the interface to delete.
        """

    @property
    @abstractmethod
    def devbind_script_path(self) -> PurePath:
        """The path to the dpdk-devbind.py script on the node."""

    @devbind_script_path.setter
    @abstractmethod
    def devbind_script_path(self, value: PurePath) -> None:
        """The path to the dpdk-devbind.py script on the node."""
