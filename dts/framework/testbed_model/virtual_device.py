# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 PANTHEON.tech s.r.o.

"""Virtual devices model.

Alongside support for physical hardware, DPDK can create various virtual devices.
"""


class VirtualDevice(object):
    """Base class for virtual devices used by DPDK.

    Attributes:
        name: The name of the virtual device.
    """

    name: str

    def __init__(self, name: str):
        """Initialize the virtual device.

        Args:
            name: The name of the virtual device.
        """
        self.name = name

    def __str__(self) -> str:
        """This corresponds to the name used for DPDK devices."""
        return self.name
