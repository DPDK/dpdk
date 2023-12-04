# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 University of New Hampshire

"""Smoke test suite.

Smoke tests are a class of tests which are used for validating a minimal set of important features.
These are the most important features without which (or when they're faulty) the software wouldn't
work properly. Thus, if any failure occurs while testing these features,
there isn't that much of a reason to continue testing, as the software is fundamentally broken.

These tests don't have to include only DPDK tests, as the reason for failures could be
in the infrastructure (a faulty link between NICs or a misconfiguration).
"""

import re

from framework.config import PortConfig
from framework.remote_session import TestPmdShell
from framework.settings import SETTINGS
from framework.test_suite import TestSuite
from framework.utils import REGEX_FOR_PCI_ADDRESS


class SmokeTests(TestSuite):
    """DPDK and infrastructure smoke test suite.

    The test cases validate the most basic DPDK functionality needed for all other test suites.
    The infrastructure also needs to be tested, as that is also used by all other test suites.

    Attributes:
        is_blocking: This test suite will block the execution of all other test suites
            in the build target after it.
        nics_in_node: The NICs present on the SUT node.
    """

    is_blocking = True
    # dicts in this list are expected to have two keys:
    # "pci_address" and "current_driver"
    nics_in_node: list[PortConfig] = []

    def set_up_suite(self) -> None:
        """Set up the test suite.

        Setup:
            Set the build directory path and a list of NICs in the SUT node.
        """
        self.dpdk_build_dir_path = self.sut_node.remote_dpdk_build_dir
        self.nics_in_node = self.sut_node.config.ports

    def test_unit_tests(self) -> None:
        """DPDK meson ``fast-tests`` unit tests.

        Test that all unit test from the ``fast-tests`` suite pass.
        The suite is a subset with only the most basic tests.

        Test:
            Run the ``fast-tests`` unit test suite through meson.
        """
        self.sut_node.main_session.send_command(
            f"meson test -C {self.dpdk_build_dir_path} --suite fast-tests -t 60",
            480,
            verify=True,
            privileged=True,
        )

    def test_driver_tests(self) -> None:
        """DPDK meson ``driver-tests`` unit tests.

        Test that all unit test from the ``driver-tests`` suite pass.
        The suite is a subset with driver tests. This suite may be run with virtual devices
        configured in the test run configuration.

        Test:
            Run the ``driver-tests`` unit test suite through meson.
        """
        vdev_args = ""
        for dev in self.sut_node.virtual_devices:
            vdev_args += f"--vdev {dev} "
        vdev_args = vdev_args[:-1]
        driver_tests_command = f"meson test -C {self.dpdk_build_dir_path} --suite driver-tests"
        if vdev_args:
            self._logger.info(
                f"Running driver tests with the following virtual devices: {vdev_args}"
            )
            driver_tests_command += f' --test-args "{vdev_args}"'

        self.sut_node.main_session.send_command(
            driver_tests_command,
            300,
            verify=True,
            privileged=True,
        )

    def test_devices_listed_in_testpmd(self) -> None:
        """Testpmd device discovery.

        Test that the devices configured in the test run configuration are found in testpmd.

        Test:
            List all devices found in testpmd and verify the configured devices are among them.
        """
        testpmd_driver = self.sut_node.create_interactive_shell(TestPmdShell, privileged=True)
        dev_list = [str(x) for x in testpmd_driver.get_devices()]
        for nic in self.nics_in_node:
            self.verify(
                nic.pci in dev_list,
                f"Device {nic.pci} was not listed in testpmd's available devices, "
                "please check your configuration",
            )

    def test_device_bound_to_driver(self) -> None:
        """Device driver in OS.

        Test that the devices configured in the test run configuration are bound to
        the proper driver.

        Test:
            List all devices with the ``dpdk-devbind.py`` script and verify that
            the configured devices are bound to the proper driver.
        """
        path_to_devbind = self.sut_node.path_to_devbind_script

        all_nics_in_dpdk_devbind = self.sut_node.main_session.send_command(
            f"{path_to_devbind} --status | awk '{REGEX_FOR_PCI_ADDRESS}'",
            SETTINGS.timeout,
        ).stdout

        for nic in self.nics_in_node:
            # This regular expression finds the line in the above string that starts
            # with the address for the nic we are on in the loop and then captures the
            # name of the driver in a group
            devbind_info_for_nic = re.search(
                f"{nic.pci}[^\\n]*drv=([\\d\\w]*) [^\\n]*",
                all_nics_in_dpdk_devbind,
            )
            self.verify(
                devbind_info_for_nic is not None,
                f"Failed to find configured device ({nic.pci}) using dpdk-devbind.py",
            )
            # We know this isn't None, but mypy doesn't
            assert devbind_info_for_nic is not None
            self.verify(
                devbind_info_for_nic.group(1) == nic.os_driver_for_dpdk,
                f"Driver for device {nic.pci} does not match driver listed in "
                f"configuration (bound to {devbind_info_for_nic.group(1)})",
            )
