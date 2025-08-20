# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 University of New Hampshire
# Copyright(c) 2025 Arm Limited

"""DPDK Hello World test suite.

Starts and stops a testpmd session to verify EAL parameters
are properly configured.
"""

from framework.remote_session.testpmd_shell import TestPmdShell
from framework.test_suite import BaseConfig, TestSuite, func_test


class Config(BaseConfig):
    """Example custom configuration."""

    #: The hello world message to print.
    msg: str = "Hello World!"


class TestHelloWorld(TestSuite):
    """Hello World test suite. One test case, which starts and stops a testpmd session."""

    config: Config

    @func_test
    def hello_world(self) -> None:
        """EAL confidence test.

        Steps:
            Start testpmd session and check status.
        Verify:
            The testpmd session throws no errors.
        """
        with TestPmdShell() as testpmd:
            testpmd.start()
        self.log(self.config.msg)
