# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 University of New Hampshire

import re

from framework.config import PortConfig
from framework.remote_session import TestPmdShell
from framework.settings import SETTINGS
from framework.test_suite import TestSuite
from framework.utils import REGEX_FOR_PCI_ADDRESS


class SmokeTests(TestSuite):
    is_blocking = True
    # dicts in this list are expected to have two keys:
    # "pci_address" and "current_driver"
    nics_in_node: list[PortConfig] = []

    def set_up_suite(self) -> None:
        """
        Setup:
            Set the build directory path and generate a list of NICs in the SUT node.
        """
        self.dpdk_build_dir_path = self.sut_node.remote_dpdk_build_dir
        self.nics_in_node = self.sut_node.config.ports

    def test_unit_tests(self) -> None:
        """
        Test:
            Run the fast-test unit-test suite through meson.
        """
        self.sut_node.main_session.send_command(
            f"meson test -C {self.dpdk_build_dir_path} --suite fast-tests -t 60",
            480,
            verify=True,
            privileged=True,
        )

    def test_driver_tests(self) -> None:
        """
        Test:
            Run the driver-test unit-test suite through meson.
        """
        vdev_args = ""
        for dev in self.sut_node.virtual_devices:
            vdev_args += f"--vdev {dev} "
        vdev_args = vdev_args[:-1]
        driver_tests_command = (
            f"meson test -C {self.dpdk_build_dir_path} --suite driver-tests"
        )
        if vdev_args:
            self._logger.info(
                "Running driver tests with the following virtual "
                f"devices: {vdev_args}"
            )
            driver_tests_command += f' --test-args "{vdev_args}"'

        self.sut_node.main_session.send_command(
            driver_tests_command,
            300,
            verify=True,
            privileged=True,
        )

    def test_devices_listed_in_testpmd(self) -> None:
        """
        Test:
            Uses testpmd driver to verify that devices have been found by testpmd.
        """
        testpmd_driver = self.sut_node.create_interactive_shell(
            TestPmdShell, privileged=True
        )
        dev_list = [str(x) for x in testpmd_driver.get_devices()]
        for nic in self.nics_in_node:
            self.verify(
                nic.pci in dev_list,
                f"Device {nic.pci} was not listed in testpmd's available devices, "
                "please check your configuration",
            )

    def test_device_bound_to_driver(self) -> None:
        """
        Test:
            Ensure that all drivers listed in the config are bound to the correct
            driver.
        """
        path_to_devbind = self.sut_node.main_session.join_remote_path(
            self.sut_node._remote_dpdk_dir, "usertools", "dpdk-devbind.py"
        )

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
                devbind_info_for_nic.group(1) == nic.os_driver,
                f"Driver for device {nic.pci} does not match driver listed in "
                f"configuration (bound to {devbind_info_for_nic.group(1)})",
            )
