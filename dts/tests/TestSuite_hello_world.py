# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 University of New Hampshire

"""DPDK Hello World test suite.

Starts and stops a testpmd session to verify EAL parameters
are properly configured.
"""

from framework.remote_session.testpmd_shell import TestPmdShell
from framework.test_suite import TestSuite, func_test


class TestHelloWorld(TestSuite):
    """Hello World test suite. One test case, which starts and stops a testpmd session."""

    @func_test
    def test_hello_world(self) -> None:
        """EAL confidence test.

        Steps:
            Start testpmd session and check status.
        Verify:
            The testpmd session throws no errors.
        """
        with TestPmdShell() as testpmd:
            testpmd.start()
        self.log("Hello World!")
