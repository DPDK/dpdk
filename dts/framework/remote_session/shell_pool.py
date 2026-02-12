# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2025 Arm Limited

"""Module defining the shell pool class.

The shell pool is used by the test run state machine to control
the shells spawned by the test suites. Each layer of execution can
stash the current pool and create a new layer of shells by calling `start_new_pool`.
You can go back to the previous pool by calling `terminate_current_pool`. These layers are
identified in the pool as levels where the higher the number, the deeper the layer of execution.
As an example layers of execution could be: test run, test suite and test case.
Which could appropriately identified with level numbers 0, 1 and 2 respectively.

The shell pool layers are implemented as a stack. Therefore, when creating a new pool, this is
pushed on top of the stack. Similarly, terminating the current pool also means removing the one
at the top of the stack.
"""

from typing import TYPE_CHECKING

from framework.logger import DTSLogger, get_dts_logger

if TYPE_CHECKING:
    from framework.remote_session.interactive_shell import (
        InteractiveShell,
    )


class ShellPool:
    """A pool managing active shells."""

    _logger: DTSLogger
    _pools: list[set["InteractiveShell"]]

    def __init__(self) -> None:
        """Shell pool constructor."""
        self._logger = get_dts_logger("shell_pool")
        self._pools = [set()]

    @property
    def pool_level(self) -> int:
        """The current level of shell pool.

        The higher level, the deeper we are in the execution state.
        """
        return len(self._pools) - 1

    @property
    def _current_pool(self) -> set["InteractiveShell"]:
        """The pool in use for the current scope."""
        return self._pools[-1]

    def register_shell(self, shell: "InteractiveShell") -> None:
        """Register a new shell to the current pool."""
        self._logger.debug(f"Registering shell {shell} to pool level {self.pool_level}.")
        self._current_pool.add(shell)

    def unregister_shell(self, shell: "InteractiveShell") -> None:
        """Unregister a shell from any pool."""
        for level, pool in enumerate(self._pools):
            try:
                pool.remove(shell)
                if pool == self._current_pool:
                    self._logger.debug(
                        f"Unregistering shell {shell} from pool level {self.pool_level}."
                    )
                else:
                    self._logger.debug(
                        f"Unregistering shell {shell} from pool level {level}, "
                        f"but we currently are in level {self.pool_level}. Is this expected?"
                    )
            except KeyError:
                pass

    def start_new_pool(self) -> None:
        """Start a new shell pool."""
        self._logger.debug(f"Starting new shell pool and advancing to level {self.pool_level+1}.")
        self._pools.append(set())

    def terminate_current_pool(self) -> None:
        """Terminate all the shells in the current pool, and restore the previous pool if any.

        If any failure occurs while closing any shell, this is tolerated allowing the termination
        to continue until the current pool is empty and removed. But this function will re-raise the
        last occurred exception back to the caller.
        """
        occurred_exception = None
        current_pool_level = self.pool_level
        self._logger.debug(f"Terminating shell pool level {current_pool_level}.")
        for shell in self._pools.pop():
            self._logger.debug(f"Closing shell {shell} in shell pool level {current_pool_level}.")
            try:
                shell.close()
            except Exception as e:
                self._logger.error(f"An exception has occurred while closing shell {shell}:")
                self._logger.exception(e)
                occurred_exception = e

        if current_pool_level == 0:
            self.start_new_pool()
        else:
            self._logger.debug(f"Restoring shell pool from level {self.pool_level}.")

        # Raise the last occurred exception again to let the system register a failure.
        if occurred_exception is not None:
            raise occurred_exception
