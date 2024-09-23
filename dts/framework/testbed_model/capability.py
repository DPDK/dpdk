# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 PANTHEON.tech s.r.o.

"""Testbed capabilities.

This module provides a protocol that defines the common attributes of test cases and suites.
"""

from collections.abc import Sequence
from typing import ClassVar, Protocol


class TestProtocol(Protocol):
    """Common test suite and test case attributes."""

    #: Whether to skip the test case or suite.
    skip: ClassVar[bool] = False
    #: The reason for skipping the test case or suite.
    skip_reason: ClassVar[str] = ""

    @classmethod
    def get_test_cases(cls, test_case_sublist: Sequence[str] | None = None) -> tuple[set, set]:
        """Get test cases. Should be implemented by subclasses containing test cases.

        Raises:
            NotImplementedError: The subclass does not implement the method.
        """
        raise NotImplementedError()
