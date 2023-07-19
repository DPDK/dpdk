# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 University of New Hampshire

from pathlib import PurePath
from typing import Callable

from .interactive_shell import InteractiveShell


class TestPmdDevice(object):
    pci_address: str

    def __init__(self, pci_address_line: str):
        self.pci_address = pci_address_line.strip().split(": ")[1].strip()

    def __str__(self) -> str:
        return self.pci_address


class TestPmdShell(InteractiveShell):
    path: PurePath = PurePath("app", "dpdk-testpmd")
    dpdk_app: bool = True
    _default_prompt: str = "testpmd>"
    _command_extra_chars: str = (
        "\n"  # We want to append an extra newline to every command
    )

    def _start_application(
        self, get_privileged_command: Callable[[str], str] | None
    ) -> None:
        """See "_start_application" in InteractiveShell."""
        self._app_args += " -- -i"
        super()._start_application(get_privileged_command)

    def get_devices(self) -> list[TestPmdDevice]:
        """Get a list of device names that are known to testpmd

        Uses the device info listed in testpmd and then parses the output to
        return only the names of the devices.

        Returns:
            A list of strings representing device names (e.g. 0000:14:00.1)
        """
        dev_info: str = self.send_command("show device info all")
        dev_list: list[TestPmdDevice] = []
        for line in dev_info.split("\n"):
            if "device name:" in line.lower():
                dev_list.append(TestPmdDevice(line))
        return dev_list
